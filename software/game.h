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

#ifndef GAME_H
#define GAME_H

#include <cstdint>

class Game
{
public:
  Game();
  void init();
  void toggle_switch(uint8_t sw);
  bool check_finish();
  void shuffle_dots(uint8_t level, uint32_t seed);
  uint8_t switches[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  uint8_t maze[6][6];

private:
  uint8_t solution[6][6] = {
      {0, 1, 1, 1, 1, 0},
      {1, 2, 2, 3, 3, 0},
      {1, 2, 2, 3, 3, 0},
      {1, 4, 4, 5, 5, 0},
      {1, 4, 4, 5, 5, 0},
      {0, 0, 0, 0, 0, 0}};

  void shiftRowRight(uint8_t row);
  void shiftRowLeft(uint8_t row);
  void shiftColDown(uint8_t col);
  void shiftColUp(uint8_t col);
};

#endif // GAME_H
