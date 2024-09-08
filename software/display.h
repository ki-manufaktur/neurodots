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

#ifndef DISPLAY_H
#define DISPLAY_H

#define IS_RGBW false
#define NUM_PIXELS 32
#define WS2812_PIN 26

#include "pico/stdlib.h"
#include <map>
#include "include/ws2812.h"

class Display
{
public:
  Display(uint8_t brightness);
  void off();
  void push_leds();
  void serialize_maze(uint8_t maze[6][6], uint8_t maze_mask[6][6] = nullptr);
  void serialize_maze_inner(uint8_t maze[6][6]);
  void serialize_maze_middle(uint8_t maze[6][6]);
  void serialize_maze_hide_col(uint8_t maze[6][6], uint8_t col);
  void serialize_maze_hide_row(uint8_t maze[6][6], uint8_t row);
  void update_brightness(uint8_t brightness);
  uint8_t pix_array[32];

private:
  std::map<uint8_t, uint32_t> ColorMap;
  void put_pixel(uint32_t pixel_grb);
  uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b);
};

#endif