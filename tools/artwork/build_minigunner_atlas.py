#!/usr/bin/env python3
"""Build the deterministic 4x E1 minigunner pose/facing atlas.

The fixed isometric camera needs real directional source views. Rotating a
single north-facing bitmap in the image plane makes east/west-facing soldiers
look as if they are lying down. Five authored views cover the full circle;
the three complementary views are safe horizontal mirrors.
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path

from PIL import Image

from atlas_common import visible_bounds


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


def remove_chroma_key(image: Image.Image) -> Image.Image:
    """Convert generated magenta backgrounds to alpha, including soft edges."""
    rgba = image.convert("RGBA")
    if image.mode == "RGBA" and rgba.getchannel("A").getextrema()[0] < 255:
        return rgba

    output = []
    pixels = rgba.get_flattened_data() if hasattr(rgba, "get_flattened_data") else rgba.getdata()
    for red, green, blue, source_alpha in pixels:
        magenta_dominance = min(red, blue) - green
        if magenta_dominance >= 100:
            alpha = 0
        elif magenta_dominance <= 40:
            alpha = source_alpha
        else:
            alpha = min(source_alpha, round((100 - magenta_dominance) * 255 / 60))
        if alpha > 0 and magenta_dominance > 8:
            # The generated subject contains no magenta, so any remaining
            # magenta dominance is edge spill from the key background.
            red = min(red, green + 12)
            blue = min(blue, green + 12)
        output.append((red, green, blue, alpha))
    rgba.putdata(output)
    return rgba


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


def validate_pose_registration(atlas: Image.Image) -> None:
    """Reject per-pose scale drift before it becomes animation pulsing."""
    diagonals: list[float] = []
    horizontal_centres: list[float] = []
    for pose in range(POSE_COUNT):
        for facing in range(FACING_COUNT):
            cell = atlas.crop((facing * CELL_SIZE,
                               pose * CELL_SIZE,
                               (facing + 1) * CELL_SIZE,
                               (pose + 1) * CELL_SIZE))
            left, top, right, bottom = visible_bounds(cell)
            diagonals.append(math.hypot(right - left, bottom - top))
            horizontal_centres.append((left + right) * 0.5)
    if max(diagonals) - min(diagonals) > 2.0:
        raise ValueError("minigunner pose scale changes between animation frames")
    if max(horizontal_centres) - min(horizontal_centres) > 1.0:
        raise ValueError("minigunner pose anchor changes between animation frames")


def build_atlas(sources: list[Path], output: Path, preview: Path | None) -> None:
    sheets = [remove_chroma_key(Image.open(source)) for source in sources]
    expected_size = sheets[0].size
    if any(sheet.size != expected_size for sheet in sheets[1:]):
        raise ValueError("all directional source sheets must have identical dimensions")

    # Engine facing order is N, NE, E, SE, S, SW, W, NW. The latter three
    # views mirror their right-facing counterpart without changing the camera.
    facing_sources = (
        (0, False),
        (1, False),
        (2, False),
        (3, False),
        (4, False),
        (3, True),
        (2, True),
        (1, True),
    )
    atlas = Image.new("RGBA", (CELL_SIZE * FACING_COUNT, CELL_SIZE * POSE_COUNT), (0, 0, 0, 0))

    for pose_index in range(POSE_COUNT):
        for facing, (source_index, mirrored) in enumerate(facing_sources):
            frame = prepare_pose(grid_cell(sheets[source_index], pose_index))
            if mirrored:
                frame = frame.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
            atlas.alpha_composite(frame, (facing * CELL_SIZE, pose_index * CELL_SIZE))

    validate_pose_registration(atlas)
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
    parser.add_argument("--source-n", "--source", dest="source_n", required=True, type=Path)
    parser.add_argument("--source-ne", required=True, type=Path)
    parser.add_argument("--source-e", required=True, type=Path)
    parser.add_argument("--source-se", required=True, type=Path)
    parser.add_argument("--source-s", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--preview", type=Path)
    args = parser.parse_args()
    build_atlas(
        [args.source_n, args.source_ne, args.source_e, args.source_se, args.source_s],
        args.output,
        args.preview,
    )


if __name__ == "__main__":
    main()
