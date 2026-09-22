// layout_probe prints the geometry keyboard_layout() resolves for a screen, so
// tests/layout.bats can assert it the same way tests/makefile.bats asserts
// `make print-<VAR>`. It links only keyboard_layout.c, so it builds with the
// host compiler and needs neither a toolchain nor a cloned upstream tree.
//
//   layout-probe --screen 1280x720 --scale 2 --padding 10 [--pill 30]
//                [--gap 2] [--rows 5] [--columns 13] [--reserve-top]
//
// --scale multiplies --padding, --pill and --gap, mirroring the SDK's SCALE1().

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../keyboard_layout.h"

static int arg_int(const char *value, const char *flag)
{
    char *end = NULL;
    long parsed = strtol(value, &end, 10);
    if (end == value || *end != '\0')
    {
        fprintf(stderr, "invalid value for %s: %s\n", flag, value);
        exit(2);
    }
    return (int)parsed;
}

int main(int argc, char *argv[])
{
    int screen_w = 0;
    int screen_h = 0;
    int scale = 1;
    int padding = 10;
    int pill_size = 30;
    int gap = 2;
    int rows = 5;
    int columns = 13;
    int reserve_top = 0;

    for (int i = 1; i < argc; i++)
    {
        const char *flag = argv[i];
        int has_value = (i + 1 < argc);

        if (strcmp(flag, "--reserve-top") == 0)
        {
            reserve_top = 1;
            continue;
        }

        if (!has_value)
        {
            fprintf(stderr, "missing value for %s\n", flag);
            return 2;
        }
        const char *value = argv[++i];

        if (strcmp(flag, "--screen") == 0)
        {
            const char *x = strchr(value, 'x');
            if (x == NULL)
            {
                fprintf(stderr, "--screen wants WxH, got %s\n", value);
                return 2;
            }
            char width[32];
            size_t len = (size_t)(x - value);
            if (len >= sizeof(width))
            {
                fprintf(stderr, "--screen width too long: %s\n", value);
                return 2;
            }
            memcpy(width, value, len);
            width[len] = '\0';
            screen_w = arg_int(width, "--screen");
            screen_h = arg_int(x + 1, "--screen");
        }
        else if (strcmp(flag, "--scale") == 0)
        {
            scale = arg_int(value, flag);
        }
        else if (strcmp(flag, "--padding") == 0)
        {
            padding = arg_int(value, flag);
        }
        else if (strcmp(flag, "--pill") == 0)
        {
            pill_size = arg_int(value, flag);
        }
        else if (strcmp(flag, "--gap") == 0)
        {
            gap = arg_int(value, flag);
        }
        else if (strcmp(flag, "--rows") == 0)
        {
            rows = arg_int(value, flag);
        }
        else if (strcmp(flag, "--columns") == 0)
        {
            columns = arg_int(value, flag);
        }
        else
        {
            fprintf(stderr, "unknown flag %s\n", flag);
            return 2;
        }
    }

    if (screen_w <= 0 || screen_h <= 0)
    {
        fprintf(stderr, "--screen WxH is required\n");
        return 2;
    }

    struct KeyboardLayoutInput in = {
        .screen_w = screen_w,
        .screen_h = screen_h,
        .padding = padding * scale,
        .pill_size = pill_size * scale,
        .gap = gap * scale,
        .rows = rows,
        .columns = columns,
        .reserve_top = reserve_top};

    struct KeyboardLayout layout = keyboard_layout(&in);

    printf("key_size=%d\n", layout.key_size);
    printf("gap=%d\n", layout.gap);
    printf("input_x=%d\n", layout.input_x);
    printf("input_y=%d\n", layout.input_y);
    printf("input_w=%d\n", layout.input_w);
    printf("input_h=%d\n", layout.input_h);
    printf("grid_x=%d\n", layout.grid_x);
    printf("grid_y=%d\n", layout.grid_y);
    printf("grid_w=%d\n", layout.grid_w);
    printf("grid_h=%d\n", layout.grid_h);
    printf("grid_bottom=%d\n", layout.grid_y + layout.grid_h);
    printf("hints_top=%d\n", screen_h - in.padding - in.pill_size);
    printf("title_y=%d\n", layout.title_y);
    printf("title_h=%d\n", layout.title_h);
    printf("row_x_3=%d\n", keyboard_row_x(&layout, 3 * layout.key_size + 2 * layout.gap));

    return 0;
}
