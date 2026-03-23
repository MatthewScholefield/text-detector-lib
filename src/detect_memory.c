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

    if (debug_enabled()) {
        fprintf(stderr, "[LIB] detect_and_transcribe_memory: padding=%d\n", padding);
    }

    /* Step 1: Run detection */
    text_detector_result_t result;
    if (text_detector_detect(rgb, &result) != 0) {
        fprintf(stderr, "Error: Detection failed\n");
        return -1;
    }

    if (debug_enabled()) {
        fprintf(stderr, "[LIB] Detection found %zu boxes (before pixel conversion)\n", result.boxes.count);
    }

    /* Step 2: Convert boxes from cell coordinates to pixel coordinates */
    text_detector_convert_to_pixels(&result.boxes, padding);

    if (debug_enabled()) {
        fprintf(stderr, "[LIB] After pixel conversion:\n");
        for (size_t i = 0; i < result.boxes.count && i < 5; i++) {
            fprintf(stderr, "[LIB]   Box[%zu]: x=%d, y=%d, w=%d, h=%d, conf=%.2f, pixels=%d\n",
                    i, result.boxes.boxes[i].x, result.boxes.boxes[i].y,
                    result.boxes.boxes[i].width, result.boxes.boxes[i].height,
                    result.boxes.boxes[i].confidence, result.boxes.boxes[i].pixel_count);
        }
        if (result.boxes.count > 5) {
            fprintf(stderr, "[LIB]   ... and %zu more boxes\n", result.boxes.count - 5);
        }
    }

    /* Step 3: Transcribe using in-memory OCR */
    int count = ocr_transcribe_from_rgb(rgb, &result.boxes, text_boxes);

    if (debug_enabled()) {
        fprintf(stderr, "[LIB] OCR transcribed %d boxes\n", count);
    }

    /* Free detection result boxes (text_boxes owns the OCR results) */
    text_detector_boxes_free(&result.boxes);

    return count;
}
