/**
 * Text Detection Library
 *
 * A lightweight C library for detecting text regions in images using a
 * convolutional neural network model.
 */

#ifndef TEXT_DETECTOR_H
#define TEXT_DETECTOR_H

#include <stddef.h>

/* Include model configuration - dimensions are set at CMake configure time */
#include "text_detector_model_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Version information */
#define TEXT_DETECTOR_VERSION_MAJOR 1
#define TEXT_DETECTOR_VERSION_MINOR 0
#define TEXT_DETECTOR_VERSION_PATCH 0

/* Fixed model parameters */
#define TEXT_DETECTOR_INPUT_CHANNELS 3

/**
 * Bounding box representing a detected text region.
 */
typedef struct {
    int x;              /* Top-left corner X coordinate */
    int y;              /* Top-left corner Y coordinate */
    int width;          /* Width of the bounding box */
    int height;         /* Height of the bounding box */
    float confidence;   /* Average confidence score (0.0 to 1.0) */
    int pixel_count;    /* Number of pixels in this region */
} text_detector_box_t;

/**
 * Array of bounding boxes returned by detection.
 */
typedef struct {
    text_detector_box_t* boxes;
    size_t count;
    size_t capacity;
} text_detector_boxes_t;

/**
 * Detection result containing raw model output and extracted boxes.
 */
typedef struct {
    float output[1][1][TEXT_DETECTOR_OUTPUT_HEIGHT][TEXT_DETECTOR_OUTPUT_WIDTH];
    text_detector_boxes_t boxes;
} text_detector_result_t;

/**
 * Text box with transcribed text content.
 */
typedef struct {
    text_detector_box_t box;           /* Bounding box */
    char* text;                        /* Transcribed text (NULL-terminated, heap-allocated) */
    float text_confidence;             /* OCR confidence score (0.0 to 1.0) */
} text_detector_text_box_t;

/**
 * Array of text boxes with transcribed content.
 */
typedef struct {
    text_detector_text_box_t* boxes;
    size_t count;
    size_t capacity;
} text_detector_text_boxes_t;

/**
 * Initialize the text detector library.
 * Must be called before any other functions.
 *
 * Args:
 *   tessdata_path: Path to tessdata directory (NULL for default)
 *   language: Language code (e.g., "eng", "fra", "deu"), NULL defaults to "eng"
 *
 * Returns: 0 on success, -1 on error.
 */
int text_detector_init(const char* tessdata_path, const char* language);

/**
 * Cleanup library resources.
 * Call when done using the library.
 */
void text_detector_cleanup(void);

/**
 * Run text detection on an RGB image.
 *
 * Args:
 *   rgb: Input image data [height][width][3], RGB format, values in [0.0, 1.0].
 *        Must be TEXT_DETECTOR_INPUT_HEIGHT x TEXT_DETECTOR_INPUT_WIDTH.
 *   result: Output structure to store detection results.
 *
 * Returns: 0 on success, -1 on error.
 */
int text_detector_detect(const float rgb[TEXT_DETECTOR_INPUT_HEIGHT][TEXT_DETECTOR_INPUT_WIDTH][3],
                         text_detector_result_t* result);

/**
 * Create a new bounding boxes array.
 *
 * Returns: Initialized boxes structure, or NULL on error.
 */
text_detector_boxes_t* text_detector_boxes_create(size_t capacity);

/**
 * Free a bounding boxes array.
 */
void text_detector_boxes_free(text_detector_boxes_t* boxes);

/**
 * Extract text bounding boxes from model output.
 *
 * Args:
 *   output: Model output logits [1][1][64][64]
 *   boxes: Output list of bounding boxes (must be created with text_detector_boxes_create)
 *
 * Returns: Number of boxes found, or -1 on error.
 */
int text_detector_extract_boxes(const float output[1][1][TEXT_DETECTOR_OUTPUT_HEIGHT][TEXT_DETECTOR_OUTPUT_WIDTH],
                                 text_detector_boxes_t* boxes);

/**
 * Load a PNG image into RGB float array normalized to [0, 1].
 * Applies letterboxing to preserve aspect ratio.
 *
 * Args:
 *   filename: Path to PNG file
 *   rgb: Output array [height][width][3], must be TEXT_DETECTOR_INPUT_HEIGHT x TEXT_DETECTOR_INPUT_WIDTH
 *
 * Returns: 0 on success, -1 on error.
 */
int text_detector_load_png(const char* filename,
                           float rgb[TEXT_DETECTOR_INPUT_HEIGHT][TEXT_DETECTOR_INPUT_WIDTH][3]);

/**
 * Save image with bounding boxes drawn as red rectangles.
 *
 * Args:
 *   filename: Output PNG path
 *   original_image: Path to original input image (for reading)
 *   boxes: Bounding boxes to draw
 *
 * Returns: 0 on success, -1 on error.
 */
int text_detector_save_annotated(const char* filename, const char* original_image,
                                 const text_detector_boxes_t* boxes);

/**
 * Convert bounding boxes from cell coordinates to pixel coordinates.
 * Each cell = 4x4 pixels. Adds optional padding.
 *
 * Args:
 *   boxes: Bounding box list to convert
 *   padding: Pixels to expand each box by (on all sides)
 */
void text_detector_convert_to_pixels(text_detector_boxes_t* boxes, int padding);

/**
 * Save cropped text regions as separate PNG files.
 *
 * Args:
 *   original_image: Path to the original input image
 *   boxes: Bounding box list (in pixel coordinates)
 *   output_prefix: Prefix for output files (e.g., "crop" -> "crop_000.png")
 *
 * Returns: 0 on success, -1 on error.
 */
int text_detector_save_crops(const char* original_image,
                             const text_detector_boxes_t* boxes,
                             const char* output_prefix);

/**
 * Create a new text boxes array.
 *
 * Returns: Initialized text boxes structure, or NULL on error.
 */
text_detector_text_boxes_t* text_detector_text_boxes_create(size_t capacity);

/**
 * Free a text boxes array and release all text strings.
 */
void text_detector_text_boxes_free(text_detector_text_boxes_t* boxes);

/**
 * Run text detection and transcription in one step.
 *
 * Args:
 *   original_image: Path to the input image
 *   rgb: Loaded image data [height][width][3] (from text_detector_load_png)
 *   text_boxes: Output text boxes with transcriptions (must be pre-allocated)
 *   padding: Pixels to expand each box by (default 3)
 *
 * Returns: Number of transcriptions, or -1 on error.
 */
int text_detector_detect_and_transcribe(const char* original_image,
                                        const float rgb[TEXT_DETECTOR_INPUT_HEIGHT][TEXT_DETECTOR_INPUT_WIDTH][3],
                                        text_detector_text_boxes_t* text_boxes,
                                        int padding);

#ifdef __cplusplus
}
#endif

#endif /* TEXT_DETECTOR_H */
