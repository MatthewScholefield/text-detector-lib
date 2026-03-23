#!/usr/bin/env python3
"""
Post-process generated C model from onnx2c to replace hardcoded dimensions
with compile-time constants.

This script replaces hardcoded dimension values (like 256) with preprocessor
macros that can be configured at compile time via CMake.

Usage:
    python postprocess_model.py input.c output.c
"""

import argparse
import re
import sys
from pathlib import Path


# Default dimensions for the current model
DEFAULT_INPUT_WIDTH = 256
DEFAULT_INPUT_HEIGHT = 256
DEFAULT_CONV1_WIDTH = 128
DEFAULT_CONV1_HEIGHT = 128
DEFAULT_OUTPUT_WIDTH = 64
DEFAULT_OUTPUT_HEIGHT = 64


def replace_default_dimensions(content, input_width, input_height):
    """Replace default hardcoded dimensions with preprocessor macros.

    Args:
        content: C source code as string
        input_width: Desired input width
        input_height: Desired input height
    """
    # Calculate derived sizes
    conv1_width = input_width // 2
    conv1_height = input_height // 2
    output_width = input_width // 4
    output_height = input_height // 4

    # Replace input dimensions: [1][3][256][256]
    content = re.sub(
        r'\[1\]\[3\]\[256\]\[256\]',
        '[1][3][TEXT_DETECTOR_INPUT_HEIGHT][TEXT_DETECTOR_INPUT_WIDTH]',
        content
    )

    # Handle custom input dimensions if they differ from 256
    if input_height != 256:
        content = re.sub(
            rf'\[1\]\[3\]\[{input_height}\]\[{input_width}\]',
            '[1][3][TEXT_DETECTOR_INPUT_HEIGHT][TEXT_DETECTOR_INPUT_WIDTH]',
            content
        )

    # Replace conv1 output dimensions: [1][4][128][128]
    content = re.sub(
        r'\[1\]\[4\]\[128\]\[128\]',
        '[1][4][TEXT_DETECTOR_CONV1_HEIGHT][TEXT_DETECTOR_CONV1_WIDTH]',
        content
    )

    # Handle custom conv1 dimensions
    if conv1_height != 128:
        content = re.sub(
            rf'\[1\]\[4\]\[{conv1_height}\]\[{conv1_width}\]',
            '[1][4][TEXT_DETECTOR_CONV1_HEIGHT][TEXT_DETECTOR_CONV1_WIDTH]',
            content
        )

    # Replace conv2 dimensions: [1][8][64][64]
    content = re.sub(
        r'\[1\]\[8\]\[64\]\[64\]',
        '[1][8][TEXT_DETECTOR_OUTPUT_HEIGHT][TEXT_DETECTOR_OUTPUT_WIDTH]',
        content
    )

    # Handle custom output dimensions in conv2
    if output_height != 64:
        content = re.sub(
            rf'\[1\]\[8\]\[{output_height}\]\[{output_width}\]',
            '[1][8][TEXT_DETECTOR_OUTPUT_HEIGHT][TEXT_DETECTOR_OUTPUT_WIDTH]',
            content
        )

    # Replace final output dimensions: [1][1][64][64]
    content = re.sub(
        r'\[1\]\[1\]\[64\]\[64\]',
        '[1][1][TEXT_DETECTOR_OUTPUT_HEIGHT][TEXT_DETECTOR_OUTPUT_WIDTH]',
        content
    )

    # Handle custom output dimensions in final layer
    if output_height != 64:
        content = re.sub(
            rf'\[1\]\[1\]\[{output_height}\]\[{output_width}\]',
            '[1][1][TEXT_DETECTOR_OUTPUT_HEIGHT][TEXT_DETECTOR_OUTPUT_WIDTH]',
            content
        )

    # Add configuration header include at the beginning
    # Find the last include line and insert our config include after it
    config_include = '#include "text_detector_model_config.h"\n'

    lines = content.split('\n')
    insert_pos = 0
    for i, line in enumerate(lines):
        if line.startswith('#include <'):
            insert_pos = i + 1
        elif line.startswith('#include "') and insert_pos > 0:
            break

    if insert_pos > 0:
        lines.insert(insert_pos, '')
        lines.insert(insert_pos + 1, config_include)
    else:
        # Prepend if no includes found
        lines.insert(0, config_include)

    content = '\n'.join(lines)

    return content


def main():
    parser = argparse.ArgumentParser(
        description='Post-process onnx2c generated C code to use configurable dimensions'
    )
    parser.add_argument('input', type=Path, help='Input C file from onnx2c')
    parser.add_argument('output', type=Path, help='Output C file with configurable dimensions')
    parser.add_argument('--width', type=int, default=256,
                       help='Input width to use for validation (default: 256)')
    parser.add_argument('--height', type=int, default=256,
                       help='Input height to use for validation (default: 256)')

    args = parser.parse_args()

    # Validate dimensions
    if args.width % 4 != 0 or args.height % 4 != 0:
        print("Error: Width and height must be multiples of 4", file=sys.stderr)
        return 1

    # Read input
    try:
        content = args.input.read_text()
    except Exception as e:
        print(f"Error reading input file: {e}", file=sys.stderr)
        return 1

    # Replace dimensions
    content = replace_default_dimensions(content, args.width, args.height)

    # Write output
    try:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(content)
    except Exception as e:
        print(f"Error writing output file: {e}", file=sys.stderr)
        return 1

    print(f"Post-processed model: {args.input} -> {args.output}")
    print(f"  Input dimensions: {args.width}x{args.height}")
    print(f"  Output dimensions: {args.width//4}x{args.height//4}")

    return 0


if __name__ == '__main__':
    sys.exit(main())
