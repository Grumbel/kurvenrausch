#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Regenerates the screenshots used by the README.

Drives the game's headless mode (no window or display needed) and composes the
frames with Pillow:

    tools/make_screenshots.py [--binary build/kurvenrausch] [--out docs]

docs/screenshot.png  the game on the Cote d'Azur corniche, at 3x
docs/zones.png       the start of each zone of the track, in a grid
"""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image


def run_game(binary, *args):
    subprocess.run([str(binary), *args], check=True)


def zone_positions(binary):
    """Camera positions of the zone starts, from --print-zones."""
    out = subprocess.run([str(binary), "--print-zones"], check=True,
                         capture_output=True, text=True).stdout
    zones = []
    for line in out.strip().splitlines():
        index, country, region, _segments, position = line.split("\t")
        zones.append((int(index), country, region, int(position.split()[1])))
    return zones


def capture(binary, directory, name, *args):
    path = Path(directory) / f"{name}.bmp"
    run_game(binary, "--screenshot", str(path), *args)
    return Image.open(path).convert("RGB")


def save_png(image, path):
    # Pixel art has few colours; a palette keeps the files small.
    image.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE) \
         .save(path, optimize=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--binary", default="build/kurvenrausch")
    parser.add_argument("--out", default="docs")
    parser.add_argument("--hero-position", default="112000",
                        help="camera position of the main screenshot")
    parser.add_argument("--hero-frames", default="200",
                        help="simulation steps before the main screenshot")
    args = parser.parse_args()

    binary = Path(args.binary)
    out = Path(args.out)
    if not binary.exists():
        sys.exit(f"{binary} not found; build the game first")
    out.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory() as tmp:
        hero = capture(binary, tmp, "hero", "--position", args.hero_position,
                       "--frames", args.hero_frames)
        save_png(hero.resize((hero.width * 3, hero.height * 3), Image.NEAREST),
                 out / "screenshot.png")

        frames = [capture(binary, tmp, f"zone{index}", "--zone", str(index), "--frames", "150")
                  for index, _country, _region, _position in zone_positions(binary)]
        columns, scale, gap = 5, 2, 6
        w, h = frames[0].size
        rows = (len(frames) + columns - 1) // columns
        sheet = Image.new("RGB", (columns * w * scale + (columns - 1) * gap,
                                  rows * h * scale + (rows - 1) * gap), (16, 16, 24))
        for i, frame in enumerate(frames):
            sheet.paste(frame.resize((w * scale, h * scale), Image.NEAREST),
                        ((i % columns) * (w * scale + gap), (i // columns) * (h * scale + gap)))
        save_png(sheet, out / "zones.png")

    print(f"wrote {out / 'screenshot.png'} and {out / 'zones.png'}")


if __name__ == "__main__":
    main()
