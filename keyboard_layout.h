#ifndef KEYBOARD_LAYOUT_H
#define KEYBOARD_LAYOUT_H

// The keyboard is laid out against the screen rather than against the firmware
// font. The MinUI/NextUI SDK opens its fonts at SCALE1(FONT_*), a fixed multiple
// of the device's FIXED_SCALE, so a layout derived from the font tracks the
// scale factor but never the resolution: two devices with the same FIXED_SCALE
// and different panels end up with an identically sized keyboard, and the one
// with the larger panel wastes most of it (issue 54).
//
// This module owns that geometry. It is deliberately free of SDL and of the SDK
// headers so it can be compiled and asserted on a bare host, which is what
// tests/layout_probe.c and tests/layout.bats do.

// KEYBOARD_WIDTH_PERCENT is the share of the screen width the widest key row
// aims for. It is tuned so the my355 - the 640x480 device the current design is
// implicitly built around - resolves back to the key size it already has.
#define KEYBOARD_WIDTH_PERCENT 88

// KEYBOARD_MIN_KEY_SIZE keeps a pathologically small screen from producing a
// zero or negative key.
#define KEYBOARD_MIN_KEY_SIZE 8

// KeyboardLayoutInput describes the screen and the SDK metrics to lay out
// against. Every measurement is in device pixels, so the caller applies SCALE1()
// before filling this in.
struct KeyboardLayoutInput
{
    int screen_w;    // screen->w
    int screen_h;    // screen->h
    int padding;     // SCALE1(PADDING)
    int pill_size;   // SCALE1(PILL_SIZE)
    int gap;         // space between keys and between rows
    int rows;        // number of key rows
    int columns;     // number of keys in the widest row
    int reserve_top; // non-zero when a title or the hardware group occupies the top bar
};

// KeyboardLayout is the resolved geometry, in device pixels. grid_x and grid_w
// describe the widest row; shorter rows are centered inside it with
// keyboard_row_x.
struct KeyboardLayout
{
    int key_size;
    int gap;
    int input_x;
    int input_y;
    int input_w;
    int input_h;
    int grid_x;
    int grid_y;
    int grid_w;
    int grid_h;
    int title_y;
    int title_h;
};

// keyboard_layout resolves the geometry for a screen.
struct KeyboardLayout keyboard_layout(const struct KeyboardLayoutInput *in);

// keyboard_row_x centers a row of row_w pixels inside the key grid.
int keyboard_row_x(const struct KeyboardLayout *layout, int row_w);

#endif
