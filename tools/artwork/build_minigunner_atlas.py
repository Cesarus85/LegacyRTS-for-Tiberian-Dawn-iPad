#!/usr/bin/env python3
"""Build the deterministic 4x E1 minigunner pose/facing atlas."""

from __future__ import annotations

import argparse
import math
from pathlib import Path

from PIL import Image


CELL_SIZE = 96
POSE_COLUMNS = 5
POSE_ROWS = 2
POSE_COUNT = 10
FACING_COUNT = 8


def grid_cell(sheet: Image.Image, index: int) -> Image.Image:
    column = index % POSE_COLUMNS
    row = index // POSE_COLUMNS
    left = round(column * sheet.width / POSE_COLUMNS)
    right = round((column + 1) * sheet.width / POSE_COLUMNS)
    top = round(row * sheet.height / POSE_ROWS)
    bottom = round((row + 1) * sheet.height / POSE_ROWS)
    return sheet.crop((left, top, right, bottom))


def prepare_pose(image: Image.Image) -> Image.Image:
    bounds = image.getchannel("A").getbbox()
    if bounds is None:
        raise ValueError("pose contains no visible pixels")
    pose = image.crop(bounds)
    scale = 88 / math.hypot(pose.width, pose.height)
    size = (max(1, round(pose.width * scale)), max(1, round(pose.height * scale)))
    pose = pose.resize(size, Image.Resampling.LANCZOS)
    cell = Image.new("RGBA", (CELL_SIZE, CELL_SIZE), (0, 0, 0, 0))
    # Feet/ground contact stays close to the same logical point between poses.
    cell.alpha_composite(pose, ((CELL_SIZE - pose.width) // 2, (CELL_SIZE - pose.height) // 2))
    return cell


def build_atlas(source: Path, output: Path, preview: Path | None) -> None:
    sheet = Image.open(source).convert("RGBA")
    atlas = Image.new("RGBA", (CELL_SIZE * FACING_COUNT, CELL_SIZE * POSE_COUNT), (0, 0, 0, 0))

    for pose_index in range(POSE_COUNT):
        north = prepare_pose(grid_cell(sheet, pose_index))
        for facing in range(FACING_COUNT):
            frame = north.rotate(
                facing * 45.0,
                resample=Image.Resampling.BICUBIC,
                expand=False,
                center=(CELL_SIZE / 2, CELL_SIZE / 2),
            )
            atlas.alpha_composite(frame, (facing * CELL_SIZE, pose_index * CELL_SIZE))

    output.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(output, optimize=True)

    if preview is not None:
        preview.parent.mkdir(parents=True, exist_ok=True)
        canvas = Image.new("RGBA", (CELL_SIZE * 5, CELL_SIZE * 2), (26, 29, 31, 255))
        for pose_index in range(POSE_COUNT):
            facing = 0 if pose_index < 5 else 6
            frame = atlas.crop((
                facing * CELL_SIZE,
                pose_index * CELL_SIZE,
                (facing + 1) * CELL_SIZE,
                (pose_index + 1) * CELL_SIZE,
            ))
            canvas.alpha_composite(frame, ((pose_index % 5) * CELL_SIZE, (pose_index // 5) * CELL_SIZE))
        canvas.save(preview, optimize=True)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--preview", type=Path)
    args = parser.parse_args()
    build_atlas(args.source, args.output, args.preview)


if __name__ == "__main__":
    main()
