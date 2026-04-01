/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled.h"
#include "ina226.h"
#include "bsp_delay.h"
#include "stdio.h"  // �����Ҫ�� printf ��ӡ���ڣ���Ҫ�������
#include "dht11.h"
#include "key.h"
#include "cloud_service.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
float temp,humi;
extern uint8_t NET_State;
float bus_voltage = 0.0f; 
    float current_ma = 0.0f;  
    float power_mw = 0.0f;  
// extern uint8_t NET_Step; // 连接步骤状态机变量

#define ALARM_BLINK_TIME_MS        3000U
#define VOL_THRESHOLD_MIN_V          2.50f
#define VOL_THRESHOLD_MAX_V          5.00f
#define VOL_THRESHOLD_STEP_V         0.10f
#define ALARM_BLINK_PERIOD_MS       250U
/* 手动消警后，3秒内不允许再次触发 */
#define ALARM_RETRIGGER_GUARD_MS   3000U
/* 继电器恢复供电后，先给系统和云连接留稳定时间 */
#define POWER_RESTORE_MUTE_MS      8000U

typedef enum {
  ALARM_IDLE = 0,
  ALARM_BLINKING,
  ALARM_LATCHED
} alarm_state_t;

static alarm_state_t g_alarm_state = ALARM_IDLE;
static uint32_t g_alarm_start_tick = 0;
static uint32_t g_alarm_last_blink_tick = 0;
static uint32_t g_alarm_guard_until = 0;
static uint8_t g_pa1_prev_level = 1U;
static uint8_t g_sensor_reinit_pending = 0U;
static uint32_t g_sensor_reinit_tick = 0U;
static uint8_t g_cloud_start_pending = 1U;
static uint32_t g_cloud_start_tick = 0U;
static float g_voltage_threshold_v = 4.20f;

static uint8_t TickExpired(uint32_t now, uint32_t deadline)
{
  /* 用带符号差值避免Tick溢出引发判断错误 */
  return (int32_t)(now - deadline) >= 0;
}

static void Alarm_Start(uint32_t now)
{
  /* 启动报警：进入闪烁阶段 */
  g_alarm_state = ALARM_BLINKING;
  g_alarm_start_tick = now;
  g_alarm_last_blink_tick = now;
}

static void Alarm_Clear(uint32_t now)
{
  /* 手动消警：恢复空闲态并开启防重复触发窗口 */
  g_alarm_state = ALARM_IDLE;
  g_alarm_guard_until = now + ALARM_RETRIGGER_GUARD_MS;
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
}

static void Alarm_ApplyOutputs(void)
{
  /* PA1与报警状态保持一致：仅在锁存报警时为低电平 */
  if (g_alarm_state == ALARM_LATCHED)
  {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
  }
  else
  {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
    if (g_alarm_state == ALARM_IDLE)
    {
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    }
  }
}

static void Alarm_Update(uint32_t now)
{
  if (g_alarm_state == ALARM_BLINKING)
  {
    /* 闪烁期间按节拍翻转PC13 */
    if ((now - g_alarm_last_blink_tick) >= ALARM_BLINK_PERIOD_MS)
    {
      g_alarm_last_blink_tick = now;
      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    }

    /* 3秒到达后转入锁存报警：PA1拉低 */
    if ((now - g_alarm_start_tick) >= ALARM_BLINK_TIME_MS)
    {
      g_alarm_state = ALARM_LATCHED;
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
    }
  }
}

static const char *Alarm_StateText(alarm_state_t s)
{
  if (s == ALARM_BLINKING) return "BLK";
  if (s == ALARM_LATCHED)  return "LAT";
  return "IDL";
}
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
      
	  char text[24];
    uint8_t dht_temp = 0;
  uint8_t dht_humi = 0;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  delay_init(72);
	delay_ms(100);
  USART1_StartReceive_IT();
  USART2_StartReceive_IT();
	OLED_GPIO_Init();
	OLED_Init();
	OLED_Clear();

  DHT11_Init();
	Key_Init();
	if (INA226_Init() == 1)
    {
//        HAL_GPIO_WritePin(GPIOC,GPIO_PIN_13,0);
    }
    else
    {
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
		}
  HAL_Delay(2000);
  g_pa1_prev_level = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_SET) ? 1U : 0U;
  /* 首次系统上电后，ESP01S 延时1秒再启动连接流程 */
  g_cloud_start_pending = 1U;
  g_cloud_start_tick = HAL_GetTick() + 1000U;
  g_alarm_guard_until = HAL_GetTick() + POWER_RESTORE_MUTE_MS;
  Cloud_ResetLink();
	 
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
		   uint32_t now = HAL_GetTick();
      uint8_t pa1_level;
       /* 调用用户按键状态机，读取标志位事件 */
       Key_Scan_All();

       if (key_pb1_short_evt)
		   {
         key_pb1_short_evt = 0U;
         /* PB1短按：电压阈值-0.1V */
         g_voltage_threshold_v -= VOL_THRESHOLD_STEP_V;
         if (g_voltage_threshold_v < VOL_THRESHOLD_MIN_V)
		     {
           g_voltage_threshold_v = VOL_THRESHOLD_MIN_V;
		     }
		   }

       if (key_pb0_short_evt)
       {
         key_pb0_short_evt = 0U;
         /* PB0按下：电压阈值+0.1V */
         g_voltage_threshold_v += VOL_THRESHOLD_STEP_V;
         if (g_voltage_threshold_v > VOL_THRESHOLD_MAX_V)
         {
           g_voltage_threshold_v = VOL_THRESHOLD_MAX_V;
         }
       }

       if (key_pb1_long_evt)
		   {
         key_pb1_long_evt = 0U;
         /* PB1长按：关闭报警并恢复输出 */
		     Alarm_Clear(now);
		   }

       if (key_pb0_long_evt)
       {
         key_pb0_long_evt = 0U;
         /* PB0长按：手动开启报警流程 */
         Alarm_Start(now);
       }

      /* 仅在继电器输出上电时读取传感器，避免断电时通讯异常 */
      pa1_level = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_SET) ? 1U : 0U;
      if (pa1_level == 1U)
      {
        bus_voltage = INA226_Get_BusVoltage();
        current_ma  = INA226_Get_Current();
        power_mw    = INA226_Get_Power();
      }
      else
      {
        bus_voltage = 0.0f;
        current_ma  = 0.0f;
        power_mw    = 0.0f;
        temp = 0.0f;
        humi = 0.0f;
      }
      if ((g_alarm_state == ALARM_IDLE) && TickExpired(now, g_alarm_guard_until))
      {
        /* 触发条件：电压超阈值 */
        if (bus_voltage > g_voltage_threshold_v)
        {
          Alarm_Start(now);
        }
      }

      Alarm_Update(now);
      Alarm_ApplyOutputs();

      pa1_level = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_SET) ? 1U : 0U;
      if ((g_pa1_prev_level == 1U) && (pa1_level == 0U))
      {
        /* 负载/模块掉电后，强制云状态机回到未连接，避免假在线 */
        Cloud_ResetLink();
      }
      if ((g_pa1_prev_level == 0U) && (pa1_level == 1U))
      {
        /* 继电器重新上电后，延时再重初始化外设，避免刚上电即通讯失败 */
        g_sensor_reinit_pending = 1U;
        g_sensor_reinit_tick = now + 300U;
        /* 继电器恢复上电后，ESP01S 延时1秒再启动连接 */
        g_cloud_start_pending = 1U;
        g_cloud_start_tick = now + 1000U;
        g_alarm_guard_until = now + POWER_RESTORE_MUTE_MS;
        Cloud_ResetLink();
      }
      g_pa1_prev_level = pa1_level;

      if (g_sensor_reinit_pending && TickExpired(now, g_sensor_reinit_tick))
      {
        DHT11_Init();
        INA226_Init();
        g_sensor_reinit_pending = 0U;
      }

      if (g_cloud_start_pending && TickExpired(now, g_cloud_start_tick))
      {
        g_cloud_start_pending = 0U;
      }

      if ((pa1_level == 1U) && (DHT11_ReadData(&dht_temp, &dht_humi) == 1))
      {
        temp = (float)dht_temp;
        humi = (float)dht_humi;
      }
      if ((pa1_level == 1U) && (g_cloud_start_pending == 0U))
      {
        Cloud_IoT_Loop();
        Cloud_Upload_Handler();
        Cloud_Handle_Message();
      }

  snprintf(text, sizeof(text), "V:%5.2f T:%4.1f   ", bus_voltage, temp);
  OLED_ShowString(0, 0, (uint8_t *)text, 12);
  snprintf(text, sizeof(text), "I:%7.2f mA   ", current_ma);
  OLED_ShowString(0, 1, (uint8_t *)text, 12);
  snprintf(text, sizeof(text), "ALM:%s PA1:%s   ", Alarm_StateText(g_alarm_state), (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_RESET) ? "L" : "H");
  OLED_ShowString(0, 2, (uint8_t *)text, 12);
  snprintf(text, sizeof(text), "VTH:%3.1f NET:%s ", g_voltage_threshold_v, (NET_State == 2) ? "yes" : "no");
  OLED_ShowString(0, 3, (uint8_t *)text, 12);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
