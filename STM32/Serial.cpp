#include "Serial.h"

#include "STM32.h"

#include "stm32f1xx.h"
#include "stm32f1xx_hal_uart.h"

inline static UART_HandleTypeDef* get_uart(void *huart)
{
	return static_cast<UART_HandleTypeDef*>(huart);
}

SerialDebug::SerialDebug(void *huart) : huart(huart) {}

void SerialDebug::print(int val, int type) {
	uint8_t test[] ="Implement me!\r\n";
	HAL_UART_Transmit(get_uart(huart), test, strlen(test), 500);
}
void SerialDebug::print(int val) {}
void SerialDebug::print(char val) {}
void SerialDebug::print(const char *val) {}
void SerialDebug::print(float val) {}
void SerialDebug::println() {}
void SerialDebug::println(const char *val) {}
void SerialDebug::println(const char *val, int type) {}
void SerialDebug::println(int val, int type) {}
void SerialDebug::println(float val) {}
void SerialDebug::setTimeout(int val) {}
int SerialDebug::readBytesUntil(const char, const char*, int len)
{
	return 0;
}
