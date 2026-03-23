/**
 * Text Detection CLI Tool
 *
 * Command-line interface for the text detection library with OCR support.
 */

#include "text_detector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {
    const char* input_file = NULL;
    const char* output_file = "output.png";
    const char* crop_prefix = NULL;
    const char* ocr_lang = NULL;
    const char* tessdata_path = NULL;
    int padding = 3;

    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fprintf(stderr, "Usage: %s [options] <input.png> [output.png]\n", argv[0]);
            fprintf(stderr, "\nOptions:\n");
            fprintf(stderr, "  --ocr-lang LANG    OCR language code (default: eng)\n");
            fprintf(stderr, "  --tessdata PATH    Path to tessdata directory\n");
            fprintf(stderr, "  --padding N        Padding to add to bounding boxes in pixels (default: 3)\n");
            fprintf(stderr, "  --dump-crops PREFIX  Save cropped text regions as PREFIX_000.png, PREFIX_001.png, etc.\n");
            fprintf(stderr, "  --help, -h         Show this help message\n");
            fprintf(stderr, "\nArguments:\n");
            fprintf(stderr, "  input.png          Input PNG image (will be resized to %dx%d)\n",
                    TEXT_DETECTOR_INPUT_WIDTH, TEXT_DETECTOR_INPUT_HEIGHT);
            fprintf(stderr, "  output.png         Optional output filename with bounding boxes (default: output.png)\n");
            fprintf(stderr, "\nExamples:\n");
            fprintf(stderr, "  %s input.png                    # English OCR\n", argv[0]);
            fprintf(stderr, "  %s --ocr-lang fra input.png     # French OCR\n", argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--ocr-lang") == 0) {
            if (i + 1 < argc) {
                ocr_lang = argv[++i];
            } else {
                fprintf(stderr, "Error: --ocr-lang requires an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--tessdata") == 0) {
            if (i + 1 < argc) {
                tessdata_path = argv[++i];
            } else {
                fprintf(stderr, "Error: --tessdata requires an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--padding") == 0) {
            if (i + 1 < argc) {
                padding = atoi(argv[++i]);
            } else {
                fprintf(stderr, "Error: --padding requires an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--dump-crops") == 0) {
            if (i + 1 < argc) {
                crop_prefix = argv[++i];
            } else {
                fprintf(stderr, "Error: --dump-crops requires a prefix argument\n");
                return 1;
            }
        } else if (input_file == NULL) {
            input_file = argv[i];
        } else {
            output_file = argv[i];
        }
    }

    if (input_file == NULL) {
        fprintf(stderr, "Usage: %s [options] <input.png> [output.png]\n", argv[0]);
        fprintf(stderr, "Use --help for more information\n");
        return 1;
    }

    printf("Text Detection CLI v%d.%d.%d\n",
           TEXT_DETECTOR_VERSION_MAJOR,
           TEXT_DETECTOR_VERSION_MINOR,
           TEXT_DETECTOR_VERSION_PATCH);
    printf("=====================================================\n");
    printf("Input: %s\n", input_file);
    printf("Output: %s\n", output_file);
    printf("Padding: %d pixels\n", padding);
    printf("OCR Language: %s\n", ocr_lang ? ocr_lang : "eng (default)");
    if (tessdata_path) {
        printf("Tessdata: %s\n", tessdata_path);
    }
    if (crop_prefix) {
        printf("Crop prefix: %s\n", crop_prefix);
    }
    printf("\n");

    /* Initialize library */
    printf("Initializing text detector...\n");
    if (text_detector_init(tessdata_path, ocr_lang) != 0) {
        fprintf(stderr, "Error: Failed to initialize text detector\n");
        return 1;
    }

    /* Allocate input tensor */
    float rgb[TEXT_DETECTOR_INPUT_HEIGHT][TEXT_DETECTOR_INPUT_WIDTH][3];

    /* Load PNG and convert to model input format */
    printf("Loading image...\n");
    if (text_detector_load_png(input_file, rgb) != 0) {
        fprintf(stderr, "Error: Failed to load image\n");
        text_detector_cleanup();
        return 1;
    }

    /* Run detection and transcription */
    text_detector_text_boxes_t* text_boxes = text_detector_text_boxes_create(256);
    if (!text_boxes) {
        fprintf(stderr, "Error: Failed to allocate memory\n");
        text_detector_cleanup();
        return 1;
    }

    printf("Running detection and OCR...\n");
    int count = text_detector_detect_and_transcribe(input_file, rgb, text_boxes, padding);
    if (count < 0) {
        fprintf(stderr, "Error: Detection/OCR failed\n");
        text_detector_text_boxes_free(text_boxes);
        text_detector_cleanup();
        return 1;
    }
    printf("Detection complete.\n\n");

    if (text_boxes->count == 0) {
        printf("No text detected.\n");
    } else {
        printf("Detected and transcribed %zu text region(s):\n", text_boxes->count);
        for (size_t i = 0; i < text_boxes->count; i++) {
            const text_detector_text_box_t* tb = &text_boxes->boxes[i];
            printf("  [%zu] \"%s\" (confidence: %.2f%%, box: x=%d, y=%d, w=%d, h=%d)\n",
                   i, tb->text ? tb->text : "(empty)",
                   tb->text_confidence * 100.0f,
                   tb->box.x, tb->box.y, tb->box.width, tb->box.height);
        }

        /* Prepare boxes for output */
        text_detector_boxes_t boxes;
        boxes.boxes = (text_detector_box_t*)malloc(sizeof(text_detector_box_t) * text_boxes->count);
        boxes.count = text_boxes->count;
        boxes.capacity = text_boxes->count;

        for (size_t i = 0; i < text_boxes->count; i++) {
            boxes.boxes[i] = text_boxes->boxes[i].box;
        }

        /* Save cropped regions if requested */
        if (crop_prefix) {
            printf("\nSaving cropped regions...\n");
            if (text_detector_save_crops(input_file, &boxes, crop_prefix) != 0) {
                fprintf(stderr, "Warning: Failed to save some cropped regions\n");
            }
            printf("\n");
        }

        /* Save annotated image with boxes */
        printf("Saving annotated image...\n");
        if (text_detector_save_annotated(output_file, input_file, &boxes) != 0) {
            fprintf(stderr, "Warning: Failed to save annotated image\n");
        }

        free(boxes.boxes);
    }

    text_detector_text_boxes_free(text_boxes);
    text_detector_cleanup();
    printf("\nDone!\n");
    return 0;
}
