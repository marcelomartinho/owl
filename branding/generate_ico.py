#!/usr/bin/env python3
"""
Generate a placeholder owl.ico file for the Owl Browser.

This creates a simple multi-size ICO file with an "O" logo.
Replace with proper branding artwork before release.

Usage:
    python generate_ico.py

Output:
    owl.ico in the same directory

Requirements:
    pip install Pillow
"""

import sys
try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("Pillow not installed. Install with: pip install Pillow")
    print("Or provide your own owl.ico file in branding/")
    sys.exit(1)

import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUTPUT_PATH = os.path.join(SCRIPT_DIR, "owl.ico")

# Icon sizes required for Windows (16x16 to 256x256)
SIZES = [16, 24, 32, 48, 64, 128, 256]

# Owl brand colors
BG_COLOR = (45, 55, 72)       # Dark slate
FG_COLOR = (236, 201, 75)     # Gold/amber (owl eyes)
RING_COLOR = (160, 174, 192)  # Light slate


def create_owl_icon(size: int) -> Image.Image:
    """Create a single icon at the given size."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Background circle
    margin = max(1, size // 16)
    draw.ellipse(
        [margin, margin, size - margin - 1, size - margin - 1],
        fill=BG_COLOR,
        outline=RING_COLOR,
        width=max(1, size // 32)
    )

    # Draw "O" letter centered
    font_size = int(size * 0.55)
    try:
        font = ImageFont.truetype("arial.ttf", font_size)
    except (OSError, IOError):
        try:
            font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", font_size)
        except (OSError, IOError):
            font = ImageFont.load_default()

    text = "O"
    bbox = draw.textbbox((0, 0), text, font=font)
    text_w = bbox[2] - bbox[0]
    text_h = bbox[3] - bbox[1]
    x = (size - text_w) // 2
    y = (size - text_h) // 2 - bbox[1]
    draw.text((x, y), text, fill=FG_COLOR, font=font)

    return img


def main():
    print("Generating Owl Browser icon...")
    images = []
    for size in SIZES:
        img = create_owl_icon(size)
        images.append(img)
        print(f"  Created {size}x{size}")

    # Save as ICO with all sizes
    images[0].save(
        OUTPUT_PATH,
        format="ICO",
        sizes=[(s, s) for s in SIZES],
        append_images=images[1:]
    )
    print(f"Saved: {OUTPUT_PATH}")


if __name__ == "__main__":
    main()
