# Code Review: STM32 Lithium Battery Monitoring System

**Project:** 3-31_lidianchi (Lithium Battery Test)
**Date:** April 2, 2026
**Reviewer:** Claude Code Assistant

---

## 📋 Executive Summary

This is a comprehensive embedded system project for monitoring lithium batteries using an STM32F1xx microcontroller. The system integrates multiple sensors, cloud connectivity via MQTT, and an alarm system for voltage protection. Overall code quality is **good** with several areas identified for improvement.

**Overall Rating:** ⭐⭐⭐⭐ (4/5)

---

## 🏗️ Architecture Overview

### System Components
1. **Main Controller** (STM32F1xx)
2. **Sensors:**
   - INA226: Current/voltage/power monitoring
   - DHT11: Temperature & humidity sensor
3. **Connectivity:** ESP01S (ESP8266) WiFi module for MQTT/IoT
4. **User Interface:**
   - OLED display (I2C)
   - 2 push buttons (PB0, PB1)
   - LED indicator (PC13)
5. **Protection:** Relay control via PA1 for load cutoff

### Key Features
- Real-time voltage, current, power monitoring
- Configurable voltage threshold with button control (2.5V - 5.0V)
- Alarm system with 3-stage protection (idle → blinking → latched)
- Cloud data upload (5-second interval)
- Remote LED control via MQTT
- Automatic sensor re-initialization after power cycling

---

## 🔍 Detailed Code Review

### 1. Main Application (main.c)

#### ✅ Strengths

1. **Well-structured alarm state machine** (lines 54-136)
   - Clear state transitions: IDLE → BLINKING → LATCHED
   - Proper use of tick-based timing to avoid blocking
   - Guard period to prevent alarm re-triggering (3 seconds)
   - Power restore muting (8 seconds) for system stability

2. **Robust power cycling detection** (lines 302-319)
   - Monitors PA1 relay output state changes
   - Delays sensor re-initialization (300ms) after power restore
   - Cloud reconnection logic after power loss
   - Prevents sensor communication errors during power transitions

3. **Smart sensor reading logic** (lines 274-289)
   - Only reads sensors when relay is active (PA1 high)
   - Sets values to 0 when power is off
   - Prevents communication errors with powered-down sensors

#### ⚠️ Issues & Recommendations

**CRITICAL ISSUES:**

1. **Buffer overflow vulnerability** (line 29)
   ```c
   #include "stdio.h"  // 如果需要用 printf 打印调试，需要包含这个
   ```
   - Comment contains corrupted characters (encoding issue)
   - Multiple locations with garbled Chinese comments throughout codebase
   - **Fix:** Re-save all files with UTF-8 encoding

2. **Unused variables** (lines 37-42)
   ```c
   float temp,humi;
   extern uint8_t NET_State;
   float bus_voltage = 0.0f;
   float current_ma = 0.0f;
   float power_mw = 0.0f;
   // extern uint8_t NET_Step; // 连接步骤状态机变量
   ```
   - Global float variables defined in PTD section (should be in PV section)
   - Inconsistent indentation
   - **Fix:** Move to proper section, ensure consistent formatting

3. **Magic numbers in alarm logic** (lines 44-52)
   - Hard-coded thresholds without clear rationale
   - **Fix:** Add comments explaining why 4.20V is default, typical Li-ion ranges

4. **Potential race condition** (lines 333-337)
   ```c
   if ((pa1_level == 1U) && (DHT11_ReadData(&dht_temp, &dht_humi) == 1))
   {
       temp = (float)dht_temp;
       humi = (float)dht_humi;
   }
   ```
   - DHT11_ReadData returns 0 on success, 1 on failure (check dht11.c:99-100)
   - Logic is **inverted** - only updates on failure!
   - **CRITICAL BUG:** Temperature/humidity never update correctly
   - **Fix:**
   ```c
   if ((pa1_level == 1U) && (DHT11_ReadData(&dht_temp, &dht_humi) == 0))
   ```

5. **Missing error handling** (lines 211-218)
   ```c
   if (INA226_Init() == 1)
   {
       // Empty - commented out LED control
   }
   else
   {
       HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
   }
   ```
   - Success case is empty
   - Failure only lights LED, doesn't prevent operation
   - **Fix:** Add proper error handling or remove empty block

**MEDIUM PRIORITY:**

6. **Long main loop without timing control** (lines 231-353)
   - Main while loop has no explicit delay
   - Could cause excessive CPU usage
   - Different operations happen at different rates without clear timing
   - **Fix:** Add systematic timing management or state machine timing

7. **Display updates on every loop iteration** (lines 345-352)
   - OLED updates happen continuously without rate limiting
   - Can cause flickering and waste power
   - **Fix:** Rate limit display updates (e.g., 100ms interval)

8. **Inconsistent code style**
   - Mixed indentation (spaces vs tabs)
   - Inconsistent brace placement
   - **Fix:** Apply consistent code formatting

---

### 2. INA226 Driver (ina226.c/h)

#### ✅ Strengths

1. **Well-documented configuration** (ina226.h:6-37)
   - Clear pin definitions with macros
   - Calibration calculation formula documented
   - User-configurable shunt resistor value

2. **Proper I2C implementation**
   - Software I2C with bit-banging
   - Proper start/stop conditions
   - ACK/NACK handling
   - Dynamic GPIO mode switching (input/output)

3. **Signed current support** (ina226.c:272-282)
   - Properly handles negative currents using int16_t
   - Supports bidirectional current measurement

#### ⚠️ Issues & Recommendations

1. **Fixed timeout values** (ina226.c:69-86)
   ```c
   uint8_t ucErrTime = 0;
   while(IIC_SDA_READ())
   {
       ucErrTime++;
       if(ucErrTime > 250)
       {
           INA226_IIC_Stop();
           return 1;
       }
   }
   ```
   - Hard-coded timeout (250 iterations)
   - No delay in loop - timeout duration depends on CPU speed
   - **Fix:** Use tick-based timeout or add delay_us()

2. **No error propagation** (ina226.c:160-172)
   - INA226_WriteReg() doesn't check Wait_Ack() return value
   - Continues writing even if slave doesn't ACK
   - **Fix:** Add error checking and propagate failures

3. **Magic number in configuration** (ina226.c:248)
   ```c
   INA226_WriteReg(REG_CONFIG, 0x4527);
   ```
   - Configuration value not explained
   - Should use bit-field macros or constants
   - **Fix:** Define CONFIG bits with meaningful names

---

### 3. DHT11 Driver (dht11.c/h)

#### ✅ Strengths

1. **DWT-based microsecond timing** (dht11.c:7-21)
   - Uses hardware cycle counter
   - More accurate than software delays
   - Self-initializing

2. **Proper protocol implementation**
   - Correct timing for DHT11 communication
   - Dynamic GPIO mode switching
   - Timeout protection

3. **Good error codes** (dht11.c:99)
   - Returns 0 for success
   - Returns 1 for communication failure
   - Returns 2 for checksum failure

#### ⚠️ Issues & Recommendations

1. **Return value documentation inconsistency** (dht11.c:99)
   ```c
   // 返回: 0=成功, 1=传感器无响应, 2=校验错误
   uint8_t DHT11_ReadData(uint8_t *temp, uint8_t *humi)
   ```
   - Documentation says 0=success
   - But main.c line 333 checks for return value 1 as success
   - **This causes the critical bug in main.c**
   - **Fix:** Ensure documentation matches implementation

2. **Long blocking delay** (dht11.c:60)
   ```c
   HAL_Delay(200); // 上电等待 1s
   ```
   - Comment says 1s, code uses 200ms (inconsistency)
   - Blocks for 200ms during initialization
   - **Fix:** Update comment to match code

3. **Timeout loop without delay** (dht11.c:72-75)
   ```c
   uint32_t timeout = 1000;
   while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET) {
       if (--timeout == 0) return 0;
       DHT11_Delay_us(1);
   }
   ```
   - Returns 0 on timeout (same as success!)
   - **Fix:** Return error code 1 on timeout for clarity

---

### 4. Key Handling (key.c/h)

#### ✅ Strengths

1. **Robust debouncing** (key.c:21-89)
   - Proper state machine implementation
   - 30ms debounce time
   - Handles both short and long press
   - Non-blocking design

2. **Event-based architecture** (key.h:22-25)
   - Uses volatile flags for main loop
   - Clean separation of concerns
   - Easy to integrate

3. **Configurable timing** (key.h:8-10)
   - Define-based configuration
   - Easy to tune

#### ⚠️ Issues & Recommendations

1. **Unused global variables** (key.c:9-10)
   ```c
   int key_flag1=0;
   int key_flag2=0;
   ```
   - Not used anywhere in codebase
   - **Fix:** Remove or document purpose

2. **Missing volatile on keys array** (key.c:5-8)
   ```c
   Key_t keys[KEY_NUM] = {
   ```
   - Array modified in scan function, read elsewhere
   - Should be volatile or protected
   - **Fix:** Add volatile qualifier if accessed from interrupt

3. **Inconsistent formatting** (key.c:55-63)
   - Irregular indentation in switch cases
   - **Fix:** Apply consistent formatting

---

### 5. Cloud Service (cloud_service.c/h)

#### ✅ Strengths

1. **Well-structured state machine** (cloud_service.c:103-204)
   - Clear sequential initialization steps
   - Proper error handling with retry logic
   - Fast MQTT connection verification

2. **JSON builder functions** (cloud_service.c:70-100)
   - Simple, lightweight JSON construction
   - No external library dependency for sending
   - Flexible parameter addition

3. **Rate-limited uploads** (cloud_service.c:207-239)
   - 5-second upload interval
   - Prevents network flooding
   - Tick-based timing

#### ⚠️ Issues & Recommendations

**CRITICAL SECURITY ISSUES:**

1. **Hardcoded credentials in source code** (cloud_service.h:17-27)
   ```c
   #define WIFI_SSID       "abcd"
   #define WIFI_PASSWORD   "88888888"
   #define USERNAME        "zLsy5u45WJ"
   #define CLIENTID        "ddd"
   #define PASSWORD        "version=2018-10-31&res=..."
   ```
   - **CRITICAL SECURITY ISSUE**: Credentials exposed in repository
   - WiFi password, MQTT tokens visible in plain text
   - **Fix:** Move to external configuration file (not committed to git)
   - Consider using build-time configuration or external storage

2. **Buffer overflow risk** (cloud_service.c:215)
   ```c
   char buff[512] = {0};
   ```
   - Fixed 512-byte buffer for JSON
   - No bounds checking in json_add_* functions
   - strncat used but not consistently
   - **Fix:** Add proper bounds checking, use snprintf consistently

3. **Missing UART frame detection** (usart.c:70-92)
   ```c
   void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
   {
       if (huart->Instance == USART2)
       {
           if (g_usart2_rx.len < USART2_RX_BUF_SIZE)
           {
               g_usart2_rx.buf[g_usart2_rx.len++] = usart2_rx_temp;
           }
   ```
   - Comments mention IDLE interrupt enabled (line 152-153)
   - But no IDLE interrupt handler implemented
   - g_usart2_rx.flag never set to 1 automatically
   - Cloud relies on this flag (cloud_service.c:244)
   - **Fix:** Implement USART_IRQHandler for IDLE detection

**MEDIUM PRIORITY:**

4. **Blocking delays in state machine** (cloud_service.c:114-117)
   ```c
   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
   HAL_Delay(250);
   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
   HAL_Delay(500);
   ```
   - Uses HAL_Delay() which blocks entire system
   - Prevents alarm updates during connection
   - **Fix:** Convert to non-blocking tick-based delays

5. **Inconsistent timeout handling** (cloud_service.c:158-160)
   ```c
   while(g_usart2_rx.flag == 0)
   {
       if(HAL_GetTick() - wait_start > 2000) break;
   }
   ```
   - No delay in tight loop
   - Wastes CPU cycles
   - **Fix:** Add small delay or use event-based approach

6. **Commented-out debug code** (cloud_service.c:246-253)
   - Should be removed or properly controlled with debug flag
   - **Fix:** Remove or use conditional compilation (#ifdef DEBUG)

---

### 6. UART Communication (usart.c/h)

#### ✅ Strengths

1. **Clean buffer structure** (usart.h:47-62)
   - Well-defined receive buffers
   - Clear flag system
   - Adequate buffer size (1KB)

2. **Printf redirection** (usart.c:24-34)
   - Standard printf works via UART1
   - Good for debugging

#### ⚠️ Issues & Recommendations

1. **Missing IDLE interrupt handler** (Critical!)
   ```c
   __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE); // Line 153
   ```
   - IDLE interrupt enabled but handler not implemented
   - Should be in stm32f1xx_it.c
   - **Fix:** Implement USART2_IRQHandler with IDLE detection:
   ```c
   void USART2_IRQHandler(void)
   {
       if(__HAL_UART_GET_FLAG(&huart2, UART_FLAG_IDLE))
       {
           __HAL_UART_CLEAR_IDLEFLAG(&huart2);
           g_usart2_rx.flag = 1;  // Mark frame complete
       }
       HAL_UART_IRQHandler(&huart2);
   }
   ```

2. **No buffer overflow protection** (usart.c:74-76)
   ```c
   if (g_usart2_rx.len < USART2_RX_BUF_SIZE)
   {
       g_usart2_rx.buf[g_usart2_rx.len++] = usart2_rx_temp;
   }
   ```
   - Silently drops data when buffer full
   - No indication to application
   - **Fix:** Add overflow flag or error callback

3. **Commented code** (usart.c:79, 89)
   - Echo functionality commented out
   - **Fix:** Remove if not needed

---

## 🐛 Critical Bugs Summary

1. **DHT11 return value logic inverted** (main.c:333)
   - Temperature/humidity never update correctly
   - Check for wrong return value

2. **Missing UART IDLE interrupt handler**
   - MQTT frame detection doesn't work
   - Cloud communication likely fails

3. **Security: Hardcoded credentials**
   - WiFi and MQTT credentials in source code
   - Should be externalized

4. **Character encoding issues**
   - Chinese comments corrupted throughout
   - Re-save files as UTF-8

---

## 💡 Recommendations for Improvement

### Short-term (Critical)

1. **Fix DHT11 bug** - Change line 333 in main.c to check for return value 0
2. **Implement IDLE interrupt** - Add proper frame detection for UART2
3. **Move credentials** - Create config.h (gitignored) for secrets
4. **Fix encoding** - Re-save all files as UTF-8

### Medium-term (Quality)

5. **Add error recovery** - Implement watchdog timer
6. **Rate limit display** - Update OLED at fixed 100ms intervals
7. **Add bounds checking** - Protect JSON buffer from overflow
8. **Add unit tests** - Test alarm state machine, key debouncing
9. **Power optimization** - Add sleep modes during idle periods

### Long-term (Architecture)

10. **RTOS consideration** - Consider migrating to FreeRTOS for better task management
11. **Non-blocking I/O** - Convert all blocking delays to async operations
12. **Configuration system** - Add EEPROM storage for thresholds
13. **OTA updates** - Implement firmware update via ESP8266
14. **Logging system** - Add structured logging for debugging

---

## 📊 Code Metrics

- **Total Files Reviewed:** 12 (C/H files in user code)
- **Lines of Code:** ~2,500 (excluding libraries)
- **Critical Bugs:** 4
- **Security Issues:** 1 (critical)
- **Code Duplication:** Low
- **Comment Quality:** Good (when encoding is fixed)
- **Test Coverage:** 0% (no unit tests)

---

## 🎯 Conclusion

This is a **well-structured embedded IoT project** with good separation of concerns and mostly clean code. The main areas requiring immediate attention are:

1. **Critical bug fixes** (DHT11 logic, UART IDLE)
2. **Security hardening** (credential management)
3. **Character encoding** (UTF-8 consistency)

Once these issues are addressed, the system should be production-ready for lithium battery monitoring applications. The alarm system design is particularly well-thought-out, and the cloud integration follows industry standards.

**Recommended Priority:**
1. Fix critical bugs (day 1)
2. Security improvements (day 2-3)
3. Code quality improvements (ongoing)

---

**Review Status:** ✅ Complete
**Next Review:** After implementing critical fixes
