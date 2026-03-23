/**
 * Internal OCR functions for ocr_memory.c
 */

#ifndef OCR_INTERNAL_H
#define OCR_INTERNAL_H

#include <tesseract/capi.h>

/* Get Tesseract API handle and initialization state */
TessBaseAPI* ocr_get_tesseract_api(void);
int ocr_is_initialized(void);

#endif /* OCR_INTERNAL_H */
