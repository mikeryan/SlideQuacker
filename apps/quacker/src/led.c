/**
 * Copyright 2016 ICE9 Consulting
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#include <ctype.h>
#include <stdint.h>

#include "os/os.h"
#include "hal/hal_gpio.h"

// speedhack
#include "mcu/nrf51.h"

/* 7segs look like this:
 *     _       0
 *    |_|    1 6 5
 *    |_|    2 3 4
 */

static const uint8_t digits[] = {
    0b0111111, /* 0 */ 0b0110000, /* 1 */ 0b1101101, /* 2 */ 0b1111001, /* 3 */
    0b1110010, /* 4 */ 0b1011011, /* 5 */ 0b1011111, /* 6 */ 0b0110011, /* 7 */
    0b1111111, /* 8 */ 0b1111011, /* 9 */ 0b1110111, /* A */ 0b1011110, /* B */
    0b0001111, /* C */ 0b1111100, /* D */ 0b1001111, /* E */ 0b1000111, /* F */
};

static const uint8_t alpha[] = {
    0b1110111, // A
    0b1011110, // B
    0b1001100, // C, lower
    0b1111100, // D
    0b1001111, // E
    0b1000111, // F
    0b1111011, // G, same as 9
    0b1010110, // H, lower
    0b0110000, // I
    0b0111000, // J
    0b0000000, // K [no repr]
    0b0001110, // L
    0b0000000, // M [no repr]
    0b1010100, // N
    0b1011100, // O, lower
    0b1100111, // P
    0b1110011, // Q
    0b1000100, // R
    0b1011011, // S, same as 5
    0b1001110, // T
    0b0011100, // U, lower
    0b0011100, // V, lower, same as u
    0b0000000, // W [no repr]
    0b1110110, // X
    0b1111010, // Y
    0b0000000, // Z [no repr]
}; 

struct capital {
    char letter;
    uint8_t repr;
};

static const struct capital capitals[] = {
    { 'C', 0b0001111, },
    { 'O', 0b0111111, },
    { 'U', 0b0111110, },
    { 'V', 0b0111110, },
};

static
uint8_t char_repr(char input)
{
    if (input == '*') // ö
        return 0b1011101;

    if (isalpha(input)) {
        if (isupper(input)) {
            int i;
            for (i = 0; i < sizeof(capitals)/sizeof(struct capital); ++i)
                if (input == capitals[i].letter)
                    return capitals[i].repr;
            // didn't find it, no capital representations
            input = tolower(input);
        }
        return alpha[input - 'a'];
    }

    if (isdigit(input))
        return digits[input - '0'];

    return 0b0000000;
}

static void
led_putchar(char input, int left)
{
    uint16_t repr = char_repr(input);

    if (left) {
        NRF_GPIO->OUTCLR = ~repr & 0x7f;
        NRF_GPIO->OUTSET = repr;
    } else {
        repr <<= 7;
        NRF_GPIO->OUTCLR = ~repr & (0x7f << 7);
        NRF_GPIO->OUTSET = repr;
    }
}

void
led_scroll(char *message)
{
    uint16_t display = 0;
    char *next;

#define SHOW NRF_GPIO->OUTCLR = ~display & 0x3fff; NRF_GPIO->OUTSET = display; os_time_delay(500)

    SHOW;
    for (next = message; *next; ++next) {
        display >>= 7;
        display |= char_repr(*next) << 7;
        SHOW;
    }
    display >>= 7;
    SHOW;
    display >>= 7;
    SHOW;
}

void
led_static(char *letters)
{
    led_putchar(letters[0], 1);
    led_putchar(letters[1], 0);
}

void
led_number(uint8_t num)
{
    // TODO
}

void
led_number_hex(uint8_t num)
{
    // TODO
}

void
led_spinner(void)
{
    int i;

#define DELAY 30
    while (1) {
        for (i = 0; i < 14; ++i) {
            // skip center
            if (i == 6 || i == 13)
                continue;
            hal_gpio_toggle(i);
            os_time_delay(DELAY);
        }
    }
}

void
led_init(void)
{
    int i;
    for (i = 0; i < 16; ++i)
        hal_gpio_init_out(i, 0);
}

// run through the alphabet
void
test_alpha(void)
{
    int i = 0;
    while (1) {
        led_putchar('A' + i, 1);
        led_putchar('a' + i, 0);
        os_time_delay(500);

        i = (i + 1) % 26;
    }
}
