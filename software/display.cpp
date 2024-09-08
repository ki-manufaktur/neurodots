/* NEURODOTS
 * Copyright (C) 2024 ki-manufaktur.de
 *
 * This file is part of <your project name>.
 *
 * <Your project name> is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "display.h"

struct RGB {
    uint8_t r, g, b;
};

struct HSV {
    float h; // Angle in degrees [0, 360)
    float s; // [0, 1]
    float v; // [0, 1]
};

RGB hsv2rgb(HSV hsv) {
    RGB rgb;
    unsigned char region, remainder, p, q, t;

    if (hsv.s == 0) {
        rgb.r = rgb.g = rgb.b = hsv.v * 255;
        return rgb;
    }

    region = hsv.h / 60;
    remainder = (int)(hsv.h - (region * 60)) * 6;

    p = (hsv.v * (1 - hsv.s)) * 255;
    q = (hsv.v * (1 - (hsv.s * remainder / 255))) * 255;
    t = (hsv.v * (1 - (hsv.s * (255 - remainder) / 255))) * 255;
    hsv.v *= 255;

    switch (region) {
        case 0:
            rgb.r = hsv.v; rgb.g = t; rgb.b = p;
            break;
        case 1:
            rgb.r = q; rgb.g = hsv.v; rgb.b = p;
            break;
        case 2:
            rgb.r = p; rgb.g = hsv.v; rgb.b = t;
            break;
        case 3:
            rgb.r = p; rgb.g = q; rgb.b = hsv.v;
            break;
        case 4:
            rgb.r = t; rgb.g = p; rgb.b = hsv.v;
            break;
        default:
            rgb.r = hsv.v; rgb.g = p; rgb.b = q;
            break;
    }

    return rgb;
}


Display::Display(uint8_t brightness)
{
    this->update_brightness(brightness);
}

void Display::update_brightness(uint8_t brightness){
    this->ColorMap[0] = 0;
    
    HSV hsv;
    hsv.v = float(brightness)/255.0;

    hsv.h = 0; // red
    hsv.s = 0;
    RGB rgb = hsv2rgb(hsv);
    this->ColorMap[1] = urgb_u32(rgb.r, rgb.g, rgb.b);    // white

    hsv.s = 1;
    rgb = hsv2rgb(hsv);
    this->ColorMap[2] = urgb_u32(rgb.r, rgb.g, rgb.b); 

    hsv.h = 120;  // green
    rgb = hsv2rgb(hsv);
    this->ColorMap[3] = urgb_u32(rgb.r, rgb.g, rgb.b);  

    hsv.h = 60;  // yellow
    rgb = hsv2rgb(hsv);
    this->ColorMap[4] = urgb_u32(rgb.r, rgb.g, rgb.b);

    hsv.h = 240;  // blue
    rgb = hsv2rgb(hsv);
    this->ColorMap[5] = urgb_u32(rgb.r, rgb.g, rgb.b);
}


void Display::put_pixel(uint32_t pixel_grb)
{
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

void Display::off()
{
    for (uint8_t i = 0; i < NUM_PIXELS; i++)
    {
        put_pixel(0);
    }
}

void Display::push_leds()
{
    for (uint8_t i = 0; i < NUM_PIXELS; i++)
    {
        put_pixel(this->ColorMap[this->pix_array[i]]);
    }
}

uint32_t Display::urgb_u32(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)(r) << 8) |
           ((uint32_t)(g) << 16) |
           (uint32_t)(b);
}

void Display::serialize_maze(uint8_t maze[6][6], uint8_t maze_mask[6][6])
{
    uint8_t mask[6][6] = {
        {1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1}};
    if (maze_mask != nullptr)
    {
        for (uint8_t i = 0; i < 6; ++i)
        {
            for (uint8_t j = 0; j < 6; ++j)
            {
                mask[i][j] = maze_mask[i][j];
            }
        }
    }
    for (uint8_t i = 0; i < 4; i++)
    {
        this->pix_array[i] = maze[0][i + 1] * mask[0][i + 1];
    }
    for (uint8_t i = 0; i < 6; i++)
    {
        this->pix_array[i + 4] = maze[1][5 - i] * mask[1][5 - i];
    }
    for (uint8_t i = 0; i < 6; i++)
    {
        this->pix_array[i + 10] = maze[2][i] * mask[2][i];
    }
    for (uint8_t i = 0; i < 6; i++)
    {
        this->pix_array[i + 16] = maze[3][5 - i] * mask[3][5 - i];
    }
    for (uint8_t i = 0; i < 6; i++)
    {
        this->pix_array[i + 22] = maze[4][i] * mask[4][i];
    }
    for (uint8_t i = 0; i < 4; i++)
    {
        this->pix_array[i + 28] = maze[5][4 - i] * mask[5][4 - i];
    }
}

void Display::serialize_maze_inner(uint8_t maze[6][6])
{
    uint8_t maze_mask[6][6] = {0};

    for (uint8_t i = 2; i < 4; ++i)
    {
        for (uint8_t j = 2; j < 4; ++j)
        {
            maze_mask[i][j] = 1;
        }
    }

    this->serialize_maze(maze, maze_mask);
}

void Display::serialize_maze_middle(uint8_t maze[6][6])
{
    uint8_t maze_mask[6][6] = {0};

    for (uint8_t i = 1; i < 5; ++i)
    {
        for (uint8_t j = 1; j < 5; ++j)
        {
            maze_mask[i][j] = 1;
        }
    }

    this->serialize_maze(maze, maze_mask);
}

void Display::serialize_maze_hide_row(uint8_t maze[6][6], uint8_t row)
{
    uint8_t maze_mask[6][6] = {
        {1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1}};

    for (uint8_t i = 0; i < 6; ++i)
    {
        maze_mask[row][i] = 0;
    }

    this->serialize_maze(maze, maze_mask);
}

void Display::serialize_maze_hide_col(uint8_t maze[6][6], uint8_t col)
{
    uint8_t maze_mask[6][6] = {
        {1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1}};

    for (uint8_t i = 0; i < 6; ++i)
    {
        maze_mask[i][col] = 0;
    }

    this->serialize_maze(maze, maze_mask);
}