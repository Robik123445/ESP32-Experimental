/*
  my_machine_map.h - Custom board map for ESP32-S3 machine

  Part of grblHAL

  Copyright (c) 2025

  grblHAL is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  grblHAL is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with grblHAL. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MY_MACHINE_MAP_H
#define MY_MACHINE_MAP_H

#ifndef CONFIG_IDF_TARGET_ESP32S3
#error "This board has ESP32-S3 processor, select a corresponding build!"
#endif

#if N_ABC_MOTORS > 1
#error "Custom map only supports one auxiliary motor in addition to XYZ."
#endif

#if KEYPAD_ENABLE == 1
#error No free pins for I2C keypad!
#endif

#define BOARD_NAME "My Machine"

// Define step pulse output pins.
#define X_STEP_PIN              GPIO_NUM_4
#define Y_STEP_PIN              GPIO_NUM_6
#define Z_STEP_PIN              GPIO_NUM_10

// Define step direction output pins. NOTE: All direction pins must be on the same port.
#define X_DIRECTION_PIN         GPIO_NUM_5
#define Y_DIRECTION_PIN         GPIO_NUM_7
#define Z_DIRECTION_PIN         GPIO_NUM_11

// Define stepper driver enable/disable output pin(s).
#define STEPPERS_ENABLE_PIN     GPIO_NUM_8

#if Y_GANGED || Y_AUTO_SQUARE || (N_ABC_MOTORS > 0)
#define M3_AVAILABLE
#define M3_STEP_PIN             GPIO_NUM_12
#define M3_DIRECTION_PIN        GPIO_NUM_13
#define M3_ENABLE_PIN           STEPPERS_ENABLE_PIN
#define M3_LIMIT_PIN            GPIO_NUM_18
#define M3_LIMIT_PIN_MAX        M3_LIMIT_PIN
#endif

// Define homing/hard limit switch input pins and limit interrupt vectors.
// GPIO38 produced false X homing triggers on this ESP32-S3 build. Use a free
// header GPIO instead to keep X limit separate from the problematic line.
#define X_LIMIT_PIN             GPIO_NUM_41
#define Y_LIMIT_PIN             GPIO_NUM_16
#define Z_LIMIT_PIN             GPIO_NUM_40

#if Y_GANGED
#define Y2_STEP_PIN             M3_STEP_PIN
#define Y2_DIRECTION_PIN        M3_DIRECTION_PIN
#define Y2_ENABLE_PIN           M3_ENABLE_PIN
#endif

#if Y_AUTO_SQUARE || Y_GANGED
#define Y2_LIMIT_PIN            M3_LIMIT_PIN
#endif

// Reserve auxiliary outputs for motion/peripheral control.
#define AUXOUTPUT0_PIN          GPIO_NUM_1   // Spindle PWM
#define AUXOUTPUT1_PIN          GPIO_NUM_21  // Laser PWM
#define AUXOUTPUT2_PIN          GPIO_NUM_14  // Spindle enable
#define AUXOUTPUT3_PIN          GPIO_NUM_47  // Dust extraction (coolant flood)
#define AUXOUTPUT4_PIN          GPIO_NUM_48  // Air assist (coolant mist)
// Dedicated analog output for BLTouch servo control (M280/M401/M402).
#define AUXOUTPUT0_PWM_PIN      GPIO_NUM_9

#if DRIVER_SPINDLE_ENABLE & SPINDLE_PWM
#define SPINDLE_PWM_PIN         AUXOUTPUT0_PIN
#endif

#if DRIVER_SPINDLE_ENABLE & SPINDLE_ENA
#define SPINDLE_ENABLE_PIN      AUXOUTPUT2_PIN
#endif

#if COOLANT_ENABLE & COOLANT_FLOOD
#define COOLANT_FLOOD_PIN       AUXOUTPUT3_PIN
#endif
#if COOLANT_ENABLE & COOLANT_MIST
#define COOLANT_MIST_PIN        AUXOUTPUT4_PIN
#endif

#if LASER_PPI_ENABLE || (DRIVER_LASER_ENABLE & SPINDLE_PWM)
#define LASER_OUTPUT_PIN        AUXOUTPUT1_PIN
#endif

#if PROBE_ENABLE
#define PROBE_PIN               GPIO_NUM_42
#endif

#endif // MY_MACHINE_MAP_H
