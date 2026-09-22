#include "keyboard_layout.h"

static int layout_min(int a, int b)
{
    return (a < b) ? a : b;
}

static int layout_max(int a, int b)
{
    return (a > b) ? a : b;
}

struct KeyboardLayout keyboard_layout(const struct KeyboardLayoutInput *in)
{
    struct KeyboardLayout layout = {0};

    int rows = layout_max(1, in->rows);
    int columns = layout_max(1, in->columns);
    int gap = layout_max(0, in->gap);

    // The SDK draws the button hints in a pill at
    // screen_h - SCALE1(PADDING + PILL_SIZE), and the hardware group in the
    // mirror image of that band at the top, so the keyboard lives between them.
    int top = in->reserve_top ? in->padding + in->pill_size : in->padding;
    int bottom = in->screen_h - in->padding - in->pill_size;
    int region_h = bottom - top;

    // Width budget: a fixed share of the screen, never closer to either edge
    // than the SDK's own content padding.
    int budget_w = layout_min(in->screen_w * KEYBOARD_WIDTH_PERCENT / 100,
                              in->screen_w - 2 * in->padding);
    int key_w_fit = (budget_w - (columns - 1) * gap) / columns;

    // Height budget: the block is the input field, a separator, and the key
    // grid, where input_h is key_size + 2*gap, the separator is key_size/2 and
    // grid_h is rows*key_size + (rows-1)*gap. Doubling that to keep the halved
    // separator in integer arithmetic and solving for key_size gives:
    int key_h_fit = (2 * region_h - 2 * (rows + 1) * gap) / (2 * rows + 3);

    int key_size = layout_max(KEYBOARD_MIN_KEY_SIZE, layout_min(key_w_fit, key_h_fit));

    layout.key_size = key_size;
    layout.gap = gap;

    layout.grid_w = columns * key_size + (columns - 1) * gap;
    layout.grid_h = rows * key_size + (rows - 1) * gap;
    layout.grid_x = (in->screen_w - layout.grid_w) / 2;

    layout.input_w = layout.grid_w;
    layout.input_h = key_size + 2 * gap;
    layout.input_x = layout.grid_x;

    // Separate the input field from the grid by half a key, so the two read as
    // distinct blocks at any size, then center the whole thing in the region
    // rather than hanging it off the top, so a tall screen does not leave a
    // dead band under the keyboard.
    int separator = key_size / 2;
    int block_h = layout.input_h + separator + layout.grid_h;
    layout.input_y = top + layout_max(0, (region_h - block_h) / 2);
    layout.grid_y = layout.input_y + layout.input_h + separator;

    layout.title_y = in->padding;
    layout.title_h = in->pill_size;

    return layout;
}

int keyboard_row_x(const struct KeyboardLayout *layout, int row_w)
{
    return layout->grid_x + (layout->grid_w - row_w) / 2;
}
