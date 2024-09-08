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

#include <cstdint>
#include <map>

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

#include "model_data.h"
#include "GpioMap.h"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "game.h"
#include "display.h"

#define PUSHBUTTON_PIN 12
#define DEBOUNCE_TIME_US 150000
#define DEFAULT_BRIGTHNESS 8
#define DEFAULT_LEVEL 20

#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

std::map<uint8_t, uint32_t> GpioMap;

volatile uint32_t timer_start = 0;
volatile uint32_t timer_stop = 0;

volatile bool game_started = false;
volatile bool game_finished = false;
volatile bool need_to_initialize = false;
volatile bool state_changed = false;
volatile bool display_hint = false;
volatile int8_t last_hint_switch = -1;

Game game = Game();
Display display = Display(DEFAULT_BRIGTHNESS);

void update_display()
{
    display.serialize_maze(game.maze);
    display.push_leds();
}

// Interrupt handler button / switch
void pin_callback(uint gpio, uint32_t events)
{
    if (gpio == PUSHBUTTON_PIN)
    {
        if (game_started == false)
        {
            // Initiates a new game by shuffling the dots, using a random seed based on the duration of the push button press
            // Start the timer on falling edge
            if (events & GPIO_IRQ_EDGE_FALL)
            {
                if ((time_us_32() - timer_start) > DEBOUNCE_TIME_US)
                {
                    timer_start = time_us_32();
                }
            }
            // Stop the timer on rising edge
            if ((events & GPIO_IRQ_EDGE_RISE) && ((time_us_32() - timer_start) > 1000))
            {
                // disable interrupt for this pin
                gpio_set_irq_enabled(PUSHBUTTON_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
                timer_stop = time_us_32();

                // re-enable interrupt after DEBOUNCE_TIME_US
                add_alarm_in_us(DEBOUNCE_TIME_US, [](alarm_id_t id, void *user_data) -> int64_t
                                {
                                    uint gpio = (uint)(uintptr_t)user_data;

                                    // clear all queued interupts
                                    gpio_acknowledge_irq(gpio, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL);
                                    // Re-enable the interrupt for this GPIO pin
                                    gpio_set_irq_enabled(gpio, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);

                                    return 0; // No need to call again
                                },
                                (void *)(uintptr_t)gpio, false);

                need_to_initialize = true;
                game_finished = false;
            }
        }
        else // if game started show hint
        {
            if (events & GPIO_IRQ_EDGE_RISE)
            {
                display_hint = true;
            }
        }
    }

    else
    { // slide switches changed
        // Disable further interrupts on this pin
        gpio_set_irq_enabled(gpio, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);

        // user choosed different switch than recommended by AI
        if (last_hint_switch != GpioMap[gpio])
        {
            last_hint_switch = -1;
        }

        game_finished = false;

        // update game
        game.toggle_switch(GpioMap[gpio]);
        update_display();

        // trigger new AI inference
        state_changed = true;

        // re-enable interrupt after DEBOUNCE_TIME_US
        add_alarm_in_us(DEBOUNCE_TIME_US, [](alarm_id_t id, void *user_data) -> int64_t
                        {
                            uint gpio = (uint)(uintptr_t)user_data;

                            // clear all queued interupts
                            gpio_acknowledge_irq(gpio, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL);
                            // Re-enable the interrupt for this GPIO pin
                            gpio_set_irq_enabled(gpio, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);

                            return 0; // No need to call again
                        },
                        (void *)(uintptr_t)gpio, false);

        if ((game.check_finish() == true) && (game_started == true))
        {
            game_finished = true;
            game_started = false;
        }
    }
}

// tflite infrastructure
const tflite::Model *model = nullptr;
tflite::MicroInterpreter *interpreter = nullptr;
TfLiteTensor *input = nullptr;
TfLiteTensor *output = nullptr;

constexpr uint32_t kTensorArenaSize = 32000;
uint8_t tensor_arena[kTensorArenaSize];

int main()
{
    GpioMap[21] = 0;
    GpioMap[20] = 1;
    GpioMap[19] = 2;
    GpioMap[18] = 3;
    GpioMap[25] = 4;
    GpioMap[24] = 5;
    GpioMap[23] = 6;
    GpioMap[22] = 7;

    // Initialize serial port
    stdio_init_all();

    // Loop through GPIO 18 to 25 (slide switched) and set them as inputs with pull-ups
    for (uint8_t gpio = 18; gpio <= 25; gpio++)
    {
        gpio_init(gpio);             // Initialize the GPIO
        gpio_set_dir(gpio, GPIO_IN); // Set as input
        gpio_pull_up(gpio);          // Enable internal pull-up resistor
        sleep_us(10);
    }

    // Initialize PUSHBUTTON_PIN GPIO
    gpio_init(PUSHBUTTON_PIN);
    gpio_set_dir(PUSHBUTTON_PIN, GPIO_IN); // Set as input
    gpio_pull_up(PUSHBUTTON_PIN);          // Enable internal pull-up resistor
    sleep_us(10);

    // initialize ws2812 pio
    PIO pio = pio0;
    uint8_t sm = 0;
    uint32_t offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);

    // read stored values for LED brightness and game difficulty
    uint32_t addr;
    int8_t page = FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE - 1;
    uint8_t *p, brightness, difficulty;

    // flash wear protection
    while (page >= 0)
    {
        addr = XIP_BASE + FLASH_TARGET_OFFSET + (page * FLASH_PAGE_SIZE);
        p = (uint8_t *)addr;
        if (*p != 0xFF)
        {
            brightness = *p;
            difficulty = p[1];
            break;
        }
        page -= 1;
    }

    if (page == -1)
    {
        brightness = DEFAULT_BRIGTHNESS;
        difficulty = DEFAULT_LEVEL;
    }

    // check for pressed PUSHBUTTON_PIN during power-on to reinitialize brightness and game difficulty
    if (gpio_get(PUSHBUTTON_PIN) == 0)
    {
        uint8_t tmp_brightness = brightness;
        uint8_t tmp_difficulty = difficulty;
        while (gpio_get(PUSHBUTTON_PIN) == 0)
        {
            tmp_brightness = 1 << (gpio_get(20) * 4 + gpio_get(19) * 2 + gpio_get(18));
            // apply brightness
            display.update_brightness(tmp_brightness);
            update_display();

            tmp_difficulty = gpio_get(24) * 4 + gpio_get(23) * 2 + gpio_get(22);
            sleep_ms(100);
        }
        if ((tmp_brightness != brightness) || (tmp_difficulty != difficulty))
        {
            // store new parameters to flash
            page += 1;
            if (page == FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE)
            // need to erase flash sector
            {
                uint32_t ints = save_and_disable_interrupts();
                flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
                restore_interrupts(ints);
                page = 0;
            }
            addr = XIP_BASE + FLASH_TARGET_OFFSET + (page * FLASH_PAGE_SIZE);
            p = (uint8_t *)addr;

            uint8_t buf[FLASH_PAGE_SIZE] = {0};
            buf[0] = tmp_brightness;
            buf[1] = tmp_difficulty;

            difficulty = tmp_difficulty;

            uint32_t ints = save_and_disable_interrupts();
            flash_range_program(FLASH_TARGET_OFFSET + (page * FLASH_PAGE_SIZE), (uint8_t *)buf, FLASH_PAGE_SIZE);
            restore_interrupts(ints);
        }
    }
    else
    {
        display.update_brightness(brightness);
        update_display();
    }

    // adapt game.switches to actual switch posiitons
    for (uint8_t gpio = 18; gpio < 22; gpio++)
    {
        if (game.switches[GpioMap[gpio]] != gpio_get(gpio))
        {
            sleep_ms(300);
            game.toggle_switch(GpioMap[gpio]);
            update_display();
        }
    }
    for (uint8_t gpio = 22; gpio < 26; gpio++)
    {
        if (game.switches[GpioMap[gpio]] == gpio_get(gpio))
        {
            sleep_ms(300);
            game.toggle_switch(GpioMap[gpio]);
            update_display();
        }
    }

    // enabel interrupts
    for (uint32_t gpio = 18; gpio <= 25; gpio++)
    {
        gpio_set_irq_enabled_with_callback(gpio, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &pin_callback);
    }
    gpio_set_irq_enabled_with_callback(PUSHBUTTON_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &pin_callback);

    // set up tflite model
    tflite::InitializeTarget();

    // Map the model into a usable data structure. This doesn't involve any
    // copying or parsing, it's a very lightweight operation.
    model = tflite::GetModel(colordot_q_tfl_tflite);

    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        printf(
            "Model provided is schema version %d not equal "
            "to supported version %d. \n",
            model->version(), TFLITE_SCHEMA_VERSION);
    }

    static tflite::MicroMutableOpResolver<11> resolver;
    TfLiteStatus resolve_status = resolver.AddFullyConnected();
    resolve_status = resolver.AddReshape();
    resolve_status = resolver.AddGatherNd();
    resolve_status = resolver.AddSub();
    resolve_status = resolver.AddSlice();
    resolve_status = resolver.AddConv2D();
    resolve_status = resolver.AddTranspose();
    resolve_status = resolver.AddPad();
    resolve_status = resolver.AddConcatenation();
    resolve_status = resolver.AddMaxPool2D();
    resolve_status = resolver.AddAdd();

    if (resolve_status != kTfLiteOk)
    {
        printf("Op resolution failed \n");
    }

    // Build an interpreter to run the model with.
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize);
    interpreter = &static_interpreter;

    // Allocate memory from the tensor_arena for the model's tensors.
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk)
    {
        printf("AllocateTensors() failed \n");
    }

    // Obtain pointers to the model's input and output tensors.
    input = interpreter->input(0);
    output = interpreter->output(0);

    uint8_t hint_switch = 0;
    int8_t max_score = -127;

    state_changed = true;

    // Loop forever
    while (true)
    {
        if (need_to_initialize == true)
        {
            // Initiates a new game by shuffling the dots, using a random seed based on the duration of the push button press
            game.shuffle_dots(difficulty, timer_stop - timer_start);
            update_display();
            need_to_initialize = false;
            game_started = true;
            state_changed = true;
        }

        if (game_finished == true)
        {
            // play effect
            sleep_ms(200);
            display.off();
            display.push_leds();
            sleep_ms(150);
            display.serialize_maze_inner(game.maze);
            display.push_leds();
            sleep_ms(150);
            display.serialize_maze_middle(game.maze);
            display.push_leds();
            sleep_ms(150);
            display.serialize_maze(game.maze);
            display.push_leds();
            sleep_ms(200);
        }

        if ((state_changed == true) && (game_finished == false))
        {
            // start a new model inference
            state_changed = false;

            // prepare input
            for (uint8_t i = 0; i < 36; i++)
            {
                input->data.i32[i] = game.maze[i / 6][i % 6];
            }

            TfLiteStatus invoke_status = interpreter->Invoke();

            // get argmax on the output
            max_score = -128;
            for (uint8_t i = 0; i < 8; i++)
            {
                if ((output->data.int8[i] > max_score) && (i != last_hint_switch))
                {
                    max_score = output->data.int8[i];
                    hint_switch = i;
                }
            }
            last_hint_switch = hint_switch;
        }

        if (display_hint == true)
        {
            // display AI recommondation
            if (hint_switch < 4)
            {
                display.serialize_maze_hide_row(game.maze, hint_switch + 1);
            }
            else
            {
                display.serialize_maze_hide_col(game.maze, hint_switch - 3);
            }
            display.push_leds();
            sleep_ms(250);
            update_display();
            display_hint = false;
        }
        sleep_ms(10);
    }
}
