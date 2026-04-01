#include "key.h"

// === 全局按键数组定义 ===
// 用户根据实际连线更改端口和引脚
Key_t keys[KEY_NUM] = {
    {GPIOB, GPIO_PIN_0, 1, KEY_IDLE, 0, 0},
    {GPIOB, GPIO_PIN_1, 2, KEY_IDLE, 0, 0},
};
int key_flag1=0;
int key_flag2=0;
volatile uint8_t key_pb0_short_evt = 0;
volatile uint8_t key_pb1_short_evt = 0;
volatile uint8_t key_pb0_long_evt  = 0;
volatile uint8_t key_pb1_long_evt  = 0;
// === 按键初始化（如需配置为输入，用户可扩展） ===
void Key_Init(void)
{
    // 可选初始化逻辑：如果按键没有在 main.c 中配置
}

// === 主按键扫描函数（应在主循环中定期调用） ===
void Key_Scan_All(void)
{
    uint32_t now = HAL_GetTick();  // 获取当前时间（ms）

    for (int i = 0; i < KEY_NUM; i++) {
        Key_t *k = &keys[i];
        uint8_t is_pressed = (HAL_GPIO_ReadPin(k->port, k->pin) == GPIO_PIN_RESET);

        switch (k->state) {
            case KEY_IDLE:
                if (is_pressed) {
                    k->state = KEY_DEBOUNCE;
                    k->tick_last = now;
                }
                break;

            case KEY_DEBOUNCE:
                if ((now - k->tick_last) >= KEY_DEBOUNCE_TIME) {
                    if (is_pressed) {
                        k->state = KEY_PRESS;
                        k->tick_last = now;
                        k->is_long_pressed = 0;
                    } else {
                        k->state = KEY_IDLE;
                    }
                }
                break;

            case KEY_PRESS:
                if (!is_pressed) {
                    if (!k->is_long_pressed) {
                        // === ★ 短按事件处理 ★ ===
                        if (k->id == 1)
												{
													key_flag2=1;
													key_pb0_short_evt = 1;
												}
                        if (k->id == 2)
												{
													key_flag1=1;
													key_pb1_short_evt = 1;
												}
                    }
                    k->state = KEY_RELEASE;
                    k->tick_last = now;
                } else if ((now - k->tick_last) >= KEY_LONG_TIME && !k->is_long_pressed) {
                    // === ★ 长按事件处理 ★ ===
                    k->is_long_pressed = 1;
                                        if (k->id == 1) key_pb0_long_evt = 1;
                                        if (k->id == 2) key_pb1_long_evt = 1;
                }
                break;

            case KEY_RELEASE:
                if (!is_pressed) {
                    k->state = KEY_IDLE;
                } else {
                    k->state = KEY_PRESS;
                    k->tick_last = now;
                }
                break;

            default:
                k->state = KEY_IDLE;
                break;
        }
    }
}
