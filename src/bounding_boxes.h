/**
 * Bounding Box Extraction for Text Detection
 *
 * Converts model output heatmap to a list of text bounding boxes using
 * connected component labeling and text-aware merging.
 */

#ifndef BOUNDING_BOXES_H
#define BOUNDING_BOXES_H

#define MAX_COMPONENTS 256
#define MIN_PIXELS 3           // Minimum pixels to consider as text
#define PROB_THRESHOLD 0.5f    // Sigmoid threshold for binary mask
#define VERTICAL_OVERLAP_PCT 0.85f  // 85% vertical overlap required
#define HORIZONTAL_GAP_MAX 5   // Maximum 5px horizontal gap allowed

/**
 * Bounding box in cell coordinates (64x64 grid)
 */
typedef struct {
    int x, y;           // Top-left corner
    int width, height;  // Dimensions
    float confidence;   // Average probability of all cells in this box
    int pixel_count;    // Number of cells in this component
} BoundingBox;

/**
 * List of bounding boxes
 */
typedef struct {
    BoundingBox boxes[MAX_COMPONENTS];
    int count;
} BoundingBoxList;

/**
 * Connected component representation
 */
typedef struct {
    int min_x, max_x;
    int min_y, max_y;
    float confidence_sum;
    int pixel_count;
} Component;

/**
 * Extract text bounding boxes from model output.
 *
 * Args:
 *   output: Model output logits [1][1][64][64]
 *   boxes: Output list of bounding boxes
 *
 * Returns:
 *   Number of boxes found, or -1 on error
 */
int extract_text_boxes(const float output[1][1][64][64], BoundingBoxList* boxes);

/**
 * Print bounding boxes in human-readable format.
 */
void print_bounding_boxes(const BoundingBoxList* boxes);

/**
 * Convert bounding boxes from cell coordinates to pixel coordinates.
 * Each cell = 4x4 pixels in original image.
 *
 * Args:
 *   boxes: Bounding box list to convert
 *   padding: Number of pixels to expand each box by (on all sides)
 */
void convert_to_pixel_coords(BoundingBoxList* boxes, int padding);

/**
 * Save cropped text regions as separate PNG files.
 *
 * Args:
 *   original_image: Path to the original input image
 *   boxes: Bounding box list (in pixel coordinates, already converted)
 *   output_prefix: Prefix for output files (e.g., "crop" -> "crop_000.png")
 *
 * Returns:
 *   0 on success, -1 on error
 */
int save_cropped_regions(const char* original_image, const BoundingBoxList* boxes,
                         const char* output_prefix);

#endif /* BOUNDING_BOXES_H */
