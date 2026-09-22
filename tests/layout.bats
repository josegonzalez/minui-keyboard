#!/usr/bin/env bats
#
# Geometry tests for the keyboard layout.
#
# These assert the pixel geometry keyboard_layout() resolves for each supported
# panel (issue 54). They build tests/layout_probe.c against keyboard_layout.c
# with the host compiler, so they need neither a cross toolchain nor a cloned
# upstream tree and run on any host with cc.
#
# The pre-fix values quoted below were measured off the screenshots attached to
# issue 54 by scanning them for the key-box pixel bands.

setup_file() {
    REPO_ROOT="$(cd "$(dirname "$BATS_TEST_FILENAME")/.." && pwd)"
    export REPO_ROOT
    export PROBE="$BATS_FILE_TMPDIR/layout-probe"
    "${CC:-cc}" -std=gnu99 -Wall -Wextra -o "$PROBE" "$REPO_ROOT/tests/layout_probe.c" "$REPO_ROOT/keyboard_layout.c"
}

# probe <WxH> <scale> <padding> [extra flags]; sets $status/$output via bats `run`.
probe() {
    local screen="$1" scale="$2" padding="$3"
    shift 3
    run "$PROBE" --screen "$screen" --scale "$scale" --padding "$padding" "$@"
    [ "$status" -eq 0 ]
}

# field <name> echoes one value out of the probe output in $output.
field() { # <name>
    printf '%s\n' "$output" | sed -n "s/^$1=//p"
}

# the devices the issue is about: 1280x720 at scale 2, where the pre-fix
# keyboard was 580px wide, 45% of the screen

@test "tg5050 fills the screen width instead of 45% of it" {
    probe 1280x720 2 10
    [ "$(field key_size)" = "82" ]
    [ "$(field grid_w)" = "1114" ]
    # 1114/1280 is 87%, against 580/1280 before the fix
    [ "$(( $(field grid_w) * 100 / 1280 ))" = "87" ]
}

@test "tg5040 Smart Pro fills the screen width despite its larger PADDING" {
    probe 1280x720 2 40
    # PADDING of 40 reserves 80 scaled pixels top and bottom, so this is the one
    # device where the height budget binds rather than the width
    [ "$(field key_size)" = "73" ]
    [ "$(field grid_w)" = "997" ]
    [ "$(( $(field grid_w) * 100 / 1280 ))" = "77" ]
}

# the Smart Pro is the one device where the height budget binds rather than the
# width, so its block lands flush against the hint bar

@test "the tg5040 Smart Pro block is bounded by the hint bar, not the width" {
    probe 1280x720 2 40
    [ "$(field grid_bottom)" -le "$(field hints_top)" ]
    [ "$(( $(field hints_top) - $(field grid_bottom) ))" -lt "$(field key_size)" ]
}

# regression: the devices whose screenshots already look right

@test "my355 keeps the key size it has today" {
    probe 640x480 2 10
    [ "$(field key_size)" = "39" ]
    [ "$(field grid_w)" = "555" ]
}

@test "the Brick grows to the same proportion as the rest" {
    probe 1024x768 3 5
    # 58px before the fix, 79.5% of the width
    [ "$(field key_size)" = "63" ]
    [ "$(field grid_w)" = "891" ]
    [ "$(( $(field grid_w) * 100 / 1024 ))" = "87" ]
}

# the grid must never run under the button hints on any panel

@test "the grid clears the button hint bar on every supported panel" {
    for geometry in "640x480 2 10" "1024x768 3 5" "1280x720 2 10" "1280x720 2 40" "320x240 1 10" "480x273 1 10" "720x720 2 10"; do
        set -- $geometry
        probe "$1" "$2" "$3"
        [ "$(field grid_bottom)" -le "$(field hints_top)" ]
        [ "$(field input_y)" -ge "$(( $3 * $2 ))" ]
    done
}

# the smallest panels in the CI matrix

@test "the 320x240 panel still resolves a usable key size" {
    probe 320x240 1 10
    [ "$(field key_size)" = "19" ]
    [ "$(field grid_w)" = "271" ]
}

@test "the 480x273 panel still resolves a usable key size" {
    probe 480x273 1 10
    [ "$(field key_size)" = "30" ]
    [ "$(field grid_w)" = "414" ]
}

# --show-hardware-group and a title share the top pill band

@test "reserving the top bar pushes the block below the hardware group" {
    probe 1280x720 2 10
    plain_input_y="$(field input_y)"
    plain_key="$(field key_size)"

    probe 1280x720 2 10 --reserve-top
    [ "$(field input_y)" -gt "$plain_input_y" ]
    [ "$(field input_y)" -ge "$(( (10 + 30) * 2 ))" ]
    [ "$(field key_size)" -le "$plain_key" ]
    [ "$(field grid_bottom)" -le "$(field hints_top)" ]
}

@test "the title band matches the SDK pill the hardware group draws into" {
    probe 1280x720 2 10
    [ "$(field title_y)" = "20" ]
    [ "$(field title_h)" = "60" ]
}

# guards

@test "a screen with no vertical room clamps rather than going negative" {
    probe 1280x120 2 10
    [ "$(field key_size)" = "8" ]
    [ "$(field grid_h)" -gt 0 ]
}

@test "the input field lines up with the key grid" {
    probe 1280x720 2 10
    [ "$(field input_x)" = "$(field grid_x)" ]
    [ "$(field input_w)" = "$(field grid_w)" ]
    [ "$(field grid_y)" = "$(( $(field input_y) + $(field input_h) + $(field key_size) / 2 ))" ]
}

@test "a short row is centered inside the widest row" {
    probe 1280x720 2 10
    grid_x="$(field grid_x)"
    grid_w="$(field grid_w)"
    key="$(field key_size)"
    gap="$(field gap)"
    row_w="$(( 3 * key + 2 * gap ))"
    [ "$(field row_x_3)" = "$(( grid_x + (grid_w - row_w) / 2 ))" ]
}
