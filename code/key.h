#ifndef __KEY_H
#define __KEY_H

#include "stm32f1xx_hal.h"  // 根据具体芯片更换头文件
#include "main.h"

// === 用户可配置宏 ===
#define KEY_NUM              2           // 按键数量
#define KEY_DEBOUNCE_TIME    30          // 消抖时间(ms)
#define KEY_LONG_TIME        2000        // 长按判定时间(ms)

// === 按键状态枚举 ===
typedef enum {
    KEY_IDLE = 0,
    KEY_DEBOUNCE,
    KEY_PRESS,
    KEY_LONG_PRESS,
    KEY_RELEASE
} KEY_STATE;
extern int key_flag1;
extern int key_flag2;
extern volatile uint8_t key_pb0_short_evt;
extern volatile uint8_t key_pb1_short_evt;
extern volatile uint8_t key_pb0_long_evt;
extern volatile uint8_t key_pb1_long_evt;
// === 按键结构体定义 ===
typedef struct {
    GPIO_TypeDef *port;       // 引脚端口
    uint16_t pin;             // 引脚号
    uint8_t id;               // 按键编号
    KEY_STATE state;          // 当前状态
    uint32_t tick_last;       // 最近一次状态变化时间
    uint8_t is_long_pressed;  // 是否已触发长按
} Key_t;

// === 外部变量与函数声明 ===
extern Key_t keys[KEY_NUM];

void Key_Init(void);
void Key_Scan_All(void);

#endif
