# text-detector-lib

A lightweight C library for detecting and recognizing text in images. Optimized for UI text detection (LLM agents, automation tools, accessibility).

## Quick Start

```c
#include "text_detector.h"
#include <stdio.h>

int main() {
    // Initialize with English OCR
    text_detector_init(NULL, "eng");

    // Load image
    float rgb[TEXT_DETECTOR_INPUT_HEIGHT][TEXT_DETECTOR_INPUT_WIDTH][3];
    text_detector_load_png("screenshot.png", rgb);

    // Detect and transcribe text
    text_detector_text_boxes_t* boxes = text_detector_text_boxes_create(256);
    text_detector_detect_and_transcribe("screenshot.png", rgb, boxes, 3);

    // Print results
    for (size_t i = 0; i < boxes->count; i++) {
        printf("Text: \"%s\" at (%d, %d) size %dx%d, confidence: %.1f%%\n",
               boxes->boxes[i].text,
               boxes->boxes[i].box.x,
               boxes->boxes[i].box.y,
               boxes->boxes[i].box.width,
               boxes->boxes[i].box.height,
               boxes->boxes[i].text_confidence * 100.0f);
    }

    // Cleanup
    text_detector_text_boxes_free(boxes);
    text_detector_cleanup();
    return 0;
}
```

Compile:
```bash
gcc -o myapp main.c -ltextdetector -lpng -ltesseract -llept -lm
```

## Installation

```bash
git clone https://github.com/MatthewScholefield/text-detector-lib.git
cd text-detector-lib
mkdir build && cd build
cmake ..
make
sudo make install
```

**Requirements:**
- C compiler with C11 support
- CMake 3.15+
- libpng-dev, tesseract-dev, leptonica-dev

Fedora/RHEL:
```bash
sudo dnf install libpng-devel tesseract-devel leptonica-devel cmake gcc gcc-c++
```

Debian/Ubuntu:
```bash
sudo apt-get install libpng-dev libtesseract-dev libleptonica-dev cmake build-essential
```

## API Reference

```c
// Initialization
int text_detector_init(const char* tessdata_path, const char* language);
void text_detector_cleanup(void);

// Load image into RGB array
int text_detector_load_png(const char* filename,
                           float rgb[TEXT_DETECTOR_INPUT_HEIGHT][TEXT_DETECTOR_INPUT_WIDTH][3]);

// Detect and transcribe text (main API)
int text_detector_detect_and_transcribe(const char* original_image,
                                        const float rgb[TEXT_DETECTOR_INPUT_HEIGHT][TEXT_DETECTOR_INPUT_WIDTH][3],
                                        text_detector_text_boxes_t* text_boxes,
                                        int padding);

// Manage text boxes
text_detector_text_boxes_t* text_detector_text_boxes_create(size_t capacity);
void text_detector_text_boxes_free(text_detector_text_boxes_t* boxes);

// Data structures
typedef struct {
    int x, y, width, height;    // Bounding box coordinates (pixels)
    float confidence;            // Detection confidence (0.0 to 1.0)
    int pixel_count;             // Number of pixels in region
} text_detector_box_t;

typedef struct {
    text_detector_box_t box;
    char* text;                 // Transcribed text (heap-allocated)
    float text_confidence;       // OCR confidence (0.0 to 1.0)
} text_detector_text_box_t;

typedef struct {
    text_detector_text_box_t* boxes;
    size_t count;
    size_t capacity;
} text_detector_text_boxes_t;

// Utility functions
int text_detector_save_annotated(const char* filename,
                                 const char* original_image,
                                 const text_detector_boxes_t* boxes);
int text_detector_save_crops(const char* original_image,
                             const text_detector_boxes_t* boxes,
                             const char* output_prefix);
```

## Configuration

**Custom input dimensions:**
```bash
cmake -DTEXT_DETECTOR_INPUT_WIDTH=512 -DTEXT_DETECTOR_INPUT_HEIGHT=512 ..
make
```

Constraints: dimensions must be multiples of 4.

**Different OCR languages:**
```c
text_detector_init(NULL, "fra");  // French
text_detector_init(NULL, "deu");  // German
text_detector_init("/usr/share/tessdata", "spa");  // Spanish with custom path
```

Install language packs: `tesseract-langpack-*` (Fedora) or `tesseract-ocr-*` (Ubuntu).

## CLI Tool

```bash
# English OCR
text-detector-cli screenshot.png output.png

# French OCR with custom padding
text-detector-cli --ocr-lang fra --padding 5 screenshot.png output.png

# Save cropped text regions
text-detector-cli --dump-crops crop screenshot.png output.png
```

## Integration

**CMake (installed library):**
```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(TEXTDETECTOR REQUIRED textdetector)
add_executable(myapp main.c)
target_link_libraries(myapp PRIVATE textdetector::textdetector)
```

**CMake (from source):**
```cmake
add_subdirectory(path/to/text-detector-lib)
target_link_libraries(myapp PRIVATE textdetector::textdetector)
```

## Performance

- **Detection**: ~5-15ms on modern CPU
- **OCR**: ~50-200ms per text region
- **Memory**: ~2MB for model weights
- **Input**: 256x256 (default, configurable)

## How It Works

1. **CNN Detection**: Custom-trained convolutional neural network processes image to identify text regions
2. **Connected Components**: Groups text regions into bounding boxes
3. **OCR Transcription**: Tesseract OCR extracts text from each region
4. **Output**: Returns bounding boxes with coordinates, confidence scores, and transcribed text

## License

MIT License - see [LICENSE](LICENSE) file

## Contributing

Contributions welcome! Please feel free to submit issues or pull requests.
