/*
	Copyright 2022 Benjamin Vedder	benjamin@vedder.se

	This file is part of the VESC firmware.

	The VESC firmware is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    The VESC firmware is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    */

#ifndef MAIN_HWCONF_VOYAGER_R_H_
#define MAIN_HWCONF_VOYAGER_R_H_

#include "conf_general.h"
#include "adc.h"

#define HW_NAME						"Voyager remote"
#define HW_EARLY_LBM_INIT
#define HW_NO_UART
#define HW_INIT_HOOK()				hw_init()

#define HW_ADC_CH0					ADC1_CHANNEL_0
#define HW_ADC_CH1					ADC1_CHANNEL_1
#define HW_ADC_CH2					ADC1_CHANNEL_2

// I2C
#define PIN_SDA						3
#define PIN_SCL						4

//GPIO
#define PIN_PWR_LATCH               20
#define PIN_BUZZER                  21
#define PIN_BUTT_ON                 10


// Functions
void hw_init(void);
void voyager_on_secuence(void);
void voyager_off_secuence(void);
#endif /* MAIN_HWCONF_DEVKIT_C3_H_ */
