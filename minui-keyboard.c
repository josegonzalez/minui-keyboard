#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <msettings.h>
#include <argp.h>
#ifdef USE_SDL2
#include <SDL2/SDL_ttf.h>
#else
#include <SDL/SDL_ttf.h>
#endif

#include "defines.h"
#include "api.h"
#include "utils.h"

SDL_Surface *screen = NULL;

// log_error logs a message to stderr for debugging purposes
void log_error(const char *msg)
{
    // Set stderr to unbuffered mode
    setvbuf(stderr, NULL, _IONBF, 0);
    fprintf(stderr, "%s\n", msg);
}

// log_info logs a message to stdout for debugging purposes
void log_info(const char *msg)
{
    // Set stdout to unbuffered mode
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("%s\n", msg);
}

// keyboard_layout_lowercase is the default keyboard layout
const char *keyboard_layout_lowercase[5][14] = {
    {"`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "\0"},
    {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]", "\\", "\0"},
    {"a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'", "\0", "\0", "\0"},
    {"z", "x", "c", "v", "b", "n", "m", ",", ".", "/", "\0", "\0", "\0", "\0"},
    {"shift", "space", "enter", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0"}};

// keyboard_layout_uppercase is the uppercase keyboard layout
const char *keyboard_layout_uppercase[5][14] = {
    {"~", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+", "\0"},
    {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "{", "}", "|", "\0"},
    {"A", "S", "D", "F", "G", "H", "J", "K", "L", ":", "\"", "\0", "\0", "\0"},
    {"Z", "X", "C", "V", "B", "N", "M", "<", ">", "?", "\0", "\0", "\0", "\0"},
    {"shift", "space", "enter", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0"}};

// keyboard_layout_special is the special keyboard layout
// note that some characters are not supported by the font in use by MinUI
// so we omit those characters from the layout
const char *keyboard_layout_special[5][14] = {
    {"~", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+", "\0"},
    {"{", "}", "|", "\\", "<", ">", "?", "\"", ";", ":", "[", "]", "\\", "\0"},
    {"±", "§", "¶", "©", "®", "™", "€", "£", "¥", "¢", "¤", "\0", "\0", "\0"},
    {"°", "•", "·", "†", "‡", "¬", "¦", "¡", "\0", "\0", "\0", "\0", "\0", "\0"},
    {"shift", "space", "enter", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0"}};

// KeyboardState holds the keyboard-related state
struct KeyboardState
{
    bool display;            // whether to display the keyboard
    int row;                 // the current keyboard row
    int col;                 // the current keyboard column
    int layout;              // the current keyboard layout
    char current_text[1024]; // the text to display in the keyboard
    char initial_text[1024]; // the initial value of the text on keyboard entry
    char final_text[1024];   // the final value of the text on keyboard exit
    char title[1024];        // the title of the keyboard
};

// AppState holds the current state of the application
struct AppState
{
    int redraw;                    // whether the screen needs to be redrawn
    int quitting;                  // whether the app should exit
    int exit_code;                 // the exit code to return
    int show_hardware_status;      // whether to show the hardware status
    int show_brightness_setting;   // whether to show the brightness or hardware state
    struct KeyboardState keyboard; // current keyboard state
};

// max returns the maximum of two integers
int max(int a, int b)
{
    return (a > b) ? a : b;
}

// count_row_length returns the number of non-empty characters in a keyboard row
int count_row_length(const char *(*layout)[14], int row) {
    int length = 0;
    for (int i = 0; i < 14; i++) {
        if (layout[row][i][0] != '\0') {
            length++;
        }
    }
    return length;
}

// calculate_column_offset returns how much to adjust the column when moving between rows
int calculate_column_offset(const char *(*layout)[14], int from_row, int to_row) {
    int from_length = count_row_length(layout, from_row);
    int to_length = count_row_length(layout, to_row);
    return (to_length - from_length) / 2;
}

// handle_keyboard_input interprets keyboard input events and mutates app state
void handle_keyboard_input(struct AppState *state)
{
    // redraw unless a key was not pressed
    state->redraw = 1;

    // track current keyboard layout
    const char *(*current_layout)[14];
    if (state->keyboard.layout == 0)
    {
        current_layout = keyboard_layout_lowercase;
    }
    else if (state->keyboard.layout == 1)
    {
        current_layout = keyboard_layout_uppercase;
    }
    else
    {
        current_layout = keyboard_layout_special;
    }

    int max_row = 5;
    int max_col = sizeof(current_layout[0]) / sizeof(current_layout[0][0]);

    if (PAD_justRepeated(BTN_UP))
    {
        if (state->keyboard.row > 0)
        {
            state->keyboard.col += calculate_column_offset(current_layout, state->keyboard.row, state->keyboard.row - 1);
            state->keyboard.row--;
        }
        else
        {
            state->keyboard.col += calculate_column_offset(current_layout, 0, max_row - 1);
            state->keyboard.row = max_row - 1;
            while (state->keyboard.col >= 0 && current_layout[state->keyboard.row][state->keyboard.col][0] == '\0')
            {
                state->keyboard.col--;
            }
            if (state->keyboard.col < 0)
            {
                state->keyboard.col = 0;
                while (current_layout[state->keyboard.row][state->keyboard.col][0] == '\0')
                {
                    state->keyboard.col++;
                }
            }
        }
    }
    else if (PAD_justRepeated(BTN_DOWN))
    {
        if (state->keyboard.row < max_row - 1)
        {
            state->keyboard.col += calculate_column_offset(current_layout, state->keyboard.row, state->keyboard.row + 1);
            state->keyboard.row++;
            while (state->keyboard.col >= 0 && current_layout[state->keyboard.row][state->keyboard.col][0] == '\0')
            {
                state->keyboard.col--;
            }
            if (state->keyboard.col < 0)
            {
                state->keyboard.col = 0;
                while (current_layout[state->keyboard.row][state->keyboard.col][0] == '\0')
                {
                    state->keyboard.col++;
                }
            }
        }
        else
        {
            state->keyboard.col += calculate_column_offset(current_layout, max_row - 1, 0);
            state->keyboard.row = 0;
        }
    }
    else if (PAD_justRepeated(BTN_LEFT))
    {
        if (state->keyboard.col > 0)
        {
            state->keyboard.col--;
        }
        else
        {
            int last_col = 13;
            while (last_col >= 0 && current_layout[state->keyboard.row][last_col][0] == '\0')
            {
                last_col--;
            }
            state->keyboard.col = last_col;
        }
    }
    else if (PAD_justRepeated(BTN_RIGHT))
    {
        const char *next_key = current_layout[state->keyboard.row][state->keyboard.col + 1];
        if (state->keyboard.col + 1 >= max_col || *next_key == '\0')
        {
            state->keyboard.col = 0;
        }
        else if (state->keyboard.col < max_col - 1)
        {
            state->keyboard.col++;
        }
    }
    else if (PAD_justReleased(BTN_X))
    {
        strcpy(state->keyboard.final_text, state->keyboard.current_text);
        state->keyboard.display = !state->keyboard.display;
        state->redraw = 1;
        state->quitting = 1;
        state->exit_code = EXIT_SUCCESS;
    }
    else if (PAD_justReleased(BTN_B))
    {
        size_t len = strlen(state->keyboard.current_text);
        if (len > 0)
        {
            state->keyboard.current_text[len - 1] = '\0';
        }
    }
    else if (PAD_justReleased(BTN_A))
    {
        const char *key = current_layout[state->keyboard.row][state->keyboard.col];

        if (*key != '\0')
        {
            if (strcmp(key, "shift") == 0)
            {
                state->keyboard.layout = (state->keyboard.layout + 1) % 3;
            }
            else if (strcmp(key, "space") == 0)
            {
                strcat(state->keyboard.current_text, " ");
            }
            else if (strcmp(key, "enter") == 0)
            {
                strcpy(state->keyboard.final_text, state->keyboard.current_text);
                state->keyboard.display = !state->keyboard.display;
                state->quitting = 1;
                state->exit_code = EXIT_SUCCESS;
            }
            else
            {
                strcat(state->keyboard.current_text, key);
            }
        }
    }
    else if (PAD_justReleased(BTN_SELECT))
    {
        state->keyboard.layout = (state->keyboard.layout + 1) % 3;
    }
    else
    {
        // do not redraw if no key was pressed
        state->redraw = 0;
    }
}

// handle_input interprets input events and mutates app state
void handle_input(struct AppState *state)
{
    PAD_poll();

    if (PAD_justReleased(BTN_Y))
    {
        strcpy(state->keyboard.final_text, state->keyboard.initial_text);
        state->quitting = 1;
        state->exit_code = 2;
        state->redraw = 1;
    }

    if (PAD_justReleased(BTN_MENU))
    {
        strcpy(state->keyboard.final_text, state->keyboard.initial_text);
        state->redraw = 0;
        state->quitting = 1;
        state->exit_code = 3;
        return;
    }

    handle_keyboard_input(state);
}

// draw_keyboard interprets the app state and draws it as a keyboard to the screen
void draw_keyboard(SDL_Surface *screen, struct AppState *state)
{
    // determine which keyboard layout to use based on current state
    const char *(*current_layout)[14];
    if (state->keyboard.layout == 0)
    {
        current_layout = keyboard_layout_lowercase;
    }
    else if (state->keyboard.layout == 1)
    {
        current_layout = keyboard_layout_uppercase;
    }
    else
    {
        current_layout = keyboard_layout_special;
    }
    const char *key = current_layout[state->keyboard.row][state->keyboard.col];

    // draw the button group on the button-right
    GFX_blitButtonGroup((char *[]){"Y", "EXIT", "X", "ENTER", NULL}, 1, screen, 1);

    // draw keyboard title
    if (strlen(state->keyboard.title) > 0)
    {
        SDL_Surface *title = TTF_RenderUTF8_Blended(font.large, state->keyboard.title, COLOR_WHITE);
        SDL_Rect title_pos = {
            (screen->w - title->w) / 2, // center horizontally
            20,                         // 20px from top
            title->w,
            title->h};
        SDL_BlitSurface(title, NULL, screen, &title_pos);
        SDL_FreeSurface(title);
    }

    // draw input field with current text
    // todo: use TTF_SizeUTF8 to compute the width of the input field
    SDL_Surface *input_placeholder = TTF_RenderUTF8_Blended(font.medium, "p", COLOR_WHITE);
    SDL_Surface *input = TTF_RenderUTF8_Blended(font.medium, state->keyboard.current_text, COLOR_WHITE);
    SDL_Rect input_pos = {
        (screen->w) / 2,
        input_placeholder->h * 2,
        0,
        input_placeholder->h};
    if (input != NULL)
    {
        input_pos.x = (screen->w - input->w) / 2;
        input_pos.w = input->w;
        input_pos.h = input->h;
    }

    // draw input field background
    SDL_Rect input_bg = {
        40,
        input_placeholder->h * 2,
        screen->w - 80,
        input_placeholder->h};
    SDL_FillRect(screen, &input_bg, SDL_MapRGB(screen->format, TRIAD_DARK_GRAY));
    SDL_BlitSurface(input, NULL, screen, &input_pos);
    SDL_FreeSurface(input);

    // draw keyboard layout
    int start_y = input_placeholder->h * 4;
    int default_key_width = input_placeholder->w;
    int default_key_height = input_placeholder->h;
    int default_key_size = max(default_key_width, default_key_height);
    int row_spacing = 5;
    int column_spacing = 5;

    int num_rows = 5;
    int num_columns = 14;

    // these special keys are not the same width as the other keys
    // so we need to compute their width separately
    // compute them here to avoid doing it conditionally for each row
    int shift_width, space_width, enter_width;
    TTF_SizeUTF8(font.medium, "shift", &shift_width, NULL);
    TTF_SizeUTF8(font.medium, "space", &space_width, NULL);
    TTF_SizeUTF8(font.medium, "enter", &enter_width, NULL);
    int special_key_width = max(shift_width, max(space_width, enter_width)) + (column_spacing * 4);

    for (int row = 0; row < num_rows; row++)
    {
        int len = 0;

        // Count non-null characters in the row
        for (int i = 0; i < num_columns; i++)
        {
            if (current_layout[row][i][0] != '\0')
            {
                len++;
            }
        }

        int total_width = (len * default_key_size) + ((len - 1) * column_spacing); // 5px between keys
        if (row == 4)
        {
            // compute row 4 differently
            // row 4 has three buttons:
            // - "shift"
            // - "space"
            // - "enter"
            // so we need to account for the actual width of the buttons
            // we can use TTF_SizeUTF8 to get the width of the buttons
            // also we need to account for the padding between the keys
            // as well as the margin on each of the 3 keys
            total_width = (special_key_width * 3) + (2 * column_spacing);
        }
        int start_x = (screen->w - total_width) / 2;

        for (int col = 0; col < len; col++)
        {
            const char *key = current_layout[row][col];
            if (*key == '\0')
            {
                continue;
            }

            SDL_Color text_color = (row == state->keyboard.row && col == state->keyboard.col) ? COLOR_BLACK : COLOR_WHITE;
            SDL_Surface *key_text = TTF_RenderUTF8_Blended(font.medium, key, text_color);

            // special keys are not the same width as the other keys
            // so we need to compute their width separately
            int current_key_width = default_key_size;
            if (strcmp(key, "shift") == 0 || strcmp(key, "space") == 0 || strcmp(key, "enter") == 0)
            {
                current_key_width = special_key_width;
            }

            SDL_Rect key_pos = {
                start_x + (col * (current_key_width + column_spacing)),
                start_y + (row * (default_key_size + row_spacing)),
                current_key_width,
                default_key_size};

            // draw key background
            Uint32 bg_color = (row == state->keyboard.row && col == state->keyboard.col) ? SDL_MapRGB(screen->format, TRIAD_WHITE) : SDL_MapRGB(screen->format, TRIAD_DARK_GRAY);
            SDL_FillRect(screen, &key_pos, bg_color);

            // center text in key
            SDL_Rect text_pos = {
                key_pos.x + (current_key_width - key_text->w) / 2,
                key_pos.y + (default_key_size - key_text->h) / 2,
                key_text->w,
                key_text->h};

            SDL_BlitSurface(key_text, NULL, screen, &text_pos);
            SDL_FreeSurface(key_text);
        }
    }
}

// draw_screen interprets the app state and draws it to the screen
void draw_screen(SDL_Surface *screen, struct AppState *state)
{
    draw_keyboard(screen, state);

    // don't forget to reset the should_redraw flag
    state->redraw = 0;
}

// swallow_stdout_from_function swallows stdout from a function
// this is useful for suppressing output from a function
// that we don't want to see in the log file
// the InitSettings() function is an example of this (some implementations print to stdout)
void swallow_stdout_from_function(void (*func)(void))
{
    int original_stdout = dup(STDOUT_FILENO);
    int dev_null = open("/dev/null", O_WRONLY);

    dup2(dev_null, STDOUT_FILENO);
    close(dev_null);

    func();

    dup2(original_stdout, STDOUT_FILENO);
    close(original_stdout);
}

void signal_handler(int signal)
{
    // if the signal is a ctrl+c, exit with code 130
    if (signal == SIGINT)
    {
        exit(130);
    }
    else
    {
        exit(1);
    }
}

// parse_arguments parses the arguments using argp and updates the app state
// supports the following flags:
// - --header <title> (default: empty string)
// - --hardware-status <true|false> (default: true)
// - --initial-value <value> (default: empty string)
bool parse_arguments(struct AppState *state, int argc, char *argv[])
{
    struct argp_option options[] = {
        {"header", 'H', "TITLE", 0, "Set keyboard header/title", 0},
        {"hardware-status", 'S', "BOOL", 0, "Show hardware status (true/false)", 0},
        {"initial-value", 'I', "VALUE", 0, "Set initial keyboard value", 0},
        {0}};

    struct arguments
    {
        char *header;
        char *hardware_status;
        char *initial_value;
    };

    error_t parse_opt(int key, char *arg, struct argp_state *argp_state)
    {
        struct arguments *arguments = argp_state->input;

        switch (key)
        {
        case 'H':
            arguments->header = arg;
            break;
        case 'S':
            arguments->hardware_status = arg;
            break;
        case 'I':
            arguments->initial_value = arg;
            break;
        default:
            return ARGP_ERR_UNKNOWN;
        }
        return 0;
    }

    struct argp argp = {options, parse_opt, 0, 0};

    struct arguments arguments = {0};
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    // Apply parsed arguments to state
    if (arguments.header)
    {
        strncpy(state->keyboard.title, arguments.header, sizeof(state->keyboard.title) - 1);
    }

    if (arguments.hardware_status)
    {
        if (strcmp(arguments.hardware_status, "true") == 0)
        {
            state->show_hardware_status = 1;
        }
        else if (strcmp(arguments.hardware_status, "false") == 0)
        {
            state->show_hardware_status = 0;
        }
    }

    if (arguments.initial_value)
    {
        strncpy(state->keyboard.initial_text, arguments.initial_value, sizeof(state->keyboard.initial_text) - 1);
        strncpy(state->keyboard.current_text, arguments.initial_value, sizeof(state->keyboard.current_text) - 1);
    }

    return true;
}

// init initializes the app state
// everything is placed here as MinUI sometimes logs to stdout
// and the logging happens depending on the platform
void init()
{
    // set the cpu speed to the menu speed
    // this is done here to ensure we downclock
    // the menu (no need to draw power unnecessarily)
    PWR_setCPUSpeed(CPU_SPEED_MENU);

    // initialize:
    // - the screen, allowing us to draw to it
    // - input from the pad/joystick/buttons/etc.
    // - power management
    // - sync hardware settings (brightness, hdmi, speaker, etc.)
    if (screen == NULL)
    {
        screen = GFX_init(MODE_MAIN);
    }
    PAD_init();
    PWR_init();
    InitSettings();
}

// destruct cleans up the app state in reverse order
void destruct()
{
    QuitSettings();
    PWR_quit();
    PAD_quit();
    GFX_quit();
}

// main is the entry point for the app
int main(int argc, char *argv[])
{
    // swallow all stdout from init calls
    // MinUI will sometimes randomly log to stdout
    swallow_stdout_from_function(init);

    signal(SIGINT, signal_handler);

    // Initialize app state
    char default_keyboard_text[1024] = "";
    char default_keyboard_title[1024] = "";
    struct AppState state = {
        .redraw = 1,
        .quitting = 0,
        .exit_code = EXIT_SUCCESS,
        .show_brightness_setting = 0,
        .show_hardware_status = 1,
        .keyboard = {
            .display = true,
            .row = 0,
            .col = 0,
            .current_text = *default_keyboard_text,
            .initial_text = *default_keyboard_text,
            .final_text = *default_keyboard_text,
            .layout = 0,
            .title = *default_keyboard_title}};

    parse_arguments(&state, argc, argv);

    // get initial wifi state
    int was_online = PLAT_isOnline();

    // draw the screen at least once
    // handle_keyboard_input sets state.redraw to 0 if no key is pressed
    int was_ever_drawn = 0;
    while (!state.quitting)
    {
        // start the frame to ensure GFX_sync() works
        // on devices that don't support vsync
        GFX_startFrame();

        // handle turning the on/off screen on/off
        // as well as general power management
        PWR_update(&state.redraw, NULL, NULL, NULL);

        // check if the device is on wifi
        // redraw if the wifi state changed
        // and then update our state
        int is_online = PLAT_isOnline();
        if (was_online != is_online)
        {
            state.redraw = 1;
        }
        was_online = is_online;

        // handle any input events
        handle_input(&state);

        // force a redraw if the screen was never drawn
        if (!was_ever_drawn && !state.redraw)
        {
            state.redraw = 1;
            was_ever_drawn = 1;
        }

        // redraw the screen if there has been a change
        if (state.redraw)
        {
            // clear the screen at the beginning of each loop
            GFX_clear(screen);

            // optionally display hardware status
            if (state.show_hardware_status)
            {
                // draw the hardware information in the top-right
                GFX_blitHardwareGroup(screen, state.show_brightness_setting);

                // draw the setting hints
                if (state.show_brightness_setting)
                {
                    GFX_blitHardwareHints(screen, state.show_brightness_setting);
                }
            }

            // your draw logic goes here
            draw_screen(screen, &state);

            // Takes the screen buffer and displays it on the screen
            GFX_flip(screen);
        }
        else
        {
            // Slows down the frame rate to match the refresh rate of the screen
            // when the screen is not being redrawn
            GFX_sync();
        }
    }

    if (state.exit_code == EXIT_SUCCESS)
    {
        log_info(state.keyboard.final_text);
    }

    swallow_stdout_from_function(destruct);

    // exit the program
    return state.exit_code;
}