/**
 * Combined detection and transcription - in-memory version
 *
 * This file provides text_detector_detect_and_transcribe_memory(),
 * which works entirely with in-memory RGB data without requiring file I/O.
 */

#include "text_detector.h"
#include "bounding_boxes.h"
#include <stdio.h>
#include <string.h>

/* External functions */
extern int text_detector_detect(const float rgb[TEXT_DETECTOR_INPUT_HEIGHT][TEXT_DETECTOR_INPUT_WIDTH][3],
                                text_detector_result_t* result);
extern int ocr_transcribe_from_rgb(const float rgb[TEXT_DETECTOR_INPUT_HEIGHT][TEXT_DETECTOR_INPUT_WIDTH][3],
                                   const text_detector_boxes_t* boxes,
                                   text_detector_text_boxes_t* text_boxes);

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

    /* Step 1: Run detection */
    text_detector_result_t result;
    if (text_detector_detect(rgb, &result) != 0) {
        fprintf(stderr, "Error: Detection failed\n");
        return -1;
    }

    /* Step 2: Convert boxes from cell coordinates to pixel coordinates */
    text_detector_convert_to_pixels(&result.boxes, padding);

    /* Step 3: Transcribe using in-memory OCR */
    int count = ocr_transcribe_from_rgb(rgb, &result.boxes, text_boxes);

    /* Free detection result boxes (text_boxes owns the OCR results) */
    text_detector_boxes_free(&result.boxes);

    return count;
}
