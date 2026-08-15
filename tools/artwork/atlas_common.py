#!/usr/bin/env python3
"""Shared deterministic helpers for generated Modern Art sprite sheets."""

from __future__ import annotations

from collections import deque
from pathlib import Path
from statistics import median

from PIL import Image


ALPHA_CROP_THRESHOLD = 8


def foundation_metrics(
    image: Image.Image,
    *,
    lower_fraction: float = 0.38,
    threshold: int = 24,
) -> tuple[float, float, int]:
    """Return width, horizontal centre and bottom of a stable ground region.

    Full-sprite alpha bounds are unsuitable for animated isometric artwork:
    a crane, door, weapon or antenna is allowed to move.  The lower portion is
    the stable footprint and therefore the right registration reference.
    """
    left, top, right, bottom = visible_bounds(image)
    base_top = bottom - round((bottom - top) * lower_fraction)
    base_mask = image.getchannel("A").crop((0, base_top, image.width, bottom))
    base_bounds = base_mask.point(lambda value: 255 if value >= threshold else 0).getbbox()
    if base_bounds is None:
        raise ValueError("animation frame has no stable foundation pixels")
    base_left, _, base_right, base_bottom = base_bounds
    return (float(base_right - base_left),
            (base_left + base_right) * 0.5,
            base_bottom + base_top)


def register_foundation_keyframes(
    frames: list[Image.Image],
    cell_size: int,
    *,
    margin: int,
    reference: Image.Image | None = None,
) -> list[Image.Image]:
    """Give generated keyframes one scale and a fixed ground anchor.

    Image generators often change apparent size or framing between authored
    keyframes.  Each frame is first corrected to the sequence's median
    footprint.  The entire sequence is then fitted once.  When ``reference``
    is supplied, the animation also inherits the static sprite's footprint,
    centre and baseline so entering or leaving an animation cannot pop.
    """
    if not frames:
        raise ValueError("animation sequence contains no keyframes")

    records = []
    for frame in frames:
        left, top, right, bottom = visible_bounds(frame)
        base_width, base_center, base_bottom = foundation_metrics(frame)
        crop = (max(0, left - 2),
                max(0, top - 2),
                min(frame.width, right + 2),
                min(frame.height, bottom + 2))
        records.append((frame, crop, base_width, base_center, base_bottom))

    median_base_width = float(median(record[2] for record in records))
    geometry = []
    max_left = max_right = max_top = max_bottom = 0.0
    for frame, crop, base_width, base_center, base_bottom in records:
        correction = median_base_width / max(1.0, base_width)
        crop_width = crop[2] - crop[0]
        crop_height = crop[3] - crop[1]
        anchor_x = (base_center - crop[0]) * correction
        anchor_y = (base_bottom - crop[1]) * correction
        width = crop_width * correction
        height = crop_height * correction
        max_left = max(max_left, anchor_x)
        max_right = max(max_right, width - anchor_x)
        max_top = max(max_top, anchor_y)
        max_bottom = max(max_bottom, height - anchor_y)
        geometry.append((frame, crop, correction, anchor_x, anchor_y))

    if reference is None:
        available = cell_size - margin * 2
        scale = min(available / (max_left + max_right),
                    available / (max_top + max_bottom))
        output_anchor_x = (cell_size - (max_left + max_right) * scale) * 0.5 + max_left * scale
        output_anchor_y = cell_size - margin - max_bottom * scale
    else:
        reference_width, output_anchor_x, output_anchor_y = foundation_metrics(reference)
        reference_scale = reference_width / median_base_width
        # Generated activity art can be taller than its static counterpart
        # (for example, a raised crane or the Hand of Nod spire). Keep the
        # reference's centre and baseline, then uniformly reduce the *whole*
        # sequence only when matching its footprint would clip a keyframe.
        # Crucially this is one scale for every phase, never a per-frame fit.
        padding = 1.0
        fit_scale = min((output_anchor_x - padding) / max_left,
                        (cell_size - padding - output_anchor_x) / max_right,
                        (output_anchor_y - padding) / max_top,
                        (cell_size - padding - output_anchor_y) / max_bottom)
        scale = min(reference_scale, fit_scale)
        if scale <= 0:
            raise ValueError("registered animation does not fit the reference sprite cell")

    output: list[Image.Image] = []
    for frame, crop, correction, anchor_x, anchor_y in geometry:
        artwork = frame.crop(crop)
        final_scale = correction * scale
        size = (max(1, round(artwork.width * final_scale)),
                max(1, round(artwork.height * final_scale)))
        resized = artwork.resize(size, Image.Resampling.LANCZOS)
        cell = Image.new("RGBA", (cell_size, cell_size), (0, 0, 0, 0))
        cell.alpha_composite(resized, (round(output_anchor_x - anchor_x * scale),
                                       round(output_anchor_y - anchor_y * scale)))
        output.append(cell)

    widths, centres, bottoms = zip(*(foundation_metrics(frame) for frame in output))
    if max(widths) - min(widths) > 1.0:
        raise ValueError("registered animation foundation width still changes")
    if max(centres) - min(centres) > 1.0:
        raise ValueError("registered animation horizontal anchor still changes")
    if max(bottoms) - min(bottoms) > 1:
        raise ValueError("registered animation baseline still changes")
    return output


def equal_grid_cell(sheet: Image.Image, index: int, count: int) -> Image.Image:
    """Return one horizontal slot without accumulating fractional-pixel error."""
    left = round(index * sheet.width / count)
    right = round((index + 1) * sheet.width / count)
    return sheet.crop((left, 0, right, sheet.height))


def _remove_opaque_dark_matte(image: Image.Image) -> Image.Image:
    """Flood a neutral black matte only when a source has no useful alpha.

    Image generators occasionally return an opaque black canvas. Restricting
    the flood to dark, nearly neutral pixels connected to an outer edge avoids
    deleting dark vehicle/body paint in the middle of the frame. Production
    inputs normally carry generated alpha, which is preferred because it also
    preserves their soft contact shadows.
    """
    width, height = image.size
    pixels = image.load()
    seen = bytearray(width * height)
    queue: deque[tuple[int, int]] = deque()

    def candidate(x: int, y: int) -> bool:
        red, green, blue, _ = pixels[x, y]
        return max(red, green, blue) <= 48 and max(red, green, blue) - min(red, green, blue) <= 14

    for x in range(width):
        if candidate(x, 0):
            queue.append((x, 0))
        if candidate(x, height - 1):
            queue.append((x, height - 1))
    for y in range(height):
        if candidate(0, y):
            queue.append((0, y))
        if candidate(width - 1, y):
            queue.append((width - 1, y))

    while queue:
        x, y = queue.popleft()
        offset = y * width + x
        if seen[offset] or not candidate(x, y):
            continue
        seen[offset] = 1
        pixels[x, y] = (0, 0, 0, 0)
        if x:
            queue.append((x - 1, y))
        if x + 1 < width:
            queue.append((x + 1, y))
        if y:
            queue.append((x, y - 1))
        if y + 1 < height:
            queue.append((x, y + 1))
    return image


def _remove_opaque_light_matte(image: Image.Image) -> Image.Image:
    """Flood an opaque neutral white/checker matte from the outer edges.

    Some image-generation exports visually preview transparency as a baked
    light checkerboard.  The building itself is isolated from the sheet edge,
    so an edge-connected flood removes the preview without erasing enclosed
    light concrete, lamps or metallic highlights.
    """
    width, height = image.size
    pixels = image.load()
    seen = bytearray(width * height)
    queue: deque[tuple[int, int]] = deque()

    def candidate(x: int, y: int) -> bool:
        red, green, blue, _ = pixels[x, y]
        return min(red, green, blue) >= 235 and max(red, green, blue) - min(red, green, blue) <= 8

    for x in range(width):
        if candidate(x, 0):
            queue.append((x, 0))
        if candidate(x, height - 1):
            queue.append((x, height - 1))
    for y in range(height):
        if candidate(0, y):
            queue.append((0, y))
        if candidate(width - 1, y):
            queue.append((width - 1, y))

    while queue:
        x, y = queue.popleft()
        offset = y * width + x
        if seen[offset] or not candidate(x, y):
            continue
        seen[offset] = 1
        pixels[x, y] = (0, 0, 0, 0)
        if x:
            queue.append((x - 1, y))
        if x + 1 < width:
            queue.append((x + 1, y))
        if y:
            queue.append((x, y - 1))
        if y + 1 < height:
            queue.append((x, y + 1))

    # Baked checker previews contain isolated light squares separated by
    # one-pixel grid seams, so edge connectivity alone cannot reach every
    # background island. Their very bright neutral palette is narrower than
    # concrete or metal highlights; remove the remaining matching pixels.
    for y in range(height):
        for x in range(width):
            if candidate(x, y):
                pixels[x, y] = (0, 0, 0, 0)
    return image


def clean_generated_background(source: Image.Image) -> Image.Image:
    """Use generated alpha, remove invisible dark RGB, and normalise opacity."""
    rgba = source.convert("RGBA")
    alpha = rgba.getchannel("A")
    minimum, maximum = alpha.getextrema()

    if minimum == maximum:
        if maximum == 0:
            raise ValueError("source sheet is completely transparent")
        corners = [rgba.getpixel((0, 0)),
                   rgba.getpixel((rgba.width - 1, 0)),
                   rgba.getpixel((0, rgba.height - 1)),
                   rgba.getpixel((rgba.width - 1, rgba.height - 1))]
        corner_luminance = sum(red + green + blue for red, green, blue, _ in corners) / 12.0
        rgba = (_remove_opaque_light_matte(rgba)
                if corner_luminance >= 160.0
                else _remove_opaque_dark_matte(rgba))
        alpha = rgba.getchannel("A")
        minimum, maximum = alpha.getextrema()

    if minimum == maximum:
        raise ValueError("source has no separable transparent/dark background")

    # Generated PNGs often top out at alpha 252-254. Make their solid artwork
    # genuinely opaque without flattening the lower-alpha contact shadows.
    scale = 255.0 / maximum
    output: list[tuple[int, int, int, int]] = []
    data = rgba.get_flattened_data() if hasattr(rgba, "get_flattened_data") else rgba.getdata()
    for red, green, blue, source_alpha in data:
        if source_alpha <= 2:
            output.append((0, 0, 0, 0))
        else:
            output.append((red, green, blue, min(255, round(source_alpha * scale))))
    rgba.putdata(output)
    return rgba


def visible_bounds(image: Image.Image) -> tuple[int, int, int, int]:
    alpha = image.getchannel("A")
    mask = alpha.point(lambda value: 255 if value >= ALPHA_CROP_THRESHOLD else 0)
    bounds = mask.getbbox()
    if bounds is None:
        raise ValueError("frame contains no visible pixels")
    return bounds


def retain_primary_component(image: Image.Image) -> Image.Image:
    """Discard neighbouring-slot fragments while retaining the authored object.

    Generated horizontal sheets can let a neighbouring object bleed a few
    pixels across an equal slot boundary. The intended building/vehicle and
    its contact shadow form the largest 8-connected alpha component.
    """
    width, height = image.size
    alpha = image.getchannel("A")
    values = alpha.get_flattened_data() if hasattr(alpha, "get_flattened_data") else alpha.getdata()
    occupied = bytes(1 if value >= ALPHA_CROP_THRESHOLD else 0 for value in values)
    seen = bytearray(width * height)
    largest: list[int] = []

    for start, present in enumerate(occupied):
        if not present or seen[start]:
            continue
        component: list[int] = []
        queue = [start]
        seen[start] = 1
        while queue:
            offset = queue.pop()
            component.append(offset)
            x = offset % width
            y = offset // width
            for neighbour_y in range(max(0, y - 1), min(height, y + 2)):
                row = neighbour_y * width
                for neighbour_x in range(max(0, x - 1), min(width, x + 2)):
                    neighbour = row + neighbour_x
                    if occupied[neighbour] and not seen[neighbour]:
                        seen[neighbour] = 1
                        queue.append(neighbour)
        if len(component) > len(largest):
            largest = component

    if not largest:
        raise ValueError("frame contains no primary alpha component")

    keep = bytearray(width * height)
    for offset in largest:
        keep[offset] = 1
    source = list(image.get_flattened_data() if hasattr(image, "get_flattened_data") else image.getdata())
    image.putdata([pixel if keep[index] else (0, 0, 0, 0) for index, pixel in enumerate(source)])
    return image


def crop_visible(image: Image.Image, padding: int = 2) -> Image.Image:
    left, top, right, bottom = visible_bounds(image)
    return image.crop(
        (
            max(0, left - padding),
            max(0, top - padding),
            min(image.width, right + padding),
            min(image.height, bottom + padding),
        )
    )


def composite_fitted(
    image: Image.Image,
    cell_size: int,
    *,
    margin: int,
    scale: float | None = None,
) -> Image.Image:
    available = cell_size - margin * 2
    if scale is None:
        scale = min(available / image.width, available / image.height)
    size = (max(1, round(image.width * scale)), max(1, round(image.height * scale)))
    if size[0] > available or size[1] > available:
        raise ValueError(f"frame {size} does not fit {cell_size}px cell with {margin}px margin")
    resized = image.resize(size, Image.Resampling.LANCZOS)
    cell = Image.new("RGBA", (cell_size, cell_size), (0, 0, 0, 0))
    cell.alpha_composite(resized, ((cell_size - size[0]) // 2, (cell_size - size[1]) // 2))
    return cell


def validate_atlas(atlas: Image.Image, columns: int, cell_size: int, output: Path) -> None:
    expected_size = (columns * cell_size, cell_size)
    if atlas.mode != "RGBA" or atlas.size != expected_size:
        raise ValueError(f"invalid atlas format: got {atlas.mode} {atlas.size}, expected RGBA {expected_size}")
    for frame in range(columns):
        cell = atlas.crop((frame * cell_size, 0, (frame + 1) * cell_size, cell_size))
        bounds = visible_bounds(cell)
        if bounds[2] - bounds[0] < 8 or bounds[3] - bounds[1] < 8:
            raise ValueError(f"atlas frame {frame} is unexpectedly empty")
        if cell.getchannel("A").getextrema()[0] != 0:
            raise ValueError(f"atlas frame {frame} has no transparent padding")
    output.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(output, format="PNG", optimize=True)
