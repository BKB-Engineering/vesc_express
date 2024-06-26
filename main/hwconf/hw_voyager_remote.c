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

#include "hw_voyager_remote.h"
#include "lispif.h"
#include "lispbm.h"
#include "driver/gpio.h"
#include "utils.h"

void hw_init(void) {
	
    // GPIO config 
	gpio_set_level(PIN_PWR_LATCH, 0);
	gpio_set_level(PIN_BUZZER, 0);
    gpio_config_t gpconf = {0};
	gpconf.pin_bit_mask = BIT(PIN_BUZZER) | BIT(PIN_PWR_LATCH);
	gpconf.mode = GPIO_MODE_OUTPUT;
	gpconf.pull_down_en = GPIO_PULLDOWN_ENABLE;
	gpconf.pull_up_en = GPIO_PULLUP_DISABLE;
	gpconf.intr_type =  GPIO_INTR_DISABLE;
	gpio_config(&gpconf);

	gpconf.pin_bit_mask = BIT(PIN_BUTT_ON);
	gpconf.mode = GPIO_MODE_INPUT;
	gpconf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	gpconf.pull_up_en = GPIO_PULLUP_DISABLE;
	gpconf.intr_type =  GPIO_INTR_DISABLE;
	gpio_config(&gpconf);

	voyager_on_secuence();

}

void voyager_off_secuence(void){
	gpio_set_level(PIN_PWR_LATCH,0);
    gpio_config_t gpconf = {0};
	gpconf.pin_bit_mask = BIT(18) | BIT(19);
	gpconf.mode = GPIO_MODE_OUTPUT;
	gpconf.pull_down_en = GPIO_PULLDOWN_ENABLE;
	gpconf.pull_up_en = GPIO_PULLUP_DISABLE;
	gpconf.intr_type =  GPIO_INTR_DISABLE;
	gpio_config(&gpconf);
	gpio_set_level(18, 0);
	gpio_set_level(19, 0);
	for (volatile uint16_t j = 0; j < 10000 ; j++) {
		__NOP();
	}
}

void voyager_on_secuence(void){
uint16_t on_count = 0;
	for(;;){

		if(gpio_get_level(PIN_BUTT_ON)){
			on_count++;
		}else{
			//when charger monitor is ready, check if charging and show
			//charge screen instead of turn off
			voyager_off_secuence();
		}
		for (volatile uint16_t i = 0; i < 4000 ; i++) {
			__NOP();
		}
		if(on_count > 2000){
			break;
		}
	}
}

