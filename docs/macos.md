# macOS Build

This document describes how to build and run minui-keyboard natively on macOS.

## Prerequisites

Install SDL2 dependencies via Homebrew:

```bash
brew install sdl2 sdl2_image sdl2_ttf pkg-config
```

## Building

```bash
# Build for macOS
PLATFORM=macos make

# Set up the resource directory (required for running)
PLATFORM=macos make setup-resources
```

## Running

```bash
./minui-keyboard-macos
```

The application requires MinUI resources to be present at `/tmp/FAKESD/.system/res/`. The `setup-resources` target copies these automatically from the MinUI repository, including every asset scale, since the emulated panel is chosen at runtime.

## Emulating another device

The macOS build exists to develop against, so it does not hardcode a panel. `FIXED_WIDTH`, `FIXED_HEIGHT`, `FIXED_SCALE` and `PADDING` are runtime values read from the environment, which is how the keyboard layout gets checked at a device geometry you do not have to hand. Upstream does the same thing on the tg5040, where those macros read the runtime `is_brick` flag.

| Variable | Default | Matches |
|---|---|---|
| `MINUI_WIDTH` | 640 | `FIXED_WIDTH` |
| `MINUI_HEIGHT` | 480 | `FIXED_HEIGHT` |
| `MINUI_SCALE` | 2 | `FIXED_SCALE` |
| `MINUI_PADDING` | 10 | `PADDING` |

The defaults are the 640x480 device this build has always emulated. The window is 800px wide and takes its height from the emulated panel's aspect ratio.

```bash
# my355 / rg35xxplus / zero28
MINUI_WIDTH=640  MINUI_HEIGHT=480 MINUI_SCALE=2 MINUI_PADDING=10 ./minui-keyboard-macos

# tg5040 Brick
MINUI_WIDTH=1024 MINUI_HEIGHT=768 MINUI_SCALE=3 MINUI_PADDING=5  ./minui-keyboard-macos

# tg5050
MINUI_WIDTH=1280 MINUI_HEIGHT=720 MINUI_SCALE=2 MINUI_PADDING=10 ./minui-keyboard-macos

# tg5040 Smart Pro
MINUI_WIDTH=1280 MINUI_HEIGHT=720 MINUI_SCALE=2 MINUI_PADDING=40 ./minui-keyboard-macos

# trimuismart
MINUI_WIDTH=320  MINUI_HEIGHT=240 MINUI_SCALE=1 MINUI_PADDING=10 ./minui-keyboard-macos
```

Each device's values live in its `platform.h` in the upstream tree, under `minui/workspace/<device>/platform/`.

## Capturing a frame

`MINUI_SCREENSHOT` writes every frame to a BMP at the emulated panel's own resolution, overwriting it as it goes, so the last one written is the frame that was on screen. The window itself is scaled for the desktop, so this is the only way to get a capture at device resolution.

```bash
MINUI_WIDTH=1280 MINUI_HEIGHT=720 MINUI_SCALE=2 MINUI_PADDING=10 \
  MINUI_SCREENSHOT=/tmp/keyboard.bmp ./minui-keyboard-macos
sips -s format png /tmp/keyboard.bmp --out /tmp/keyboard.png
```

## Keyboard Mappings

The following keyboard keys are mapped to controller buttons:

| Button | Keyboard Key |
|--------|--------------|
| D-Pad Up | Arrow Up |
| D-Pad Down | Arrow Down |
| D-Pad Left | Arrow Left |
| D-Pad Right | Arrow Right |
| A | S |
| B | A |
| X | W |
| Y | Q |
| Start | Enter |
| Select | ' (apostrophe) |
| Menu | Space |
| Power | Backspace |

The L1/L2/R1/R2/L3/R3 and Plus/Minus buttons are not mapped.

## Quitting

Use **Cmd+Q** to quit the application.

## Window

The application opens an 800px-wide resizable window whose height follows the emulated panel's aspect ratio, so the default 640x480 panel gives the 800x600 window this build has always used.
