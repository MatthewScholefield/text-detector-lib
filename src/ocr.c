/**
 * OCR integration using Tesseract
 */

#include "text_detector.h"
#include "bounding_boxes.h"
#include <tesseract/capi.h>
#include <leptonica/allheaders.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>

#define MODEL_HEIGHT TEXT_DETECTOR_INPUT_HEIGHT
#define MODEL_WIDTH TEXT_DETECTOR_INPUT_WIDTH

/* Tesseract API handle */
static TessBaseAPI* tesseract_api = NULL;
static int tesseract_initialized = 0;

/* External functions from image_io.c */
extern int load_png(const char* filename, float rgb[MODEL_HEIGHT][MODEL_WIDTH][3]);

/* Internal OCR initialization - called from text_detector_init() */
int ocr_init(const char* tessdata_path, const char* language) {
    if (tesseract_initialized) {
        return 0;  /* Already initialized */
    }

    /* Create Tesseract API */
    tesseract_api = TessBaseAPICreate();
    if (!tesseract_api) {
        fprintf(stderr, "Error: Failed to create Tesseract API\n");
        return -1;
    }

    /* Default to English if no language specified */
    const char* lang = language ? language : "eng";

    /* Initialize Tesseract */
    char* datapath = (char*)tessdata_path;  /* Tesseract wants non-const */
    if (TessBaseAPIInit3(tesseract_api, datapath, lang) != 0) {
        fprintf(stderr, "Error: Failed to initialize Tesseract\n");
        fprintf(stderr, "  Tessdata path: %s\n", tessdata_path ? tessdata_path : "(default)");
        fprintf(stderr, "  Language: %s\n", lang);
        fprintf(stderr, "  Make sure tesseract is installed and tessdata is available\n");
        TessBaseAPIDelete(tesseract_api);
        tesseract_api = NULL;
        return -1;
    }

    /* Set page segmentation mode to auto */
    TessBaseAPISetPageSegMode(tesseract_api, PSM_AUTO);

    tesseract_initialized = 1;
    return 0;
}

/* Internal OCR cleanup - called from text_detector_cleanup() */
void ocr_cleanup(void) {
    if (tesseract_api) {
        TessBaseAPIDelete(tesseract_api);
        tesseract_api = NULL;
    }
    tesseract_initialized = 0;
}

/**
 * Load a region from the original image as PIX for Tesseract.
 * Adjusts coordinates for letterboxing offset.
 */
static PIX* load_image_region(const char* original_image,
                              const text_detector_box_t* box,
                              int orig_width, int orig_height,
                              int offset_x, int offset_y) {
    FILE* fp = fopen(original_image, "rb");
    if (!fp) {
        return NULL;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        return NULL;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return NULL;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

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

    /* Adjust box coordinates from letterboxed model space to original image space */
    int x1 = box->x - offset_x;
    int y1 = box->y - offset_y;
    int x2 = (box->x + box->width) - offset_x;
    int y2 = (box->y + box->height) - offset_y;

    /* Clamp to image bounds */
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= orig_width) x2 = orig_width - 1;
    if (y2 >= orig_height) y2 = orig_height - 1;

    int crop_width = x2 - x1 + 1;
    int crop_height = y2 - y1 + 1;

    if (crop_width <= 0 || crop_height <= 0) {
        /* Free resources */
        for (int y = 0; y < orig_height; y++) {
            free(row_pointers[y]);
        }
        free(row_pointers);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return NULL;
    }

    /* Create PIX structure for Tesseract */
    PIX* pix = pixCreate(crop_width, crop_height, 32);  /* 32-bit RGBA */

    /* Copy pixels to PIX */
    for (int y = 0; y < crop_height; y++) {
        for (int x = 0; x < crop_width; x++) {
            int src_x = x1 + x;
            int src_y = y1 + y;
            png_bytep src_row = row_pointers[src_y];

            u_int32_t pixel;
            unsigned char r = src_row[src_x * 3 + 0];
            unsigned char g = src_row[src_x * 3 + 1];
            unsigned char b = src_row[src_x * 3 + 2];

            /* Compose RGB into 32-bit pixel (0xBBGGRRAA for leptonica) */
            pixel = (r << L_RED_SHIFT) | (g << L_GREEN_SHIFT) | (b << L_BLUE_SHIFT);
            pixSetPixel(pix, x, y, pixel);
        }
    }

    /* Free resources */
    for (int y = 0; y < orig_height; y++) {
        free(row_pointers[y]);
    }
    free(row_pointers);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);

    return pix;
}

/* Internal transcription function - called from text_detector_detect_and_transcribe() */
int ocr_transcribe(const char* original_image,
                   const text_detector_boxes_t* boxes,
                   text_detector_text_boxes_t* text_boxes) {
    if (!tesseract_initialized || !tesseract_api) {
        fprintf(stderr, "Error: OCR not initialized.\n");
        return -1;
    }

    if (!original_image || !boxes || !text_boxes) {
        return -1;
    }

    /* Get original image dimensions */
    FILE* fp = fopen(original_image, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open image: %s\n", original_image);
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

    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);

    /* Calculate letterboxing offset */
    float scale_x = (float)MODEL_WIDTH / (float)orig_width;
    float scale_y = (float)MODEL_HEIGHT / (float)orig_height;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;
    int scaled_width = (int)(orig_width * scale);
    int scaled_height = (int)(orig_height * scale);
    int offset_x = (MODEL_WIDTH - scaled_width) / 2;
    int offset_y = (MODEL_HEIGHT - scaled_height) / 2;

    /* Transcribe each box */
    size_t count = (boxes->count < text_boxes->capacity) ? boxes->count : text_boxes->capacity;
    text_boxes->count = 0;

    for (size_t i = 0; i < count; i++) {
        /* Load image region */
        PIX* pix = load_image_region(original_image, &boxes->boxes[i],
                                     orig_width, orig_height, offset_x, offset_y);
        if (!pix) {
            fprintf(stderr, "Warning: Failed to load region %zu\n", i);
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
