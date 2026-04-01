#include "dht11.h"

// ==========================================
// 1. 内置最简微秒延时 (基于 DWT 计数器)
// ==========================================
// 无需外部初始化，直接调用即可
void DHT11_Delay_us(uint32_t us)
{
    static uint8_t initialized = 0;
    if (!initialized) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
        initialized = 1;
    }
    
    uint32_t startTick = DWT->CYCCNT;
    // SystemCoreClock 是 HAL 库全局变量，自动根据时钟配置变化
    uint32_t ticks = us * (SystemCoreClock / 1000000); 
    while ((DWT->CYCCNT - startTick) < ticks);
}

// ==========================================
// 2. 引脚模式切换辅助函数
// ==========================================

// 切换为输出模式 (推挽)
static void DHT11_Mode_Out(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; // 推挽输出强劲
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

// 切换为输入模式 (上拉)
static void DHT11_Mode_In(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP; // 开启内部上拉
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

// ==========================================
// 3. 核心读取逻辑
// ==========================================

// 初始化
void DHT11_Init(void)
{
    // 开启 GPIOB 时钟 (如果你改了引脚，记得改这里)
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    // 默认输出高电平
    DHT11_Mode_Out();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
    HAL_Delay(200); // 上电等待 1s
}

// 读取字节
static uint8_t DHT11_Read_Byte(void)
{
    uint8_t i, dat = 0;
    for (i = 0; i < 8; i++)
    {
        // 1. 等待低电平结束 (数据位开始前的 50us 低电平)
        // 加超时防止死循环
        uint32_t timeout = 1000;
        while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET) {
            if (--timeout == 0) return 0; 
            DHT11_Delay_us(1);
        }

        // 2. 现在是高电平，延时 40us 判断是 0 还是 1
        // '0' -> 26-28us 高电平
        // '1' -> 70us 高电平
        DHT11_Delay_us(40);

        // 3. 检查电平
        if (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET)
        {
            dat |= (1 << (7 - i)); // 读到 1
            // 等待高电平结束 (直到变为低电平)
            timeout = 1000;
            while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET) {
                if (--timeout == 0) return 0;
                DHT11_Delay_us(1);
            }
        }
        // 如果是 0，上面的 delay_us(40) 后电平已经变低了，直接进入下一次循环
    }
    return dat;
}

// 读取数据主函数
// 返回: 0=成功, 1=传感器无响应, 2=校验错误
uint8_t DHT11_ReadData(uint8_t *temp, uint8_t *humi)
{
    uint8_t buf[5] = {0};

    // --- 1. 主机发送开始信号 ---
    DHT11_Mode_Out();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET); // 拉低
    HAL_Delay(20); // 至少 18ms
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);   // 拉高
    DHT11_Delay_us(30); // 等待 30us

    // --- 2. 切换输入，等待从机响应 ---
    DHT11_Mode_In();
    
    // 检测 DHT11 是否拉低了总线
    if (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET)
    {
        // 等待 80us 低电平响应结束
        uint32_t timeout = 100;
        while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET) {
            if (--timeout == 0) return 1; // 超时
            DHT11_Delay_us(1);
        }

        // 等待 80us 高电平准备结束 (DHT11 准备发送数据)
        timeout = 100;
        while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET) {
            if (--timeout == 0) return 1; // 超时
            DHT11_Delay_us(1);
        }

        // --- 3. 读取 40 位数据 ---
        for (int i = 0; i < 5; i++) {
            buf[i] = DHT11_Read_Byte();
        }

        // --- 4. 校验 ---
        // 校验和 = 湿高 + 湿低 + 温高 + 温低
        if ((buf[0] + buf[1] + buf[2] + buf[3]) == buf[4])
        {
            *humi = buf[0];
            *temp = buf[2];
            return 0; // 成功
        }
        else {
            return 2; // 校验失败
        }
    }
    
    return 1; // 没有检测到响应 (一直是高电平)
}