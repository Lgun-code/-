#ifndef __CLOUD_SERVICE_H
#define __CLOUD_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "usart.h"
#include "mqtt.h"   // 包含你的MQTT头文件
#include "cJSON.h"  // 包含cJSON头文件
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* ================= 用户配置区域 ================= */
#define WIFI_SSID       "abcd"          
#define WIFI_PASSWORD   "88888888"      
#define MQTT_BROKER_IP  "mqtts.heclouds.com"      // OneNET IP
#define MQTT_BROKER_PORT 1883                
#define USERNAME				"zLsy5u45WJ"							//ONTNET产品ID
#define CLIENTID				"ddd"							//ONENET设备名称
#define PASSWORD				"version=2018-10-31&res=products%2FzLsy5u45WJ%2Fdevices%2Fddd&et=1882951979&method=md5&sign=%2FKhP3TKhiejrEBtTVkxitw%3D%3D"		//ONENET token
                                 
#define PUBTOPIC				"$sys/zLsy5u45WJ/ddd/thing/property/post"					//发布
#define SUBTOPIC				"$sys/zLsy5u45WJ/ddd/thing/property/set"					//订阅
#define SUB_REPLY				"$sys/zLsy5u45WJ/ddd/thing/property/set_reply"		//响应下发

#define UPLOAD_INTERVAL  5000 

extern uint8_t NET_State; // 0:未连接, 1:透传中, 2:MQTT在线


void Cloud_IoT_Loop(void);      
void Cloud_Handle_Message(void);
void Cloud_Upload_Handler(void);
void Cloud_SendBytes(uint8_t *data, uint16_t len);
void Cloud_ResetLink(void);
#ifdef __cplusplus
}
#endif

#endif 
