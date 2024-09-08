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

#include "game.h"
#include "GpioMap.h"

#include "pico/stdlib.h"
#include <stdlib.h>
#include <cstdint>

Game::Game()
{
    this->init();
}

void Game::init()
{
    for (uint8_t i = 0; i < 6; i++)
    {
        for (uint8_t j = 0; j < 6; j++)
        {
            this->maze[i][j] = this->solution[i][j];
        }
    }
    for (uint8_t i = 0; i < 8; i++) {
        this->switches[i] = 0;
    }
}

bool Game::check_finish()
{
    bool finish = true;
    for (uint8_t i = 0; i < 6; i++)
    {
        for (uint8_t j = 0; j < 6; j++)
        {
            if (this->maze[i][j] != this->solution[i][j])
            {
                finish = false;
            }
        }
    }
    return finish;
}

void Game::shiftRowRight(uint8_t row)
{
    for (uint8_t i = 5; i > 0; i--)
    {
        this->maze[row][i] = this->maze[row][i - 1];
    }
    this->maze[row][0] = 0;
}

void Game::shiftRowLeft(uint8_t row)
{
    for (uint8_t i = 0; i < 5; i++)
    {
        this->maze[row][i] = this->maze[row][i + 1];
    }
    this->maze[row][5] = 0;
}

void Game::shiftColDown(uint8_t col)
{
    for (uint8_t i = 5; i > 0; i--)
    {
        this->maze[i][col] = this->maze[i - 1][col];
    }
    this->maze[0][col] = 0;
}

void Game::shiftColUp(uint8_t col)
{
    for (uint8_t i = 0; i < 5; i++)
    {
        this->maze[i][col] = this->maze[i + 1][col];
    }
    this->maze[5][col] = 0;
}

void Game::toggle_switch(uint8_t sw)
{
    if (sw < 4)
    { // row
        uint8_t row = 1 + sw;
        uint8_t status = this->switches[sw];
        if (status == 0)
        { // shift right
            this->shiftRowRight(row);
        }
        else
        { // shift left
            this->shiftRowLeft(row);
        }
        this->switches[sw] ^= 1;
    }
    else
    { // column
        uint8_t col = sw - 3;
        uint8_t status = this->switches[sw];
        if (status == 0)
        { // shift down
            this->shiftColDown(col);
        }
        else
        { // shift up
            this->shiftColUp(col);
        }
        this->switches[sw] ^= 1;
    }
}

void Game::shuffle_dots(uint8_t level, uint32_t seed)
{
    uint16_t level2steps[8] = {3, 6, 10, 15, 25, 50, 100, 500};
    uint16_t steps = level2steps[level];
    int8_t last_sw = -1;
    int8_t sw = -1;

    srand(seed);

    while (true)
    {
        this->init();
        steps = level2steps[level];
        last_sw = -1;
        sw = -1;

        while (steps > 0)
        {
            sw = rand() % 8;
            if (sw != last_sw)
            {
                this->toggle_switch(sw);
                last_sw = sw;
                steps -= 1;
            }
        }

        // correct switch position
        for (int8_t gpio = 18; gpio < 22; gpio++)
        {
            if (this->switches[GpioMap[gpio]] != gpio_get(gpio))
            {
                this->toggle_switch(GpioMap[gpio]);
            }
        }
        for (uint8_t gpio = 22; gpio < 26; gpio++)
        {
            if (this->switches[GpioMap[gpio]] == gpio_get(gpio))
            {
                this->toggle_switch(GpioMap[gpio]);
            }
        }
        if (this->check_finish() != true)
        {
            break;
        }
    }
}
