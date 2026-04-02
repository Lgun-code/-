#include "cloud_service.h"
#include "main.h"
// /* 引用外部变量 */
extern float temp;
extern float humi;

extern UART_HandleTypeDef huart2;
extern usart2_rx_t g_usart2_rx;
extern float bus_voltage;
extern float current_ma;    
extern float power_mw;


/* 内部变量 */
bool baojing = false;
bool fire = true;
bool wring = true;

static uint8_t NET_Step = 0;  
uint8_t NET_State = 0;  
static uint32_t last_upload_tick = 0; // 上传时间戳
static uint8_t g_baojing_force_upload_cnt = 0;

void Cloud_SetBaojing(bool state)
{
    if (baojing != state)
    {
        baojing = state;
        /* 状态变化后连续触发几次即时上报，提高云端接收成功率 */
        g_baojing_force_upload_cnt = 3;
        last_upload_tick = 0;
    }
}

/* ================= 辅助函数 ================= */

// 通过串口发送原始字节 (用于发送 MQTT 报文)
void Cloud_SendBytes(uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(&huart2, data, len, 1000);
}

void Cloud_ResetLink(void)
{
    NET_Step = 0;
    NET_State = 0;
    last_upload_tick = 0;
    g_baojing_force_upload_cnt = 0;
    g_usart2_rx.len = 0;
    g_usart2_rx.flag = 0;
    memset(g_usart2_rx.buf, 0, USART2_RX_BUF_SIZE);
}

static uint8_t Cloud_Check_AT(char *cmd, char *ack, uint16_t timeout_ms)
{
    g_usart2_rx.len = 0;
    g_usart2_rx.flag = 0;
    memset(g_usart2_rx.buf, 0, USART2_RX_BUF_SIZE);

    HAL_UART_Transmit(&huart2, (uint8_t *)cmd, strlen(cmd), 500);

    uint32_t start_tick = HAL_GetTick();
    
    while ((HAL_GetTick() - start_tick) < timeout_ms)
    {
        // 如果接收标志位变了 (说明 ESP8266 回复了数据)
        if (g_usart2_rx.flag == 1)
        {
            // 检查是否包含期望的回复 (比如 "OK")
            if (strstr((char *)g_usart2_rx.buf, ack) != NULL)
            {
                return 1; // 成功！立即返回，不再浪费时间
            }
            
            HAL_Delay(10); 
        }
    }

    return 0; // 超时未收到
}

/* ================= JSON 构建函数 (你的代码移植) ================= */
void json_begin(char *buf, uint16_t size) {
    snprintf(buf, size, "{\"id\":\"123\",\"version\":\"1.0\",\"params\":{");
}
void json_add_float(char *buf, uint16_t size, const char *key, float value) {
    char temp[64];
    snprintf(temp, sizeof(temp), "\"%s\":{\"value\":%.1f},", key, value);
    strncat(buf, temp, size - strlen(buf) - 1);
}
void json_add_int(char *buf, uint16_t size, const char *key, int value) {
    char temp[64];
    snprintf(temp, sizeof(temp), "\"%s\":{\"value\":%d},", key, value);
    strncat(buf, temp, size - strlen(buf) - 1);
}
void json_add_bool(char *buf, uint16_t size, const char *key, const char *value) {
    char temp[64];
    snprintf(temp, sizeof(temp), "\"%s\":{\"value\":%s},", key, value);
    strncat(buf, temp, size - strlen(buf) - 1);
}
void json_add_str(char *buf, uint16_t size, const char *key, const char *value) {
    char temp[64];
    snprintf(temp, sizeof(temp), "\"%s\":{\"value\":\"%s\"},", key, value);
    strncat(buf, temp, size - strlen(buf) - 1);
}
void json_end(char *buf) {
    uint16_t len = strlen(buf);
    if (buf[len - 1] == ',') {
        buf[len - 1] = '}'; buf[len] = '}'; buf[len + 1] = '\0';
    } else {
        strcat(buf, "}}");
    }
}

/* ================= 核心状态机 ================= */
void Cloud_IoT_Loop(void)
{
    char temp_buf[128];

    if (NET_State == 2) return;

    switch (NET_Step)
    {
    case 0: // 复位
        printf("Step 0: Reset ESP8266...\r\n");
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
        HAL_Delay(250);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
        HAL_Delay(500);
        if (Cloud_Check_AT("AT\r\n", "OK", 500)) NET_Step++;
        break;

    case 1: // 模式设置
        printf("Step 1: CWMODE=1\r\n");
        if (Cloud_Check_AT("AT+CWMODE=1\r\n", "OK", 500)) NET_Step++;
        break;

    case 2: // 连接路由
        printf("Step 2: Connect Wi-Fi...\r\n");
        snprintf(temp_buf, sizeof(temp_buf), "AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_SSID, WIFI_PASSWORD);
        if (Cloud_Check_AT(temp_buf, "OK", 10000)) NET_Step++;
        else printf("Wi-Fi Fail, Retrying...\r\n");
        break;

    case 3: // 连接 TCP
        printf("Step 3: Connect TCP...\r\n");
        snprintf(temp_buf, sizeof(temp_buf), "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", MQTT_BROKER_IP, MQTT_BROKER_PORT);
        if (Cloud_Check_AT(temp_buf, "OK", 5000)) NET_Step++;
        break;

    case 4: // 透传模式
        printf("Step 4: CIPMODE=1\r\n");
        if (Cloud_Check_AT("AT+CIPMODE=1\r\n", "OK", 500)) NET_Step++;
        break;

    case 5: // 开启发送 & MQTT 登录
        printf("Step 5: Enter Throughput & MQTT Connect...\r\n");
        if (Cloud_Check_AT("AT+CIPSEND\r\n", ">", 500))
        {
            memset(g_usart2_rx.buf, 0, USART2_RX_BUF_SIZE);
            g_usart2_rx.len = 0;
            g_usart2_rx.flag = 0;
            
            NET_State = 1; 
            MQTT_Init();
            MQTT_Connect(100); 
            Cloud_SendBytes(mqtt.buff, mqtt.length);
            printf("MQTT Connect Packet Sent.\r\n");

            uint32_t wait_start = HAL_GetTick();
            while(g_usart2_rx.flag == 0)
            {
                if(HAL_GetTick() - wait_start > 2000) break; // 超时退出
            }
            

            if (MQTT_CONNACK(g_usart2_rx.buf, g_usart2_rx.len) == 0) 
            {
                printf("MQTT Connected (Fast)!\r\n");
                
                // 清理
                memset(g_usart2_rx.buf, 0, USART2_RX_BUF_SIZE);
                g_usart2_rx.len = 0;
                g_usart2_rx.flag = 0;
                
                // 发送订阅
                MQTT_SUBSCRIBE(SUBTOPIC, 0);
                Cloud_SendBytes(mqtt.buff, mqtt.length);
                printf("Subscribing...\r\n");

                wait_start = HAL_GetTick();
                while(g_usart2_rx.flag == 0)
                {
                    if(HAL_GetTick() - wait_start > 2000) break;
                }

                if(g_usart2_rx.buf[0] == 0x90)
                {
                     printf("Subscribed (Fast)!\r\n");
                     NET_State = 2; 
                     NET_Step++;
                }
                else
                {
                    NET_State = 2; 
                    NET_Step++;
                }
            }
            else
            {
                printf("Connect Failed.\r\n");
                NET_Step = 0; 
            }
        }
        break;
    }
}

/* ================= 业务逻辑：数据上传 ================= */
void Cloud_Upload_Handler(void)
{
    uint8_t do_upload = 0;

    if (NET_State != 2) return;

    if (HAL_GetTick() - last_upload_tick >= UPLOAD_INTERVAL)
    {
        do_upload = 1;
    }

    if (g_baojing_force_upload_cnt > 0)
    {
        do_upload = 1;
    }

    if (do_upload)
    {
        last_upload_tick = HAL_GetTick();

        char buff[512] = {0}; 
        
    //    float temp = 20.5; 
    //    float humi = 66.6;
        
        
        // 构建 JSON
        json_begin(buff, sizeof(buff));
        json_add_float(buff, sizeof(buff), "temp", temp);
        json_add_float(buff, sizeof(buff), "humi", humi);

        json_add_float(buff, sizeof(buff), "I", current_ma);
        json_add_float(buff, sizeof(buff), "V", bus_voltage);
        json_add_bool(buff, sizeof(buff), "baojing", baojing ? "true" : "false");
		// 		json_add_bool(buff, sizeof(buff), "fire", fire ? "true" : "false");
		// 		json_add_bool(buff, sizeof(buff), "wring", wring ? "true" : "false");
        json_end(buff);

        MQTT_PUBLISH_LEVEL0(0, PUBTOPIC, (unsigned char*)buff, strlen(buff));
        
        Cloud_SendBytes(mqtt.buff, mqtt.length);

        if (g_baojing_force_upload_cnt > 0)
        {
            g_baojing_force_upload_cnt--;
        }

        // printf("Data Uploaded: %s\r\n", buff);
    }
}

/* ================= 业务逻辑：数据解析 ================= */
void Cloud_Handle_Message(void)
{
    if (g_usart2_rx.flag != 1) return;

//    printf("\r\n[RX Debug] Event! Length: %d\r\n", g_usart2_rx.len);

//    printf("[RX Debug] Hex Data: ");
//    for (int i = 0; i < g_usart2_rx.len; i++)
//    {
//        printf("%02X ", g_usart2_rx.buf[i]);
//    }
//    printf("\r\n");

    char *json_start = NULL;
    for (int i = 0; i < g_usart2_rx.len; i++)
    {
        if (g_usart2_rx.buf[i] == '{')
        {
            json_start = (char *)&g_usart2_rx.buf[i];
            break; 
        }
    }
    
    if (json_start != NULL)
    {
        printf("[RX Debug] Found JSON: %s\r\n", json_start);

        cJSON *root = cJSON_Parse(json_start);
        if (root)
        {
            cJSON *id_item = cJSON_GetObjectItem(root, "id");
            char msg_id[32] = {0};
            if (cJSON_IsString(id_item)) {
                strncpy(msg_id, id_item->valuestring, sizeof(msg_id) - 1);
            }

            cJSON *params = cJSON_GetObjectItem(root, "params");
            if (params)
            {
                cJSON *led = cJSON_GetObjectItem(params, "LED_Switch");
                if (led)
                {
                    bool state = false;
                    if (cJSON_IsBool(led)) state = cJSON_IsTrue(led);
                    else if (cJSON_IsNumber(led)) state = (led->valueint == 1);

                    if (state) {
                        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); // 实际控制代码
                        printf(">>> CMD EXEC: LED ON <<<\r\n");
                    } else {
                        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   // 实际控制代码
                        printf(">>> CMD EXEC: LED OFF <<<\r\n");
                    }
                }
            }
            
            if (strlen(msg_id) > 0)
            {
                char reply[128];
                snprintf(reply, sizeof(reply), "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}", msg_id);
                
                MQTT_PUBLISH_LEVEL0(0, SUB_REPLY, (unsigned char*)reply, strlen(reply));
                
                Cloud_SendBytes(mqtt.buff, mqtt.length);
                printf("[RX Debug] Replied to OneNET: %s\r\n", reply);
            }

            cJSON_Delete(root);
        }
        else
        {
            printf("[RX Debug] Error: cJSON Parse Failed!\r\n");
        }
    }
    else
    {
    }

    g_usart2_rx.len = 0;
    g_usart2_rx.flag = 0;
    memset(g_usart2_rx.buf, 0, USART2_RX_BUF_SIZE);
}
