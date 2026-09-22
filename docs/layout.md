# Keyboard layout

The keyboard is laid out against the screen, not against the firmware font. This document covers why,
how the geometry is computed, and how to check it.

## Why the font is the wrong thing to scale from

The MinUI and NextUI SDKs open their fonts at `SCALE1(FONT_*)`, which is a fixed multiple of the
device's `FIXED_SCALE`. `FIXED_SCALE` exists to keep UI elements a consistent *physical* size, so it
tracks pixel density rather than panel size.

A layout derived from the font therefore follows the scale factor but never the resolution. Two devices
with the same `FIXED_SCALE` and different panels end up with an identically sized keyboard, and the one
with the larger panel wastes most of its screen. That is what issue 54 reported on the tg5050, where the
keyboard covered 45% of the width, against 89% on the my355.

| Device | FIXED_SCALE | Resolution | PADDING | key before | grid before | key after | grid after |
|---|---|---|---|---|---|---|---|
| my355 | 2 | 640x480 | 10 | 39 | 567 (88.6%) | 39 | 555 (86.7%) |
| tg5040 Brick | 3 | 1024x768 | 5 | 58 | 814 (79.5%) | 63 | 891 (87.0%) |
| tg5050 | 2 | 1280x720 | 10 | 40 | 580 (45.3%) | 82 | 1114 (87.0%) |
| tg5040 Smart Pro | 2 | 1280x720 | 40 | ~39 | ~567 (~44%) | 73 | 997 (77.9%) |
| trimuismart | 1 | 320x240 | 10 | ~19 | ~307 (~96%) | 19 | 271 (84.7%) |
| m17 | 1 | 480x273 | 10 | ~19 | ~307 (~64%) | 30 | 414 (86.3%) |

The before columns for my355, the Brick and the tg5050 were measured off the screenshots attached to
issue 54; the rest are extrapolated from those line heights. `FIXED_WIDTH` and `FIXED_HEIGHT` are runtime
values on some devices - the h700 resolves them from the panel or the attached HDMI mode - so the
geometry is computed at draw time from `screen->w` and `screen->h`, never at compile time.

## The geometry

`keyboard_layout.c` owns the whole calculation. It has no SDL or SDK dependency: the caller applies
`SCALE1()` and hands it plain pixels, which is what makes it testable on a bare host.

The keyboard lives between the two bands the SDK reserves. `GFX_blitButtonGroup` draws the button hints
at `screen_h - SCALE1(PADDING + PILL_SIZE)`, and `GFX_blitHardwareGroup` draws into the mirror image of
that band at the top, which the keyboard title also uses. So:

```
top      = reserve_top ? padding + pill_size : padding
bottom   = screen_h - padding - pill_size
region_h = bottom - top
```

Keys are squares, sized to whichever of the two budgets is tighter:

```
budget_w  = min(screen_w * KEYBOARD_WIDTH_PERCENT / 100, screen_w - 2 * padding)
key_w_fit = (budget_w - (columns - 1) * gap) / columns
key_h_fit = (2 * region_h - 2 * (rows + 1) * gap) / (2 * rows + 3)
key_size  = max(KEYBOARD_MIN_KEY_SIZE, min(key_w_fit, key_h_fit))
```

`key_h_fit` is the height budget solved for `key_size`. The block is the input field, a separator and
the key grid, where `input_h` is `key_size + 2*gap`, the separator is `key_size / 2` and `grid_h` is
`rows*key_size + (rows-1)*gap`; the whole thing is doubled so the halved separator stays in integer
arithmetic.

The block is then centered in the region, so a tall screen does not leave a dead band under the
keyboard, and the grid is centered horizontally. Rows shorter than the widest one are centered inside
the grid by `keyboard_row_x`.

### The constants

- `KEYBOARD_WIDTH_PERCENT` is 88. It is picked so the my355 - the 640x480 device the previous design was
  implicitly tuned to, and the one whose screenshot looks right - resolves back to the key size it
  already had. Every other device is then pulled to the same proportion.
- `KEYBOARD_KEY_GAP` is 2, scaled, replacing a flat unscaled 5px. That lands at 4 on a scale-2 device and
  6 on the Brick.
- `KEYBOARD_MIN_KEY_SIZE` is 8, which only matters on a screen too short to hold a keyboard at all.

The tg5040 Smart Pro is the one supported device where the height budget binds rather than the width:
its `PADDING` of 40 reserves 80 scaled pixels at the top and bottom of every MinUI screen, which the
keyboard has to stay clear of.

## The key font

Sizing the keys off the screen only helps if the glyphs grow with them, and the SDK's `font.*` cannot
follow the screen. `minui-keyboard.c` therefore opens its own face at

```
SCALE1(FONT_MEDIUM) * key_size / TTF_FontHeight(font.medium)
```

which gives a line height matching `key_size`. The factor is measured off the SDK's own medium font
rather than hardcoded, so a key box keeps the relationship to its glyph it has always had, and the my355
resolves back to `SCALE1(FONT_MEDIUM)` itself.

The face comes from the user's themed font under NextUI (`RES_PATH "/" CFG_getFontFile()`, with
`CFG_getFontStyle()`) and from `FONT_PATH` otherwise, behind the same `PLATFORM_NEXTUI` gate the theme
helpers use. It is cached and reopened only when the computed size changes, which matters on devices
whose resolution can change while running, and falls back to `font.medium` if it cannot be opened.

## Checking it

The bats suite asserts the resolved geometry per device. It builds `tests/layout_probe.c` against
`keyboard_layout.c` with the host compiler, so it needs neither a toolchain nor a cloned upstream tree:

```bash
make test
```

The probe is also useful on its own. It takes the panel and the SDK metrics and prints what the layout
resolves to, in the spirit of the `make print-<VAR>` target the build-wiring tests use:

```bash
cc -o layout-probe tests/layout_probe.c keyboard_layout.c
./layout-probe --screen 1280x720 --scale 2 --padding 10
```

`--pill`, `--gap`, `--rows`, `--columns` and `--reserve-top` cover the rest of the inputs. `--scale`
multiplies `--padding`, `--pill` and `--gap`, mirroring `SCALE1()`.

To see the result rather than the numbers, the macOS build emulates any panel. See
[macos.md](macos.md#emulating-another-device).
