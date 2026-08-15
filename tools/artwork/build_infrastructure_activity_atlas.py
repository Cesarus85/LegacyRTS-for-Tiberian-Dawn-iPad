#!/usr/bin/env python3
"""Build deterministic GDI/Nod building-activity atlases from five keyframes."""

from __future__ import annotations

import argparse
from pathlib import Path
from PIL import Image

from atlas_common import (
    clean_generated_background,
    equal_grid_cell,
    register_foundation_keyframes,
    retain_primary_component,
    visible_bounds,
)


CELL_SIZE = 256
COLUMNS = 10
ROWS = 3
CONYARD_FRAMES = 20
INFANTRY_FRAMES = 10
KEYFRAME_COUNT = 5
MARGIN = 6


def prepared_keyframes(path: Path, reference: Image.Image) -> list[Image.Image]:
    sheet = clean_generated_background(Image.open(path))
    frames = [retain_primary_component(equal_grid_cell(sheet, index, KEYFRAME_COUNT))
              for index in range(KEYFRAME_COUNT)]
    return register_foundation_keyframes(frames,
                                         CELL_SIZE,
                                         margin=MARGIN,
                                         reference=reference)


def ping_pong_position(frame: int, count: int) -> float:
    if count <= 1:
        return 0.0
    # Treat the atlas as a looping cycle. Using count-1 would duplicate the
    # middle pose in every even-sized sequence and create a visible pause.
    cycle = frame / (count * 0.5)
    return (cycle if cycle <= 1.0 else 2.0 - cycle) * (KEYFRAME_COUNT - 1)


def interpolate(frames: list[Image.Image], position: float) -> Image.Image:
    lower = max(0, min(int(position), KEYFRAME_COUNT - 1))
    upper = min(lower + 1, KEYFRAME_COUNT - 1)
    fraction = max(0.0, min(position - lower, 1.0))
    if lower == upper or fraction <= 0.001:
        return frames[lower].copy()

    # The game exposes more activity phases than the authored sheet contains.
    # Blend registered neighbouring poses instead of holding one pose for
    # several ticks. At the final game size this reads as short mechanical
    # motion blur, while the fixed foundation remains perfectly stationary.
    # Smoothstep removes the tiny velocity discontinuity at each keyframe.
    fraction = fraction * fraction * (3.0 - 2.0 * fraction)
    return Image.blend(frames[lower], frames[upper], fraction)


def put_frame(atlas: Image.Image, frame: int, image: Image.Image) -> None:
    column = frame % COLUMNS
    row = frame // COLUMNS
    atlas.alpha_composite(image, (column * CELL_SIZE, row * CELL_SIZE))


def build_atlas(conyard_source: Path,
                infantry_source: Path,
                reference_atlas: Path,
                output: Path) -> None:
    reference = Image.open(reference_atlas).convert("RGBA")
    if reference.size != (CELL_SIZE * 3, CELL_SIZE):
        raise ValueError(f"invalid infrastructure reference atlas size {reference.size}")
    infantry_reference = reference.crop((CELL_SIZE, 0, CELL_SIZE * 2, CELL_SIZE))
    conyard_reference = reference.crop((CELL_SIZE * 2, 0, CELL_SIZE * 3, CELL_SIZE))
    conyard = prepared_keyframes(conyard_source, conyard_reference)
    infantry = prepared_keyframes(infantry_source, infantry_reference)
    atlas = Image.new("RGBA", (COLUMNS * CELL_SIZE, ROWS * CELL_SIZE), (0, 0, 0, 0))

    for frame in range(CONYARD_FRAMES):
        put_frame(atlas, frame, interpolate(conyard, ping_pong_position(frame, CONYARD_FRAMES)))
    for phase in range(INFANTRY_FRAMES):
        put_frame(atlas,
                  CONYARD_FRAMES + phase,
                  interpolate(infantry, ping_pong_position(phase, INFANTRY_FRAMES)))

    if atlas.size != (COLUMNS * CELL_SIZE, ROWS * CELL_SIZE):
        raise ValueError(f"invalid activity atlas size {atlas.size}")
    for frame in range(CONYARD_FRAMES + INFANTRY_FRAMES):
        column = frame % COLUMNS
        row = frame // COLUMNS
        cell = atlas.crop((column * CELL_SIZE,
                           row * CELL_SIZE,
                           (column + 1) * CELL_SIZE,
                           (row + 1) * CELL_SIZE))
        visible_bounds(cell)
        if cell.getchannel("A").getextrema()[0] != 0:
            raise ValueError(f"activity frame {frame} has no transparent padding")
    output.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(output, format="PNG", optimize=True)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--conyard-source", required=True, type=Path)
    parser.add_argument("--infantry-source", required=True, type=Path)
    parser.add_argument("--reference-atlas", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    build_atlas(args.conyard_source,
                args.infantry_source,
                args.reference_atlas,
                args.output)


if __name__ == "__main__":
    main()
