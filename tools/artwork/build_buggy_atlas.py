#!/usr/bin/env python3
"""Build a deterministic 4x two-part light-vehicle atlas from master artwork."""

from __future__ import annotations

import argparse
import math
from pathlib import Path

from PIL import Image


CELL_SIZE = 96
ATLAS_COLUMNS = 8
ATLAS_ROWS = 8
FRAME_COUNT = 64


def alpha_crop(image: Image.Image, padding: int = 12) -> Image.Image:
    bounds = image.getchannel("A").getbbox()
    if bounds is None:
        raise ValueError("component contains no visible pixels")
    left, top, right, bottom = bounds
    return image.crop(
        (
            max(0, left - padding),
            max(0, top - padding),
            min(image.width, right + padding),
            min(image.height, bottom + padding),
        )
    )


def centered_component(
    component: Image.Image,
    maximum_diagonal: int,
    pivot: tuple[float, float] = (0.5, 0.5),
) -> Image.Image:
    # Keep every rotation inside its cell. Limiting only width/height clips
    # long sprites at diagonal facings.
    scale = maximum_diagonal / math.hypot(component.width, component.height)
    size = (
        max(1, round(component.width * scale)),
        max(1, round(component.height * scale)),
    )
    resized = component.resize(size, Image.Resampling.LANCZOS)
    cell = Image.new("RGBA", (CELL_SIZE, CELL_SIZE), (0, 0, 0, 0))
    pivot_x = round(pivot[0] * resized.width)
    pivot_y = round(pivot[1] * resized.height)
    cell.alpha_composite(resized, (CELL_SIZE // 2 - pivot_x, CELL_SIZE // 2 - pivot_y))
    return cell


def component_split(sheet: Image.Image) -> int:
    """Find the widest transparent vertical gap between chassis and turret."""
    alpha = sheet.getchannel("A")
    occupied = []
    for x in range(sheet.width):
        occupied.append(alpha.crop((x, 0, x + 1, sheet.height)).getextrema()[1] > 16)

    runs: list[tuple[int, int]] = []
    start = None
    for x, present in enumerate(occupied):
        if not present and start is None:
            start = x
        elif present and start is not None:
            if start > 0 and x < sheet.width:
                runs.append((start, x))
            start = None
    if not runs:
        raise ValueError("could not find transparent gap between chassis and turret")
    left, right = max(runs, key=lambda run: run[1] - run[0])
    return (left + right) // 2


def rotated_frame(master: Image.Image, shape_frame: int) -> Image.Image:
    # C&C's BodyShape table reverses logical facing: SHP frame 1 is one
    # 11.25-degree step counter-clockwise from north.
    return master.rotate(
        shape_frame * 11.25,
        resample=Image.Resampling.BICUBIC,
        expand=False,
        center=(CELL_SIZE / 2, CELL_SIZE / 2),
    )


def build_atlas(
    source: Path,
    output: Path,
    preview: Path | None,
    turret_pivot_y: float,
) -> None:
    sheet = Image.open(source).convert("RGBA")
    midpoint = component_split(sheet)
    chassis = alpha_crop(sheet.crop((0, 0, midpoint, sheet.height)))
    turret = alpha_crop(sheet.crop((midpoint, 0, sheet.width, sheet.height)))

    chassis_master = centered_component(chassis, 90)
    # The gun barrel makes the visible bounding box asymmetric. Centre the
    # circular mounting ring, not the alpha bounds, so rotation stays fixed to
    # the vehicle's actual turret socket.
    turret_master = centered_component(turret, 86, (0.5, turret_pivot_y))
    atlas = Image.new(
        "RGBA",
        (CELL_SIZE * ATLAS_COLUMNS, CELL_SIZE * ATLAS_ROWS),
        (0, 0, 0, 0),
    )

    for frame in range(FRAME_COUNT):
        source_frame = frame if frame < 32 else frame - 32
        master = chassis_master if frame < 32 else turret_master
        sprite = rotated_frame(master, source_frame)
        x = (frame % ATLAS_COLUMNS) * CELL_SIZE
        y = (frame // ATLAS_COLUMNS) * CELL_SIZE
        atlas.alpha_composite(sprite, (x, y))

    output.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(output, optimize=True)

    if preview is not None:
        preview.parent.mkdir(parents=True, exist_ok=True)
        canvas = Image.new("RGBA", (CELL_SIZE * 8, CELL_SIZE * 2), (26, 29, 31, 255))
        sample_frames = (0, 8, 16, 24)
        for column, frame in enumerate(sample_frames):
            body = atlas.crop((
                (frame % ATLAS_COLUMNS) * CELL_SIZE,
                (frame // ATLAS_COLUMNS) * CELL_SIZE,
                (frame % ATLAS_COLUMNS + 1) * CELL_SIZE,
                (frame // ATLAS_COLUMNS + 1) * CELL_SIZE,
            ))
            turret_frame = frame + 32
            turret_sprite = atlas.crop((
                (turret_frame % ATLAS_COLUMNS) * CELL_SIZE,
                (turret_frame // ATLAS_COLUMNS) * CELL_SIZE,
                (turret_frame % ATLAS_COLUMNS + 1) * CELL_SIZE,
                (turret_frame // ATLAS_COLUMNS + 1) * CELL_SIZE,
            ))
            combined = Image.new("RGBA", (CELL_SIZE, CELL_SIZE), (0, 0, 0, 0))
            combined.alpha_composite(body)
            combined.alpha_composite(turret_sprite)
            canvas.alpha_composite(combined.resize((CELL_SIZE * 2, CELL_SIZE * 2), Image.Resampling.NEAREST),
                                   (column * CELL_SIZE * 2, 0))
        canvas.save(preview, optimize=True)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--preview", type=Path)
    parser.add_argument(
        "--turret-pivot-y",
        type=float,
        default=0.755,
        help="normalised mounting-ring centre in the alpha-cropped turret",
    )
    args = parser.parse_args()
    if not 0.0 < args.turret_pivot_y < 1.0:
        parser.error("--turret-pivot-y must be between zero and one")
    build_atlas(args.source, args.output, args.preview, args.turret_pivot_y)


if __name__ == "__main__":
    main()
