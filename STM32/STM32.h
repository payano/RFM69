#pragma once

#include <stdint.h>
#include <cstdio> /* sprintf and sscanf */

#include "Serial.h"

#define STM32IDE
#undef DEBUG

#define HIGH    1           /* GPIO_PIN_SET,         stm32f1xx_hal_gpio.h */
#define LOW     0           /* GPIO_PIN_RESET,       stm32f1xx_hal_gpio.h */
#define RISING  0x10110000u /* GPIO_MODE_IT_RISING,  stm32f1xx_hal_gpio.h */
#define FALLING 0x10210000u /* GPIO_MODE_IT_FALLING, stm32f1xx_hal_gpio.h */

enum PRINT_TYPE {
	HEX = 0,
	DEC,
	BIN,
};

enum OUTPUT_TYPE {
	OUTPUT = 0,
	INPUT,
};

/* GPIO */
/* Struct to handle gpio write pins*/
struct gpio_pin {
	struct gpio_pin operator=(struct gpio_pin &b) {
		struct gpio_pin ret;
		ret.GPIOx = b.GPIOx;
		ret.GPIO_Pin = b.GPIO_Pin;
		return ret;
	}
	void *GPIOx;
	uint16_t GPIO_Pin;
};


/* SPI */
#define SPI_CLOCK_DIV16 0
#define SPI_MODE0 0

#define MSBFIRST 0
#define LSBFIRST 1

#define byte uint8_t

/* if c++ inputs std::string this must be revised */
#define F(str) str

unsigned long millis();
uint32_t abs(uint32_t val);


void detachInterrupt(struct gpio_pin &irqnum);
void attachInterrupt(struct gpio_pin &irqnum, void (*func)(), int rise_or_fall);
void digitalWrite(struct gpio_pin &pin, uint8_t val);
int strlen(const void *data);
/* Is not used, this is configured via stm32cubemx instead */
void pinMode(struct gpio_pin &pin, OUTPUT_TYPE type);

