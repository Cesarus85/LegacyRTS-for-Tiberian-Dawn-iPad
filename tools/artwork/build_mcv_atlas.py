#!/usr/bin/env python3
"""Build an eight-facing MCV atlas from five camera-fixed source views."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image

from atlas_common import (
    clean_generated_background,
    composite_fitted,
    crop_visible,
    equal_grid_cell,
    retain_primary_component,
    validate_atlas,
)


CELL_SIZE = 192
SOURCE_COUNT = 5
FACING_COUNT = 8
MARGIN = 5


def build_atlas(source: Path, output: Path) -> None:
    sheet = clean_generated_background(Image.open(source))
    authored = [
        crop_visible(retain_primary_component(equal_grid_cell(sheet, frame, SOURCE_COUNT)))
        for frame in range(SOURCE_COUNT)
    ]

    # A shared scale keeps the vehicle from pulsing when it turns. Narrower
    # front/rear views remain narrower than the broad side view by design.
    available = CELL_SIZE - MARGIN * 2
    scale = min(
        available / max(frame.width for frame in authored),
        available / max(frame.height for frame in authored),
    )
    # The accepted source sheets are ordered as requested, but visual audit of
    # the cab/front shows that the generated camera interpretation is the
    # opposite world heading: S, SW, W, NW, N. Normalise that source-specific
    # convention here so the public atlas contract remains the engine order
    # N, NE, E, SE, S, SW, W, NW.
    facing_sources = (
        (4, False),  # N
        (3, True),   # NE
        (2, True),   # E
        (1, True),   # SE
        (0, False),  # S
        (1, False),  # SW
        (2, False),  # W
        (3, False),  # NW
    )
    atlas = Image.new("RGBA", (CELL_SIZE * FACING_COUNT, CELL_SIZE), (0, 0, 0, 0))
    for facing, (source_index, mirrored) in enumerate(facing_sources):
        frame = authored[source_index]
        if mirrored:
            frame = frame.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        prepared = composite_fitted(frame, CELL_SIZE, margin=MARGIN, scale=scale)
        atlas.alpha_composite(prepared, (facing * CELL_SIZE, 0))

    validate_atlas(atlas, FACING_COUNT, CELL_SIZE, output)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    build_atlas(args.source, args.output)


if __name__ == "__main__":
    main()
