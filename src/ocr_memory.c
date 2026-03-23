/**
 * OCR integration using Tesseract - in-memory version
 *
 * This file provides OCR functions that work directly with in-memory RGB data,
 * avoiding the need for temporary PNG files.
 */

#include "text_detector.h"
#include <tesseract/capi.h>
#include <leptonica/allheaders.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MODEL_HEIGHT TEXT_DETECTOR_INPUT_HEIGHT
#define MODEL_WIDTH TEXT_DETECTOR_INPUT_WIDTH

/* External Tesseract API handle from ocr.c */
extern TessBaseAPI* tesseract_api;
extern int tesseract_initialized;

/**
 * Create a PIX structure from RGB memory region.
 * This is the in-memory equivalent of load_image_region().
 *
 * Args:
 *   rgb: Full RGB image data [height][width][3] in model space (0-1 normalized)
 *   full_width: Width of the full RGB array
 *   full_height: Height of the full RGB array
 *   box: Bounding box to extract (in model space coordinates)
 *
 * Returns:
 *   PIX structure for Tesseract, or NULL on error
 */
static PIX* create_pix_from_rgb_region(
    const float rgb[MODEL_HEIGHT][MODEL_WIDTH][3],
    int full_width,
    int full_height,
    const text_detector_box_t* box) {

    /* Extract region */
    int x1 = box->x;
    int y1 = box->y;
    int x2 = box->x + box->width;
    int y2 = box->y + box->height;

    /* Clamp to bounds */
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= full_width) x2 = full_width - 1;
    if (y2 >= full_height) y2 = full_height - 1;

    int crop_width = x2 - x1;
    int crop_height = y2 - y1;

    if (crop_width <= 0 || crop_height <= 0) {
        return NULL;
    }

    /* Create PIX structure for Tesseract (32-bit RGBA) */
    PIX* pix = pixCreate(crop_width, crop_height, 32);
    if (!pix) {
        return NULL;
    }

    /* Copy pixels to PIX */
    for (int y = 0; y < crop_height; y++) {
        for (int x = 0; x < crop_width; x++) {
            int src_x = x1 + x;
            int src_y = y1 + y;

            /* Convert normalized RGB (0-1) to uint8 (0-255) */
            unsigned char r = (unsigned char)(rgb[src_y][src_x][0] * 255.0f);
            unsigned char g = (unsigned char)(rgb[src_y][src_x][1] * 255.0f);
            unsigned char b = (unsigned char)(rgb[src_y][src_x][2] * 255.0f);

            /* Compose RGB into 32-bit pixel (0xBBGGRRAA for leptonica) */
            u_int32_t pixel = (r << L_RED_SHIFT) | (g << L_GREEN_SHIFT) | (b << L_BLUE_SHIFT);
            pixSetPixel(pix, x, y, pixel);
        }
    }

    return pix;
}

/**
 * Transcribe text regions using OCR with in-memory RGB data.
 * This is the in-memory equivalent of ocr_transcribe().
 *
 * Args:
 *   rgb: Full RGB image data [height][width][3] in model space (0-1 normalized)
 *   boxes: Detected bounding boxes (in model space coordinates)
 *   text_boxes: Output text boxes with transcriptions (must be pre-allocated)
 *
 * Returns:
 *   Number of transcriptions, or -1 on error
 */
int ocr_transcribe_from_rgb(
    const float rgb[MODEL_HEIGHT][MODEL_WIDTH][3],
    const text_detector_boxes_t* boxes,
    text_detector_text_boxes_t* text_boxes) {

    if (!tesseract_initialized || !tesseract_api) {
        fprintf(stderr, "Error: OCR not initialized.\n");
        return -1;
    }

    if (!rgb || !boxes || !text_boxes) {
        return -1;
    }

    /* Transcribe each box */
    size_t count = (boxes->count < text_boxes->capacity) ? boxes->count : text_boxes->capacity;
    text_boxes->count = 0;

    for (size_t i = 0; i < count; i++) {
        /* Create PIX from RGB region */
        PIX* pix = create_pix_from_rgb_region(rgb, MODEL_WIDTH, MODEL_HEIGHT, &boxes->boxes[i]);
        if (!pix) {
            fprintf(stderr, "Warning: Failed to extract region %zu\n", i);
            continue;
        }

        /* Set image to Tesseract */
        TessBaseAPISetImage2(tesseract_api, pix);

        /* Get text */
        char* text = TessBaseAPIGetUTF8Text(tesseract_api);
        if (!text) {
            pixDestroy(&pix);
            continue;
        }

        /* Trim trailing newlines */
        size_t len = strlen(text);
        while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r')) {
            text[--len] = '\0';
        }

        /* Get confidence */
        int confidence = TessBaseAPIMeanTextConf(tesseract_api);
        float conf_float = confidence / 100.0f;

        /* Store result */
        text_boxes->boxes[i].box = boxes->boxes[i];
        text_boxes->boxes[i].text = strdup(text);
        text_boxes->boxes[i].text_confidence = conf_float;
        text_boxes->count++;

        /* Cleanup */
        TessDeleteText(text);
        pixDestroy(&pix);
    }

    return (int)text_boxes->count;
}
