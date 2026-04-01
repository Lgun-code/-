#ifndef __DHT11_H
#define __DHT11_H

#include "main.h"

// 这里的引脚定义必须与你的实际接线一致
// 确认你接的是 PB14
#define DHT11_PORT GPIOB
#define DHT11_PIN  GPIO_PIN_14

void DHT11_Init(void);
uint8_t DHT11_ReadData(uint8_t *temp, uint8_t *humi);

#endif