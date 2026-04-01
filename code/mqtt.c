#include "mqtt.h"
#include "string.h"
#include "stdio.h"
#include "main.h"
#include "usart.h"
#include "cJSON.h"
#include <stdbool.h>
//#include "LED.h"
#include "cloud_service.h"

MQTT_CB mqtt;

extern UART_HandleTypeDef huart2;
extern usart2_rx_t g_usart2_rx;

void MQTT_Init(void)
{
	memset(&mqtt,0,sizeof(mqtt));
	sprintf(mqtt.ClientID, CLIENTID);
	sprintf(mqtt.Username, USERNAME);
	strcpy(mqtt.Passward, PASSWORD);
	mqtt.MessageID = 1;
} 

// 客户端连接 (0x10) 没有遗嘱功能 
void MQTT_Connect(unsigned int keepalive){
	mqtt.Variable_len = 10;
	mqtt.Payload_len = 2 + strlen(mqtt.ClientID) 
					 + 2 + strlen(mqtt.Username) 
					 + 2 + strlen(mqtt.Passward);
	mqtt.Remain_len = mqtt.Variable_len + mqtt.Payload_len;
	
	/* 固定报头 */  
	mqtt.buff[0] = 0x10;	// CONNECT报文类型 
	mqtt.Fixed_len = 1;
	/* 剩余长度编码 */ 
	do{
		if(mqtt.Remain_len / 128 == 0){		// 不需要进位 
			mqtt.buff[mqtt.Fixed_len] = mqtt.Remain_len;
		}else{								// 需要进位 
			mqtt.buff[mqtt.Fixed_len] = (mqtt.Remain_len % 128) | 0x80;		// 0x80 为置进位标志位 
		}
		mqtt.Fixed_len++;
		mqtt.Remain_len = mqtt.Remain_len / 128;
	} while(mqtt.Remain_len);
	
	/* 可变报头 */
	unsigned int offset = 0;
	offset = mqtt.Fixed_len;
	mqtt.buff[offset++] = 0x00;
	mqtt.buff[offset++] = 0x04;
	mqtt.buff[offset++] = 0x4D;		// "M"
	mqtt.buff[offset++] = 0x51;		// "Q"
	mqtt.buff[offset++] = 0x54;		// "T"
	mqtt.buff[offset++] = 0x54;		// "T"
	mqtt.buff[offset++] = 0x04;		// 协议等级 (MQTT 3.1.1)
	mqtt.buff[offset++] = 0xC2;		// Connect Flags (用户名、密码、Clean Session)
	mqtt.buff[offset++] = (keepalive >> 8) & 0xFF;	// 保活时间 MSB 
	mqtt.buff[offset++] = (keepalive & 0xFF);		// 保活时间 LSB 
	
	/* 有效载荷 */
	mqtt.buff[offset++] = (strlen(mqtt.ClientID) >> 8) & 0xFF;
	mqtt.buff[offset++] = (strlen(mqtt.ClientID) & 0xFF);
	memcpy(&mqtt.buff[offset],mqtt.ClientID,strlen(mqtt.ClientID));
	offset += strlen(mqtt.ClientID); 
	
	mqtt.buff[offset++] = (strlen(mqtt.Username) >> 8) & 0xFF;
	mqtt.buff[offset++] = (strlen(mqtt.Username) & 0xFF);
	memcpy(&mqtt.buff[offset],mqtt.Username,strlen(mqtt.Username));
	offset += strlen(mqtt.Username); 
	
	mqtt.buff[offset++] = (strlen(mqtt.Passward) >> 8) & 0xFF;
	mqtt.buff[offset++] = (strlen(mqtt.Passward) & 0xFF);
	memcpy(&mqtt.buff[offset],mqtt.Passward,strlen(mqtt.Passward));
	offset += strlen(mqtt.Passward); 
	
	mqtt.length = mqtt.Fixed_len + mqtt.Variable_len + mqtt.Payload_len;
}

// 客户端连接 (0x10) 有遗嘱功能 
void MQTT_ConnectWill(unsigned int keepalive, unsigned char willretain, unsigned char willQS, unsigned char cleansession){
	mqtt.Variable_len = 10;
	mqtt.Payload_len = 2 + strlen(mqtt.ClientID) 
					 + 2 + strlen(mqtt.Username) 
					 + 2 + strlen(mqtt.Passward)
					 + 2 + strlen(mqtt.WillTopic)
					 + 2 + strlen(mqtt.WillData);
	mqtt.Remain_len = mqtt.Variable_len + mqtt.Payload_len;
	
	/* 固定报头 */  
	mqtt.buff[0] = 0x10;	// CONNECT报文类型 
	mqtt.Fixed_len = 1;
	/* 剩余长度编码 */ 
	do{
		if(mqtt.Remain_len / 128 == 0){		// 不需要进位 
			mqtt.buff[mqtt.Fixed_len] = mqtt.Remain_len;
		}else{								// 需要进位 
			mqtt.buff[mqtt.Fixed_len] = (mqtt.Remain_len % 128) | 0x80;		// 0x80 为置进位标志位 
		}
		mqtt.Fixed_len++;
		mqtt.Remain_len = mqtt.Remain_len / 128;
	} while(mqtt.Remain_len);
	
	/* 可变报头 */
	unsigned int offset = 0;
	offset = mqtt.Fixed_len;
	mqtt.buff[offset++] = 0x00;
	mqtt.buff[offset++] = 0x04;
	mqtt.buff[offset++] = 0x4D;		// "M"
	mqtt.buff[offset++] = 0x51;		// "Q"
	mqtt.buff[offset++] = 0x54;		// "T"
	mqtt.buff[offset++] = 0x54;		// "T"
	mqtt.buff[offset++] = 0x04;		// 协议等级 (MQTT 3.1.1)
	mqtt.buff[offset++] = 0xC4 | (willretain << 5) | (willQS << 3) | (cleansession << 1);	// Connect Flags (用户名、密码、Will Flag)
	mqtt.buff[offset++] = (keepalive >> 8) & 0xFF;	// 保活时间 MSB 
	mqtt.buff[offset++] = (keepalive & 0xFF);		// 保活时间 LSB 
	
	/* 有效载荷 */
	// ClientID
	mqtt.buff[offset++] = (strlen(mqtt.ClientID) >> 8) & 0xFF;
	mqtt.buff[offset++] = (strlen(mqtt.ClientID) & 0xFF);
	memcpy(&mqtt.buff[offset],mqtt.ClientID,strlen(mqtt.ClientID));
	offset += strlen(mqtt.ClientID); 
	// WillTopic
	mqtt.buff[offset++] = (strlen(mqtt.WillTopic) >> 8) & 0xFF;
	mqtt.buff[offset++] = (strlen(mqtt.WillTopic) & 0xFF);
	memcpy(&mqtt.buff[offset],mqtt.WillTopic,strlen(mqtt.WillTopic));
	offset += strlen(mqtt.WillTopic); 
	// WillData
	mqtt.buff[offset++] = (strlen(mqtt.WillData) >> 8) & 0xFF;
	mqtt.buff[offset++] = (strlen(mqtt.WillData) & 0xFF);
	memcpy(&mqtt.buff[offset],mqtt.WillData,strlen(mqtt.WillData));
	offset += strlen(mqtt.WillData); 
	// Username
	mqtt.buff[offset++] = (strlen(mqtt.Username) >> 8) & 0xFF;
	mqtt.buff[offset++] = (strlen(mqtt.Username) & 0xFF);
	memcpy(&mqtt.buff[offset],mqtt.Username,strlen(mqtt.Username));
	offset += strlen(mqtt.Username); 
	// Passward
	mqtt.buff[offset++] = (strlen(mqtt.Passward) >> 8) & 0xFF;
	mqtt.buff[offset++] = (strlen(mqtt.Passward) & 0xFF);
	memcpy(&mqtt.buff[offset],mqtt.Passward,strlen(mqtt.Passward));
	offset += strlen(mqtt.Passward); 
	
	mqtt.length = mqtt.Fixed_len + mqtt.Variable_len + mqtt.Payload_len;
}
// 服务器连接确认 (0x20) 
int MQTT_CONNACK(unsigned char *rxdata, unsigned int rxdata_len)
{
	if((rxdata_len == 4)&&(rxdata[0] == 0x20)){
		
	}else{
		return -1;
	}
	return rxdata[3];	// 兼容不同编译环境 无返回值情况 
}
// 取消连接 
void MQTT_DISCONNECT(void)
{
	mqtt.buff[0] = 0xE0;
	mqtt.buff[1] = 0x00;
	mqtt.length = 2;
}
// 订阅 (0x82) 
void MQTT_SUBSCRIBE(char *topic,char Qs)
{
	mqtt.Variable_len = 2;
	mqtt.Payload_len = 2 + strlen(topic) + 1;
	mqtt.Remain_len = mqtt.Variable_len + mqtt.Payload_len;
	
	/* 固定报头 */  
	mqtt.buff[0] = 0x82;	// SUBSCRIBE报文类型 
	mqtt.Fixed_len = 1;
	/* 剩余长度编码 */ 
	do{
		if(mqtt.Remain_len / 128 == 0){		// 不需要进位 
			mqtt.buff[mqtt.Fixed_len] = mqtt.Remain_len;
		}else{								// 需要进位 
			mqtt.buff[mqtt.Fixed_len] = (mqtt.Remain_len % 128) | 0x80;		// 0x80 为置进位标志位 
		}
		mqtt.Fixed_len++;
		mqtt.Remain_len = mqtt.Remain_len / 128;
	} while(mqtt.Remain_len);
	
	/* 可变报头 */
	unsigned int offset = 0;
	offset = mqtt.Fixed_len;
	mqtt.MessageID++;
	if(mqtt.MessageID == 0) 
		mqtt.MessageID = 1;
	mqtt.buff[offset++] = mqtt.MessageID / 256;  // MSB
	mqtt.buff[offset++] = mqtt.MessageID % 256;  // LSB 
	
	/* 有效载荷 */
	mqtt.buff[offset++] = strlen(topic) / 256;
	mqtt.buff[offset++] = strlen(topic) % 256;
	memcpy(&mqtt.buff[offset],topic,strlen(topic));
	offset += strlen(topic);	
	mqtt.buff[offset] = Qs;

	mqtt.length = mqtt.Fixed_len + mqtt.Variable_len + mqtt.Payload_len;	
}
// 订阅确认 (0x90) 
int MQTT_SUBACK(unsigned char *rxdata, unsigned int rxdata_len)
{
	if((rxdata_len == 5)&&(rxdata[0] == 0x90)){
		
	}else{
		return -1;
	}
	return rxdata[4];	// 兼容不同编译环境 无返回值情况 
}

// 取消订阅 (0xA2) 比订阅少一个字节(无质量等级)
void MQTT_UNSUBSCRIBE(char *topic)
{
	mqtt.Variable_len = 2;
	mqtt.Payload_len = 2 + strlen(topic);
	mqtt.Remain_len = mqtt.Variable_len + mqtt.Payload_len;
	
	/* 固定报头 */  
	mqtt.buff[0] = 0xA2;	// UNSUBSCRIBE报文类型 
	mqtt.Fixed_len = 1;
	/* 剩余长度编码 */ 
	do{
		if(mqtt.Remain_len / 128 == 0){		// 不需要进位 
			mqtt.buff[mqtt.Fixed_len] = mqtt.Remain_len;
		}else{								// 需要进位 
			mqtt.buff[mqtt.Fixed_len] = (mqtt.Remain_len % 128) | 0x80;		// 0x80 为置进位标志位 
		}
		mqtt.Fixed_len++;
		mqtt.Remain_len = mqtt.Remain_len / 128;
	} while(mqtt.Remain_len);
	
	/* 可变报头 */
	unsigned int offset = 0;
	offset = mqtt.Fixed_len;
	if(mqtt.MessageID == 0) 
		mqtt.MessageID = 1;
	mqtt.buff[offset++] = mqtt.MessageID / 256;  // MSB
	mqtt.buff[offset++] = mqtt.MessageID % 256;  // LSB 
	
	/* 有效载荷 */
	mqtt.buff[offset++] = strlen(topic) / 256;
	mqtt.buff[offset++] = strlen(topic) % 256;
	memcpy(&mqtt.buff[offset],topic,strlen(topic));
	offset += strlen(topic);	

	mqtt.length = mqtt.Fixed_len + mqtt.Variable_len + mqtt.Payload_len;	
}

// 取消订阅确认 (0xB0) 
int MQTT_UNSUBACK(unsigned char *rxdata, unsigned int rxdata_len)
{
	if((rxdata_len == 4)&&(rxdata[0] == 0xB0)){
		
	}else{
		return -1;
	}
	return 0;	
}
// 心跳请求 
void MQTT_PINGREQ(void)
{
	mqtt.buff[0] = 0xC0;
	mqtt.buff[1] = 0x00;
	mqtt.length = 2;
}

// 心跳响应 
int MQTT_PINGRESP(unsigned char *rxdata, unsigned int rxdata_len)
{
	if((rxdata_len == 2)&&(rxdata[0] == 0xD0)){
		
	}else{
		return -1;
	}
	return 0;	
}


/*
	param[0]	retain: 	设置保留功能(未订阅情况下发送消息 在下次订阅会自动收到消息) 
	param[1]    topic:  	订阅主题 
	param[2]	data: 		消息内容
	param[3]	data_len:  	消息长度 
*/ 
void MQTT_PUBLISH_LEVEL0(char retain,char *topic,unsigned char *data,unsigned int data_len)
{
	mqtt.Variable_len = 2 + strlen(topic);
	mqtt.Payload_len = data_len;
	mqtt.Remain_len = mqtt.Variable_len + mqtt.Payload_len;
	
	/* 固定报头 */  
	mqtt.buff[0] = 0x30 | (retain << 0);	// PUBLISH报文类型 
	mqtt.Fixed_len = 1;
	/* 剩余长度编码 */ 
	do{
		if(mqtt.Remain_len / 128 == 0){		// 不需要进位 
			mqtt.buff[mqtt.Fixed_len] = mqtt.Remain_len;
		}else{								// 需要进位 
			mqtt.buff[mqtt.Fixed_len] = (mqtt.Remain_len % 128) | 0x80;		// 0x80 为置进位标志位 
		}
		mqtt.Fixed_len++;
		mqtt.Remain_len = mqtt.Remain_len / 128;
	} while(mqtt.Remain_len);
	
	unsigned int offset = 0;
	offset = mqtt.Fixed_len;

	/* 可变报头 */
	mqtt.buff[offset++] = (strlen(topic) >> 8) & 0xFF;	// MSB
	mqtt.buff[offset++] = (strlen(topic) & 0xFF);		// LSB
	memcpy(&mqtt.buff[offset],topic,strlen(topic));
	offset += strlen(topic); 
	
	/* 有效载荷 */
	memcpy(&mqtt.buff[offset],data,data_len);
	offset += data_len; 
	
	mqtt.length = mqtt.Fixed_len + mqtt.Variable_len + mqtt.Payload_len;
}

/*
	param[0]	dup:		是否重发 
	param[1]	retain: 	设置保留功能(未订阅情况下发送消息 在下次订阅会自动收到消息) 
	param[2]    topic:  	订阅主题 
	param[3]	data: 		消息内容
	param[4]	data_len:  	消息长度 
*/ 
void MQTT_PUBLISH_LEVEL1(char dup,char retain,char *topic,unsigned char *data,unsigned int data_len)
{
	mqtt.Variable_len = 2 + strlen(topic) + 2;
	mqtt.Payload_len = data_len;
	mqtt.Remain_len = mqtt.Variable_len + mqtt.Payload_len;
	
	/* 固定报头 */  
	mqtt.buff[0] = 0x32 | (dup << 3) | (retain << 0);	// PUBLISH报文类型 
	mqtt.Fixed_len = 1;
	/* 剩余长度编码 */ 
	do{
		if(mqtt.Remain_len / 128 == 0){		// 不需要进位 
			mqtt.buff[mqtt.Fixed_len] = mqtt.Remain_len;
		}else{								// 需要进位 
			mqtt.buff[mqtt.Fixed_len] = (mqtt.Remain_len % 128) | 0x80;		// 0x80 为置进位标志位 
		}
		mqtt.Fixed_len++;
		mqtt.Remain_len = mqtt.Remain_len / 128;
	} while(mqtt.Remain_len);
	
	unsigned int offset = 0;
	offset = mqtt.Fixed_len;

	/* 可变报头 */
	mqtt.buff[offset++] = (strlen(topic) >> 8) & 0xFF;	// MSB
	mqtt.buff[offset++] = (strlen(topic) & 0xFF);		// LSB
	memcpy(&mqtt.buff[offset],topic,strlen(topic));
	offset += strlen(topic);
	 
	// 2字节报文标识符 
	if(mqtt.MessageID == 0) 
		mqtt.MessageID = 1;
	mqtt.buff[offset++] = mqtt.MessageID / 256;  // MSB
	mqtt.buff[offset++] = mqtt.MessageID % 256;  // LSB
	
	/* 有效载荷 */
	memcpy(&mqtt.buff[offset],data,data_len);
	offset += data_len; 
	
	mqtt.length = mqtt.Fixed_len + mqtt.Variable_len + mqtt.Payload_len;
}


/*
	param[0]	dup:		是否重发 
	param[1]	retain: 	设置保留功能(未订阅情况下发送消息 在下次订阅会自动收到消息) 
	param[2]    topic:  	订阅主题 
	param[3]	data: 		消息内容
	param[4]	data_len:  	消息长度 
*/ 
void MQTT_PUBLISH_LEVEL2(char dup,char retain,char *topic,unsigned char *data,unsigned int data_len)
{
	mqtt.Variable_len = 2 + strlen(topic) + 2;
	mqtt.Payload_len = data_len;
	mqtt.Remain_len = mqtt.Variable_len + mqtt.Payload_len;
	
	/* 固定报头 */  
	mqtt.buff[0] = 0x34 | (dup << 3) | (retain << 0);	// PUBLISH报文类型 
	mqtt.Fixed_len = 1;
	/* 剩余长度编码 */ 
	do{
		if(mqtt.Remain_len / 128 == 0){		// 不需要进位 
			mqtt.buff[mqtt.Fixed_len] = mqtt.Remain_len;
		}else{								// 需要进位 
			mqtt.buff[mqtt.Fixed_len] = (mqtt.Remain_len % 128) | 0x80;		// 0x80 为置进位标志位 
		}
		mqtt.Fixed_len++;
		mqtt.Remain_len = mqtt.Remain_len / 128;
	} while(mqtt.Remain_len);
	
	unsigned int offset = 0;
	offset = mqtt.Fixed_len;

	/* 可变报头 */
	mqtt.buff[offset++] = (strlen(topic) >> 8) & 0xFF;	// MSB
	mqtt.buff[offset++] = (strlen(topic) & 0xFF);		// LSB
	memcpy(&mqtt.buff[offset],topic,strlen(topic));
	offset += strlen(topic);
	 
	// 2字节报文标识符 
	if(mqtt.MessageID == 0) 
		mqtt.MessageID = 1;
	mqtt.buff[offset++] = mqtt.MessageID / 256;  // MSB
	mqtt.buff[offset++] = mqtt.MessageID % 256;  // LSB
	
	/* 有效载荷 */
	memcpy(&mqtt.buff[offset],data,data_len);
	offset += data_len; 
	
	mqtt.length = mqtt.Fixed_len + mqtt.Variable_len + mqtt.Payload_len;
}

int MQTT_ProcessPUBLISH(unsigned char *rxdata, unsigned int rxdata_len,unsigned char *qs,unsigned int *messageid)
{
	char i;
	int topic_len;
	int data_len;
	
	if((rxdata[0] & 0xF0) == 0x30){
		for(i = 1;i < 5;i++)
		{
			if((rxdata[i] & 0x80) == 0x00)
				break;
		}
		switch(rxdata[0] & 0x06){
			case 0x00:
				*qs = 0;
				*messageid = 0;
				topic_len = rxdata[1 + i] * 256 + rxdata[1 + i + 1];
				data_len = rxdata_len - 1 - i - 2 - topic_len;
				memset(mqtt.topic,0,TOPIC_SIZE);
				memset(mqtt.data,0,DATA_SIZE);
				memcpy(mqtt.topic,&rxdata[1+i],topic_len + 2);
				mqtt.data[0] = data_len / 256;
				mqtt.data[1] = data_len % 256;
				memcpy(&mqtt.data[2],&rxdata[1 + i + 2 + topic_len],data_len);
				break;
			case 0x02:
				*qs = 1;
				topic_len = rxdata[1 + i] * 256 + rxdata[1 + i + 1];
				*messageid = rxdata[1 + i + 2 + topic_len] * 256 + rxdata[1 + i + 2 + topic_len + 1];
				data_len = rxdata_len - 1 - i - 2 - topic_len - 2;
				memset(mqtt.topic,0,TOPIC_SIZE);
				memset(mqtt.data,0,DATA_SIZE);
				memcpy(mqtt.topic,&rxdata[1+i],topic_len + 2);
				mqtt.data[0] = data_len / 256;
				mqtt.data[1] = data_len % 256;
				memcpy(&mqtt.data[2],&rxdata[1 + i + 2 + topic_len + 2],data_len);
				break;
			case 0x04:
				*qs = 2;
				topic_len = rxdata[1 + i] * 256 + rxdata[1 + i + 1];
				*messageid = rxdata[1 + i + 2 + topic_len] * 256 + rxdata[1 + i + 2 + topic_len + 1];
				data_len = rxdata_len - 1 - i - 2 - topic_len - 2;
				memset(mqtt.topic,0,TOPIC_SIZE);
				memset(mqtt.data,0,DATA_SIZE);
				memcpy(mqtt.topic,&rxdata[1+i],topic_len + 2);
				mqtt.data[0] = data_len / 256;
				mqtt.data[1] = data_len % 256;
				memcpy(&mqtt.data[2],&rxdata[1 + i + 2 + topic_len + 2],data_len);
				break;
		}
	}else{
		return -1;
	}
	return 0;	
}

// Qs=1时 (4号报文)
void MQTT_PUBACK(unsigned int messageid)
{
	mqtt.buff[0] = 0x40;
	mqtt.buff[1] = 0x02;
	mqtt.buff[2] = messageid / 256;
	mqtt.buff[3] = messageid % 256;
	mqtt.length = 4;
}

int MQTT_ProcessPUBACK(unsigned char *rxdata, unsigned int rxdata_len,unsigned int *messageid)
{
	if((rxdata_len == 4)&&(rxdata[0] == 0x40)){
		*messageid = rxdata[2] * 256 + rxdata[3];
	}else{
		return -1;
	}
	return 0;	
}

// Qs=2时 (5号报文)
void MQTT_PUBREC(unsigned int messageid)
{
	mqtt.buff[0] = 0x50;
	mqtt.buff[1] = 0x02;
	mqtt.buff[2] = messageid / 256;
	mqtt.buff[3] = messageid % 256;
	mqtt.length = 4;
}

int MQTT_ProcessPUBREC(unsigned char *rxdata, unsigned int rxdata_len,unsigned int *messageid)
{
	if((rxdata_len == 4)&&(rxdata[0] == 0x50)){
		*messageid = rxdata[2] * 256 + rxdata[3];
	}else{
		return -1;
	}
	return 0;	
}

// (6号报文) 客户端给服务器发送3号报文后 接收到服务器的5号报文 需要再次回复6号报文 
void MQTT_PUBREL(unsigned int messageid)
{
	mqtt.buff[0] = 0x62;
	mqtt.buff[1] = 0x02;
	mqtt.buff[2] = messageid / 256;
	mqtt.buff[3] = messageid % 256;
	mqtt.length = 4;
}

int MQTT_ProcessPUBREL(unsigned char *rxdata, unsigned int rxdata_len,unsigned int *messageid)
{
	if((rxdata_len == 4)&&(rxdata[0] == 0x62)){
		*messageid = rxdata[2] * 256 + rxdata[3];
	}else{
		return -1;
	}
	return 0;	
}

// (7号报文)
void MQTT_PUBCOMP(unsigned int messageid)
{
	mqtt.buff[0] = 0x70;
	mqtt.buff[1] = 0x02;
	mqtt.buff[2] = messageid / 256;
	mqtt.buff[3] = messageid % 256;
	mqtt.length = 4;
}

int MQTT_ProcessPUBCOMP(unsigned char *rxdata, unsigned int rxdata_len,unsigned int *messageid)
{
	if((rxdata_len == 4)&&(rxdata[0] == 0x70)){
		*messageid = rxdata[2] * 256 + rxdata[3];
	}else{
		return -1;
	}
	return 0;	
}
