/**
 * Combined detection and transcription - in-memory version
 *
 * This file provides text_detector_detect_and_transcribe_memory(),
 * which works entirely with in-memory RGB data without requiring file I/O.
 */

#include "text_detector.h"
#include "bounding_boxes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Internal definition of text_detector_result_t for access to boxes field.
 * This matches the definition in text_detector.c.
 */
struct text_detector_result_t {
    float output[1][1][TEXT_DETECTOR_OUTPUT_HEIGHT][TEXT_DETECTOR_OUTPUT_WIDTH];
    text_detector_boxes_t boxes;
};

/* External functions */
extern int text_detector_detect(const float rgb[TEXT_DETECTOR_INPUT_HEIGHT][TEXT_DETECTOR_INPUT_WIDTH][3],
                                text_detector_result_t* result);
extern int ocr_transcribe_from_rgb(const float rgb[TEXT_DETECTOR_INPUT_HEIGHT][TEXT_DETECTOR_INPUT_WIDTH][3],
                                   const text_detector_boxes_t* boxes,
                                   text_detector_text_boxes_t* text_boxes);

/* Enable debug output */
static inline int debug_enabled(void) {
    return getenv("MCP_DEBUG") != NULL;
}

/**
 * Run text detection and transcription in one step (in-memory version).
 *
 * This is the in-memory equivalent of text_detector_detect_and_transcribe(),
 * but works entirely with RGB pixel data without requiring a file path.
 */
int text_detector_detect_and_transcribe_memory(
    const float rgb[TEXT_DETECTOR_INPUT_HEIGHT][TEXT_DETECTOR_INPUT_WIDTH][3],
    text_detector_text_boxes_t* text_boxes,
    int padding) {

    if (!rgb || !text_boxes) {
        return -1;
    }

    /* Step 1: Allocate and run detection */
    text_detector_result_t* result = text_detector_result_create(text_boxes->capacity);
    if (!result) {
        fprintf(stderr, "Error: Failed to allocate result structure\n");
        return -1;
    }

    int detect_result = text_detector_detect(rgb, result);

    if (detect_result != 0) {
        fprintf(stderr, "Error: Detection failed\n");
        text_detector_result_free(result);
        return -1;
    }

    /* Step 2: Convert boxes from cell coordinates to pixel coordinates */
    text_detector_convert_to_pixels(result, padding);

    if (debug_enabled()) {
        const text_detector_box_t* boxes = text_detector_result_get_boxes(result);
        size_t count = text_detector_result_get_count(result);
        fprintf(stderr, "[LIB] Detected %zu text regions:\n", count);
        for (size_t i = 0; i < count; i++) {
            fprintf(stderr, "[LIB]   [%zu] x=%d y=%d w=%d h=%d conf=%.2f\n",
                    i, boxes[i].x, boxes[i].y, boxes[i].width, boxes[i].height, boxes[i].confidence);
        }
    }

    /* Step 3: Transcribe using in-memory OCR */
    // We need to access the internal boxes structure for OCR transcription
    // This is a bit ugly but necessary since ocr_transcribe_from_rgb expects text_detector_boxes_t*
    int count = ocr_transcribe_from_rgb(rgb, &result->boxes, text_boxes);

    /* Free detection result (text_boxes owns the OCR results) */
    text_detector_result_free(result);

    return count;
}
