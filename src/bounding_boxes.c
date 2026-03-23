/**
 * Bounding Box Extraction Implementation
 */

#include "bounding_boxes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <png.h>
#include <setjmp.h>

// Forward declaration for MODEL_WIDTH/MODEL_HEIGHT from main.c
#define MODEL_WIDTH 256
#define MODEL_HEIGHT 256

#define OUTPUT_WIDTH 64
#define OUTPUT_HEIGHT 64

/**
 * Apply sigmoid activation to convert logits to probabilities.
 */
static float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

/**
 * Calculate vertical overlap percentage between two boxes.
 * Returns the overlap as a percentage of the shorter box's height.
 */
static float vertical_overlap_pct(const BoundingBox* a, const BoundingBox* b) {
    int a_bottom = a->y + a->height;
    int b_bottom = b->y + b->height;

    // Calculate intersection
    int overlap_top = (a->y > b->y) ? a->y : b->y;
    int overlap_bottom = (a_bottom < b_bottom) ? a_bottom : b_bottom;

    if (overlap_bottom <= overlap_top) {
        return 0.0f;  // No overlap
    }

    int overlap_height = overlap_bottom - overlap_top;
    int min_height = (a->height < b->height) ? a->height : b->height;

    return (float)overlap_height / (float)min_height;
}

/**
 * Calculate horizontal gap between two boxes.
 * Positive values indicate a gap, negative values indicate overlap.
 */
static int horizontal_gap(const BoundingBox* a, const BoundingBox* b) {
    int a_right = a->x + a->width;
    int b_right = b->x + b->width;

    // Gap from a to b, or b to a (take the one that means they're closest)
    int gap1 = b->x - a_right;
    int gap2 = a->x - b_right;

    // Return the minimum gap (if both are positive, boxes are separated)
    // If one is negative, boxes overlap, so return that negative value
    if (gap1 < 0 || gap2 < 0) {
        // Boxes overlap - return the "most negative" (largest overlap)
        return (gap1 < gap2) ? gap1 : gap2;
    } else {
        // Boxes don't overlap - return the smaller gap
        return (gap1 < gap2) ? gap1 : gap2;
    }
}

/**
 * Merge two boxes into one.
 * The merged box contains both original boxes.
 */
static BoundingBox merge_boxes(const BoundingBox* a, const BoundingBox* b) {
    BoundingBox merged;
    int a_right = a->x + a->width;
    int a_bottom = a->y + a->height;
    int b_right = b->x + b->width;
    int b_bottom = b->y + b->height;

    // Union of both boxes
    merged.x = (a->x < b->x) ? a->x : b->x;
    merged.y = (a->y < b->y) ? a->y : b->y;

    int max_right = (a_right > b_right) ? a_right : b_right;
    int max_bottom = (a_bottom > b_bottom) ? a_bottom : b_bottom;

    merged.width = max_right - merged.x;
    merged.height = max_bottom - merged.y;

    // Weighted average of confidences by pixel count
    float total_pixels = (float)(a->pixel_count + b->pixel_count);
    merged.confidence = (a->confidence * a->pixel_count + b->confidence * b->pixel_count) / total_pixels;
    merged.pixel_count = a->pixel_count + b->pixel_count;

    return merged;
}

/**
 * Stage 1 & 2: Connected Component Labeling using BFS
 * Finds connected regions in the binary mask and computes bounding boxes.
 */
static int find_connected_components(const float output[1][1][64][64], Component* components) {
    int visited[OUTPUT_HEIGHT][OUTPUT_WIDTH] = {0};
    int component_count = 0;
    int queue[OUTPUT_HEIGHT * OUTPUT_WIDTH][2];  // BFS queue: [y, x]

    for (int y = 0; y < OUTPUT_HEIGHT; y++) {
        for (int x = 0; x < OUTPUT_WIDTH; x++) {
            float prob = sigmoid(output[0][0][y][x]);

            // Check if this cell is above threshold and not visited
            if (prob >= PROB_THRESHOLD && !visited[y][x]) {
                // Start BFS for this component
                int queue_front = 0;
                int queue_back = 0;
                queue[queue_back][0] = y;
                queue[queue_back][1] = x;
                queue_back++;
                visited[y][x] = 1;

                // Initialize component
                Component comp = {
                    .min_x = x,
                    .max_x = x,
                    .min_y = y,
                    .max_y = y,
                    .confidence_sum = prob,
                    .pixel_count = 1
                };

                // BFS to find all connected cells
                while (queue_front < queue_back) {
                    int cy = queue[queue_front][0];
                    int cx = queue[queue_front][1];
                    queue_front++;

                    // Check 8-connected neighbors
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            if (dy == 0 && dx == 0) continue;

                            int ny = cy + dy;
                            int nx = cx + dx;

                            // Check bounds
                            if (ny >= 0 && ny < OUTPUT_HEIGHT && nx >= 0 && nx < OUTPUT_WIDTH) {
                                if (!visited[ny][nx]) {
                                    float nprob = sigmoid(output[0][0][ny][nx]);
                                    if (nprob >= PROB_THRESHOLD) {
                                        visited[ny][nx] = 1;
                                        queue[queue_back][0] = ny;
                                        queue[queue_back][1] = nx;
                                        queue_back++;

                                        // Update component bounds
                                        if (nx < comp.min_x) comp.min_x = nx;
                                        if (nx > comp.max_x) comp.max_x = nx;
                                        if (ny < comp.min_y) comp.min_y = ny;
                                        if (ny > comp.max_y) comp.max_y = ny;
                                        comp.confidence_sum += nprob;
                                        comp.pixel_count++;
                                    }
                                }
                            }
                        }
                    }
                }

                // Store component if we have space
                if (component_count < MAX_COMPONENTS) {
                    components[component_count++] = comp;
                } else {
                    fprintf(stderr, "Warning: Too many components, truncating\n");
                    return component_count;
                }
            }
        }
    }

    return component_count;
}

/**
 * Stage 3: Filter small components that are likely noise.
 */
static int filter_components(Component* components, int count) {
    int write_idx = 0;
    for (int i = 0; i < count; i++) {
        if (components[i].pixel_count >= MIN_PIXELS) {
            if (write_idx != i) {
                components[write_idx] = components[i];
            }
            write_idx++;
        }
    }
    return write_idx;
}

/**
 * Stage 4: Merge nearby boxes using text-aware criteria.
 *
 * Merge criteria:
 * - Vertical overlap > 85%
 * - Horizontal gap <= 5px
 */
static int merge_nearby_boxes(Component* components, int count, BoundingBox* boxes) {
    // Convert components to boxes
    int box_count = 0;
    for (int i = 0; i < count && box_count < MAX_COMPONENTS; i++) {
        boxes[box_count].x = components[i].min_x;
        boxes[box_count].y = components[i].min_y;
        boxes[box_count].width = components[i].max_x - components[i].min_x + 1;
        boxes[box_count].height = components[i].max_y - components[i].min_y + 1;
        boxes[box_count].confidence = components[i].confidence_sum / components[i].pixel_count;
        boxes[box_count].pixel_count = components[i].pixel_count;
        box_count++;
    }

    // Iteratively merge boxes until no more merges possible
    int merged;
    do {
        merged = 0;
        for (int i = 0; i < box_count && !merged; i++) {
            for (int j = i + 1; j < box_count && !merged; j++) {
                float v_overlap = vertical_overlap_pct(&boxes[i], &boxes[j]);
                int h_gap = horizontal_gap(&boxes[i], &boxes[j]);

                if (v_overlap > VERTICAL_OVERLAP_PCT && h_gap <= HORIZONTAL_GAP_MAX) {
                    // Merge boxes i and j
                    boxes[i] = merge_boxes(&boxes[i], &boxes[j]);

                    // Remove box j by shifting subsequent boxes
                    for (int k = j; k < box_count - 1; k++) {
                        boxes[k] = boxes[k + 1];
                    }
                    box_count--;
                    merged = 1;
                }
            }
        }
    } while (merged);

    return box_count;
}

/**
 * Main API: Extract text bounding boxes from model output.
 */
int extract_text_boxes(const float output[1][1][64][64], BoundingBoxList* boxes) {
    Component components[MAX_COMPONENTS];

    // Stage 1 & 2: Find connected components
    int count = find_connected_components(output, components);
    if (count == 0) {
        boxes->count = 0;
        return 0;
    }

    // Stage 3: Filter small components
    count = filter_components(components, count);
    if (count == 0) {
        boxes->count = 0;
        return 0;
    }

    // Stage 4: Merge nearby boxes
    boxes->count = merge_nearby_boxes(components, count, boxes->boxes);

    return boxes->count;
}

/**
 * Print bounding boxes in human-readable format.
 */
void print_bounding_boxes(const BoundingBoxList* boxes) {
    printf("Detected %d text region(s):\n", boxes->count);

    for (int i = 0; i < boxes->count; i++) {
        const BoundingBox* b = &boxes->boxes[i];
        printf("  Box %d: x=%d, y=%d, width=%d, height=%d, confidence=%.2f, pixels=%d\n",
               i, b->x, b->y, b->width, b->height, b->confidence, b->pixel_count);
    }
}

/**
 * Convert bounding boxes from cell coordinates to pixel coordinates.
 * Each cell = 4x4 pixels in original image.
 */
void convert_to_pixel_coords(BoundingBoxList* boxes, int padding) {
    const int scale = 4;
    for (int i = 0; i < boxes->count; i++) {
        BoundingBox* b = &boxes->boxes[i];
        b->x = b->x * scale - padding;
        b->y = b->y * scale - padding;
        b->width = b->width * scale + 2 * padding;
        b->height = b->height * scale + 2 * padding;
    }
}

/**
 * Save cropped text regions as separate PNG files.
 */
int save_cropped_regions(const char* original_image, const BoundingBoxList* boxes,
                         const char* output_prefix) {
    FILE* fp = fopen(original_image, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Could not open file %s\n", original_image);
        return -1;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        return -1;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        return -1;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return -1;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    int orig_width = png_get_image_width(png, info);
    int orig_height = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    // Convert to RGB if needed
    if (bit_depth == 16) {
        png_set_strip_16(png);
    }
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }
    if (color_type == PNG_COLOR_TYPE_RGBA) {
        png_set_strip_alpha(png);
    }

    png_read_update_info(png, info);

    // Allocate row pointers
    png_bytep* row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * orig_height);
    for (int y = 0; y < orig_height; y++) {
        row_pointers[y] = (png_bytep)malloc(png_get_rowbytes(png, info));
    }

    png_read_image(png, row_pointers);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);

    // Calculate letterboxing offset (same as in save_annotated_image)
    float scale_x = (float)MODEL_WIDTH / (float)orig_width;
    float scale_y = (float)MODEL_HEIGHT / (float)orig_height;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;
    int scaled_width = (int)(orig_width * scale);
    int scaled_height = (int)(orig_height * scale);
    int offset_x = (MODEL_WIDTH - scaled_width) / 2;
    int offset_y = (MODEL_HEIGHT - scaled_height) / 2;

    // Save each cropped region
    for (int i = 0; i < boxes->count; i++) {
        const BoundingBox* b = &boxes->boxes[i];

        // Adjust box coordinates from letterboxed model space to original image space
        int x1 = b->x - offset_x;
        int y1 = b->y - offset_y;
        int x2 = (b->x + b->width) - offset_x;
        int y2 = (b->y + b->height) - offset_y;

        // Clamp to image bounds
        if (x1 < 0) x1 = 0;
        if (y1 < 0) y1 = 0;
        if (x2 >= orig_width) x2 = orig_width - 1;
        if (y2 >= orig_height) y2 = orig_height - 1;

        int crop_width = x2 - x1 + 1;
        int crop_height = y2 - y1 + 1;

        if (crop_width <= 0 || crop_height <= 0) {
            fprintf(stderr, "Warning: Skipping invalid crop region for box %d\n", i);
            continue;
        }

        // Generate output filename
        char filename[256];
        snprintf(filename, sizeof(filename), "%s_%03d.png", output_prefix, i);

        // Write cropped image
        FILE* out_fp = fopen(filename, "wb");
        if (!out_fp) {
            fprintf(stderr, "Error: Could not create file %s\n", filename);
            continue;
        }

        png_structp out_png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!out_png) {
            fclose(out_fp);
            continue;
        }

        png_infop out_info = png_create_info_struct(out_png);
        if (!out_info) {
            png_destroy_write_struct(&out_png, NULL);
            fclose(out_fp);
            continue;
        }

        if (setjmp(png_jmpbuf(out_png))) {
            png_destroy_write_struct(&out_png, &out_info);
            fclose(out_fp);
            continue;
        }

        png_init_io(out_png, out_fp);
        png_set_IHDR(out_png, out_info, crop_width, crop_height, 8, PNG_COLOR_TYPE_RGB,
                     PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
        png_write_info(out_png, out_info);

        // Allocate row pointers for cropped image
        png_bytep* crop_row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * crop_height);
        for (int y = 0; y < crop_height; y++) {
            crop_row_pointers[y] = (png_bytep)malloc(3 * crop_width);
            // Copy pixels from original image
            for (int x = 0; x < crop_width; x++) {
                int src_x = x1 + x;
                int src_y = y1 + y;
                png_bytep src_row = row_pointers[src_y];
                crop_row_pointers[y][x * 3 + 0] = src_row[src_x * 3 + 0];  // R
                crop_row_pointers[y][x * 3 + 1] = src_row[src_x * 3 + 1];  // G
                crop_row_pointers[y][x * 3 + 2] = src_row[src_x * 3 + 2];  // B
            }
        }

        png_write_image(out_png, crop_row_pointers);
        png_write_end(out_png, NULL);
        png_destroy_write_struct(&out_png, &out_info);
        fclose(out_fp);

        // Free crop row pointers
        for (int y = 0; y < crop_height; y++) {
            free(crop_row_pointers[y]);
        }
        free(crop_row_pointers);

        printf("Saved cropped region to: %s (%dx%d)\n", filename, crop_width, crop_height);
    }

    // Free row pointers
    for (int y = 0; y < orig_height; y++) {
        free(row_pointers[y]);
    }
    free(row_pointers);

    return 0;
}
