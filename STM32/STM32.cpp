#include "STM32.h"
#include <stm32f1xx_hal.h>

unsigned long millis() { return 0;}
uint32_t abs(uint32_t val)  { return val >= 0 ? val : val *= -1; }

void detachInterrupt(struct gpio_pin &irqnum) {}
void attachInterrupt(struct gpio_pin &irqnum, void (*func)(), int rise_or_fall) {}
void digitalWrite(struct gpio_pin &pin, uint8_t val)
{
	GPIO_PinState pinState = static_cast<GPIO_PinState>(val);
	HAL_GPIO_WritePin(static_cast<GPIO_TypeDef*>(pin.GPIOx), pin.GPIO_Pin, pinState);
}
void pinMode(struct gpio_pin &pin, PRINT_TYPE type) {}

int strlen(const void *data)
{
	const char *str = static_cast<const char*>(data);
	int i = 0;
	for(; *str != '\0' ; ++i, ++str);
	return i;
}
