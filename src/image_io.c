/**
 * Image I/O functions for text detection library
 * Extracted from main.c for library use
 */

#include "text_detector.h"
#include "bounding_boxes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include <setjmp.h>

#define MODEL_HEIGHT TEXT_DETECTOR_INPUT_HEIGHT
#define MODEL_WIDTH TEXT_DETECTOR_INPUT_WIDTH

/**
 * Load PNG image and convert to RGB float array normalized to [0, 1]
 */
int load_png(const char* filename, float rgb[MODEL_HEIGHT][MODEL_WIDTH][3]) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Could not open file %s\n", filename);
        return 1;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        return 1;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        return 1;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 1;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    int width = png_get_image_width(png, info);
    int height = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    /* Convert to RGBA 8-bit if needed */
    if (bit_depth == 16) {
        png_set_strip_16(png);
    }
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png);
    }
    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png);
    }

    /* Ensure RGB format */
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }
    if (color_type == PNG_COLOR_TYPE_RGBA) {
        png_set_strip_alpha(png);
    }

    png_read_update_info(png, info);

    /* Allocate row pointers */
    png_bytep* row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
    for (int y = 0; y < height; y++) {
        row_pointers[y] = (png_bytep)malloc(png_get_rowbytes(png, info));
    }

    png_read_image(png, row_pointers);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);

    /* Convert to model input format using letterboxing (preserve aspect ratio) */
    float scale_x = (float)MODEL_WIDTH / (float)width;
    float scale_y = (float)MODEL_HEIGHT / (float)height;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;  /* Use smaller scale to fit */

    int scaled_width = (int)(width * scale);
    int scaled_height = (int)(height * scale);

    /* Calculate padding to center the image */
    int offset_x = (MODEL_WIDTH - scaled_width) / 2;
    int offset_y = (MODEL_HEIGHT - scaled_height) / 2;

    /* Fill with black (zero) first */
    for (int y = 0; y < MODEL_HEIGHT; y++) {
        for (int x = 0; x < MODEL_WIDTH; x++) {
            rgb[y][x][0] = 0.0f;
            rgb[y][x][1] = 0.0f;
            rgb[y][x][2] = 0.0f;
        }
    }

    /* Copy scaled image to center */
    for (int y = 0; y < scaled_height; y++) {
        for (int x = 0; x < scaled_width; x++) {
            /* Map to source coordinates (nearest neighbor) */
            int src_y = (y * height) / scaled_height;
            int src_x = (x * width) / scaled_width;

            /* Destination coordinates with offset */
            int dst_y = y + offset_y;
            int dst_x = x + offset_x;

            png_bytep row = row_pointers[src_y];
            png_bytep px = &(row[src_x * 3]);

            /* Convert to float [0, 1] and store as RGB */
            rgb[dst_y][dst_x][0] = px[0] / 255.0f;
            rgb[dst_y][dst_x][1] = px[1] / 255.0f;
            rgb[dst_y][dst_x][2] = px[2] / 255.0f;
        }
    }

    /* Free row pointers */
    for (int y = 0; y < height; y++) {
        free(row_pointers[y]);
    }
    free(row_pointers);

    return 0;
}

/**
 * Save original image with bounding boxes drawn on it.
 * Boxes are drawn as red rectangles.
 * Adjusts for letterboxing offset to map boxes from model space to original image space.
 */
int save_annotated_image(const char* filename, const char* original_image,
                         const BoundingBoxList* boxes) {
    FILE* fp = fopen(original_image, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Could not open file %s\n", original_image);
        return 1;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        return 1;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        return 1;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 1;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    int orig_width = png_get_image_width(png, info);
    int orig_height = png_get_image_height(png, info);

    /* Calculate the same letterboxing offset that was used during inference */
    float scale_x = (float)MODEL_WIDTH / (float)orig_width;
    float scale_y = (float)MODEL_HEIGHT / (float)orig_height;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;

    int scaled_width = (int)(orig_width * scale);
    int scaled_height = (int)(orig_height * scale);
    int offset_x = (MODEL_WIDTH - scaled_width) / 2;
    int offset_y = (MODEL_HEIGHT - scaled_height) / 2;
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    /* Convert to RGB if needed */
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

    /* Allocate row pointers */
    png_bytep* row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * orig_height);
    for (int y = 0; y < orig_height; y++) {
        row_pointers[y] = (png_bytep)malloc(png_get_rowbytes(png, info));
    }

    png_read_image(png, row_pointers);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);

    /* Draw bounding boxes (red) */
    for (int i = 0; i < boxes->count; i++) {
        const BoundingBox* b = &boxes->boxes[i];

        /* Adjust box coordinates from letterboxed model space to original image space */
        int x1 = b->x - offset_x;
        int y1 = b->y - offset_y;
        int x2 = (b->x + b->width) - offset_x;
        int y2 = (b->y + b->height) - offset_y;

        /* Clamp to image bounds */
        if (x1 < 0) x1 = 0;
        if (y1 < 0) y1 = 0;
        if (x2 >= orig_width) x2 = orig_width - 1;
        if (y2 >= orig_height) y2 = orig_height - 1;

        /* Draw top and bottom lines */
        for (int x = x1; x <= x2; x++) {
            if (y1 >= 0 && y1 < orig_height && x >= 0 && x < orig_width) {
                row_pointers[y1][x * 3 + 0] = 255;  /* R */
                row_pointers[y1][x * 3 + 1] = 0;    /* G */
                row_pointers[y1][x * 3 + 2] = 0;    /* B */
            }
            if (y2 >= 0 && y2 < orig_height && x >= 0 && x < orig_width) {
                row_pointers[y2][x * 3 + 0] = 255;  /* R */
                row_pointers[y2][x * 3 + 1] = 0;    /* G */
                row_pointers[y2][x * 3 + 2] = 0;    /* B */
            }
        }

        /* Draw left and right lines */
        for (int y = y1; y <= y2; y++) {
            if (y >= 0 && y < orig_height && x1 >= 0 && x1 < orig_width) {
                row_pointers[y][x1 * 3 + 0] = 255;  /* R */
                row_pointers[y][x1 * 3 + 1] = 0;    /* G */
                row_pointers[y][x1 * 3 + 2] = 0;    /* B */
            }
            if (y >= 0 && y < orig_height && x2 >= 0 && x2 < orig_width) {
                row_pointers[y][x2 * 3 + 0] = 255;  /* R */
                row_pointers[y][x2 * 3 + 1] = 0;    /* G */
                row_pointers[y][x2 * 3 + 2] = 0;    /* B */
            }
        }
    }

    /* Write the annotated image */
    FILE* out_fp = fopen(filename, "wb");
    if (!out_fp) {
        fprintf(stderr, "Error: Could not create file %s\n", filename);
        for (int y = 0; y < orig_height; y++) {
            free(row_pointers[y]);
        }
        free(row_pointers);
        return 1;
    }

    png_structp out_png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!out_png) {
        fclose(out_fp);
        for (int y = 0; y < orig_height; y++) {
            free(row_pointers[y]);
        }
        free(row_pointers);
        return 1;
    }

    png_infop out_info = png_create_info_struct(out_png);
    if (!out_info) {
        png_destroy_write_struct(&out_png, NULL);
        fclose(out_fp);
        for (int y = 0; y < orig_height; y++) {
            free(row_pointers[y]);
        }
        free(row_pointers);
        return 1;
    }

    if (setjmp(png_jmpbuf(out_png))) {
        png_destroy_write_struct(&out_png, &out_info);
        fclose(out_fp);
        for (int y = 0; y < orig_height; y++) {
            free(row_pointers[y]);
        }
        free(row_pointers);
        return 1;
    }

    png_init_io(out_png, out_fp);
    png_set_IHDR(out_png, out_info, orig_width, orig_height, 8, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(out_png, out_info);
    png_write_image(out_png, row_pointers);
    png_write_end(out_png, NULL);
    png_destroy_write_struct(&out_png, &out_info);
    fclose(out_fp);

    for (int y = 0; y < orig_height; y++) {
        free(row_pointers[y]);
    }
    free(row_pointers);

    return 0;
}
