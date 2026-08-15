#!/usr/bin/env python3
"""Build a deterministic 3-by-1 Power/Barracks/Construction Yard atlas."""

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


CELL_SIZE = 256
FRAME_COUNT = 3


def build_atlas(source: Path, output: Path) -> None:
    sheet = clean_generated_background(Image.open(source))
    atlas = Image.new("RGBA", (CELL_SIZE * FRAME_COUNT, CELL_SIZE), (0, 0, 0, 0))

    # Runtime frame contract: Power Plant, infantry production building,
    # deployed Construction Yard. Each authored source occupies one equal slot.
    for frame in range(FRAME_COUNT):
        source_frame = retain_primary_component(equal_grid_cell(sheet, frame, FRAME_COUNT))
        prepared = composite_fitted(crop_visible(source_frame), CELL_SIZE, margin=6)
        atlas.alpha_composite(prepared, (frame * CELL_SIZE, 0))

    validate_atlas(atlas, FRAME_COUNT, CELL_SIZE, output)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    build_atlas(args.source, args.output)


if __name__ == "__main__":
    main()
