#include "ina226.h"
#include "bsp_delay.h"

// --- 内部寄存器地址 ---
#define REG_CONFIG      0x00 
#define REG_SHUNT_V     0x01 
#define REG_BUS_V       0x02 
#define REG_POWER       0x03 
#define REG_CURRENT     0x04 
#define REG_CALIBRATION 0x05 

// --- 引脚操作宏 ---
#define IIC_SCL_H()     HAL_GPIO_WritePin(INA226_SCL_PORT, INA226_SCL_PIN, GPIO_PIN_SET)
#define IIC_SCL_L()     HAL_GPIO_WritePin(INA226_SCL_PORT, INA226_SCL_PIN, GPIO_PIN_RESET)
#define IIC_SDA_H()     HAL_GPIO_WritePin(INA226_SDA_PORT, INA226_SDA_PIN, GPIO_PIN_SET)
#define IIC_SDA_L()     HAL_GPIO_WritePin(INA226_SDA_PORT, INA226_SDA_PIN, GPIO_PIN_RESET)
#define IIC_SDA_READ()  HAL_GPIO_ReadPin(INA226_SDA_PORT, INA226_SDA_PIN)

// =======================================================
// ?? 核心绝招：SDA 引脚方向动态切换 (完美解决读全1问题)
// =======================================================
static void SDA_IN(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = INA226_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT; // 强制变输入，让传感器说话
    GPIO_InitStruct.Pull = GPIO_PULLUP;     // 开启内部上拉保底
    HAL_GPIO_Init(INA226_SDA_PORT, &GPIO_InitStruct);
}

static void SDA_OUT(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = INA226_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; // 强制变推挽输出，像OLED一样暴力驱动
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(INA226_SDA_PORT, &GPIO_InitStruct);
}

// I2C 起始信号
static void INA226_IIC_Start(void)
{
    SDA_OUT();
    IIC_SDA_H();
    IIC_SCL_H();
    delay_us(4);
    IIC_SDA_L();
    delay_us(4);
    IIC_SCL_L();
}

// I2C 停止信号
static void INA226_IIC_Stop(void)
{
    SDA_OUT();
    IIC_SCL_L();
    IIC_SDA_L();
    delay_us(4);
    IIC_SCL_H(); 
    delay_us(4);
    IIC_SDA_H();
    delay_us(4);
}

// 等待应答
static uint8_t INA226_IIC_Wait_Ack(void)
{
    uint8_t ucErrTime = 0;
    
    SDA_IN(); // ?? 主机变输入，准备听传感器回话
    
    IIC_SDA_H();
    delay_us(2);
    IIC_SCL_H();
    delay_us(2);
    
    while(IIC_SDA_READ())
    {
        ucErrTime++;
        if(ucErrTime > 250)
        {
            INA226_IIC_Stop();
            return 1; // 接收应答失败
        }
    }
    IIC_SCL_L();
    return 0; // 成功
}

// 产生 ACK
static void INA226_IIC_Ack(void)
{
    SDA_OUT();
    IIC_SCL_L();
    IIC_SDA_L();
    delay_us(4);
    IIC_SCL_H();
    delay_us(4);
    IIC_SCL_L();
}

// 产生 NACK
static void INA226_IIC_NAck(void)
{
    SDA_OUT();
    IIC_SCL_L();
    IIC_SDA_H();
    delay_us(4);
    IIC_SCL_H();
    delay_us(4);
    IIC_SCL_L();
}

// 发送一个字节
static void INA226_IIC_Send_Byte(uint8_t txd)
{
    uint8_t t;
    SDA_OUT(); // ?? 保证处于输出状态
    IIC_SCL_L();
    for(t = 0; t < 8; t++)
    {
        if((txd & 0x80) >> 7)
            IIC_SDA_H();
        else
            IIC_SDA_L();
        txd <<= 1;
        delay_us(4);
        IIC_SCL_H();
        delay_us(4);
        IIC_SCL_L();
        delay_us(4);
    }
}

// 读一个字节
static uint8_t INA226_IIC_Read_Byte(uint8_t ack)
{
    uint8_t i, receive = 0;
    
    SDA_IN(); // ?? 必须变输入才能读
    
    for(i = 0; i < 8; i++)
    {
        IIC_SCL_L();
        delay_us(4);
        IIC_SCL_H();
        receive <<= 1;
        if(IIC_SDA_READ()) receive++;
        delay_us(4);
    }
    if (!ack)
        INA226_IIC_NAck();
    else
        INA226_IIC_Ack();
    return receive;
}

// --- 写寄存器 ---
static void INA226_WriteReg(uint8_t reg, uint16_t data)
{
    INA226_IIC_Start();
    INA226_IIC_Send_Byte(INA226_ADDR);
    INA226_IIC_Wait_Ack();
    INA226_IIC_Send_Byte(reg);
    INA226_IIC_Wait_Ack();
    INA226_IIC_Send_Byte((uint8_t)(data >> 8));
    INA226_IIC_Wait_Ack();
    INA226_IIC_Send_Byte((uint8_t)(data & 0xFF));
    INA226_IIC_Wait_Ack();
    INA226_IIC_Stop();
}

// --- 读寄存器 ---
static uint16_t INA226_ReadReg(uint8_t reg)
{
    uint16_t data = 0;
    
    INA226_IIC_Start();
    INA226_IIC_Send_Byte(INA226_ADDR);
    INA226_IIC_Wait_Ack();
    INA226_IIC_Send_Byte(reg);
    INA226_IIC_Wait_Ack();
    
    INA226_IIC_Start();
    INA226_IIC_Send_Byte(INA226_ADDR | 0x01);
    INA226_IIC_Wait_Ack();
    
    data = INA226_IIC_Read_Byte(1); // 发 ACK
    data <<= 8;
    data |= INA226_IIC_Read_Byte(0); // 发 NACK
    
    INA226_IIC_Stop();
    
    return data;
}

// GPIO 初始化引脚 (初始化时全部推挽输出)
static void INA226_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    INA226_SCL_CLK_ENABLE();
    INA226_SDA_CLK_ENABLE();

    GPIO_InitStruct.Pin = INA226_SCL_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(INA226_SCL_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = INA226_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; 
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(INA226_SDA_PORT, &GPIO_InitStruct);

    IIC_SCL_H();
    IIC_SDA_H();
}




/* ==================================================================== */
/* ==================== 暴露的应用层接口 =============================== */
/* ==================================================================== */

/**
 * @brief  初始化 INA226，配置运行模式并下发校准数据
 * @return 1:成功  0:未检测到芯片 (连线错误或芯片损坏)
 */
uint8_t INA226_Init(void)
{
    uint16_t id_check;
    
    INA226_GPIO_Init();
    
    // 读制造商 ID 寄存器 (0xFE) 验证芯片，正确应返回 0x5449
    id_check = INA226_ReadReg(0xFE);
    if(id_check != 0x5449)
    {
        return 0; // 芯片离线
    }

    // 1. 配置芯片运行参数
    // 0x4527 = 平均值16次，总线转换时间1.1ms，分流转换时间1.1ms，连续测压测流模式
    INA226_WriteReg(REG_CONFIG, 0x4527);
    
    // 2. 写入校准寄存器 (必须写，否则读不到电流)
    INA226_WriteReg(REG_CALIBRATION, INA226_CAL_VALUE);
    
    return 1;
}

/**
 * @brief  获取总线电压 (VBUS)
 * @return 绝对电压值，单位: V
 */
float INA226_Get_BusVoltage(void)
{
    uint16_t reg_val = INA226_ReadReg(REG_BUS_V);
    // VBUS LSB 恒定为 1.25mV
    return (float)reg_val * 0.00125f;
}

/**
 * @brief  获取经过校准和换算后的电流值
 * @note   支持正负极性，完美支持充放电检测
 * @return 电流值，单位: mA
 */
float INA226_Get_Current(void)
{
    int16_t raw_current = 0;
    int16_t raw_shunt = 0;
    float shunt_mv = 0.0f;
    
    // 强制转换为带符号的 int16_t 
    // INA226 内部使用补码表示负数，转化为 int16_t 会自动映射出正确负值
    raw_current = (int16_t)INA226_ReadReg(REG_CURRENT);

    /* 某些情况下校准寄存器会失效，先尝试重写校准并重读 */
    if (raw_current == 0)
    {
        raw_shunt = (int16_t)INA226_ReadReg(REG_SHUNT_V);
        if (raw_shunt != 0)
        {
            INA226_WriteReg(REG_CALIBRATION, INA226_CAL_VALUE);
            raw_current = (int16_t)INA226_ReadReg(REG_CURRENT);

            /* 若电流寄存器仍为0，则用分流电压按欧姆定律兜底换算 */
            if (raw_current == 0)
            {
                /* Shunt Voltage LSB = 2.5uV = 0.0025mV */
                shunt_mv = (float)raw_shunt * 0.0025f;
                return (shunt_mv / INA226_R_SHUNT) * 1000.0f;
            }
        }
    }
    
    // 原始数据乘以 LSB 即为真实毫安值
    return (float)raw_current * INA226_CURRENT_LSB;
}

/**
 * @brief  获取瞬时功率 (顺带提供)
 * @return 功率值，单位: mW
 */
float INA226_Get_Power(void)
{
    uint16_t reg_val = INA226_ReadReg(REG_POWER);
    // Power LSB 等于 25 * Current_LSB 
    return (float)reg_val * (25.0f * INA226_CURRENT_LSB);
}