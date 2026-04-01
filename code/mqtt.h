#ifndef __MQTT_H_
#define __MQTT_H_

#define PARA_SIZE 512
#define BUFF_SIZE 512
#define TOPIC_SIZE 256
#define DATA_SIZE 256
typedef struct{
	unsigned int Fixed_len;					// 固定报头长度 
	unsigned int Variable_len;			// 可变报头长度 
	unsigned int Payload_len;				// 负载长度 
	unsigned int MessageID;					// 报文标识符 
	unsigned int length;						// 报文整体长度 
	unsigned int Remain_len;				// 剩余长度 
	unsigned char buff[BUFF_SIZE];	// 数据缓冲区 
	char ClientID[PARA_SIZE];				// 参数缓冲区 
	char Username[PARA_SIZE];				// 参数缓冲区 
	char Passward[PARA_SIZE];				// 参数缓冲区 
	char WillTopic[PARA_SIZE];			// 遗嘱主题缓冲区
	char WillData[PARA_SIZE];				// 遗嘱数据缓冲区 
	char topic[TOPIC_SIZE];					// 接收到服务器推送的3号报文 提取topic到缓冲区 
	unsigned char data[DATA_SIZE];	// 接收到服务器推送的3号报文 提取data到缓冲区 
}MQTT_CB;	

extern MQTT_CB mqtt;

void MQTT_Init(void);
void MQTT_Connect(unsigned int keepalive);
void MQTT_ConnectWill(unsigned int keepalive, unsigned char willretain, unsigned char willQS, unsigned char cleansession);
int MQTT_CONNACK(unsigned char *rxdata, unsigned int rxdata_len); 
void MQTT_DISCONNECT(void);
void MQTT_SUBSCRIBE(char *topic,char Qs); 
int MQTT_SUBACK(unsigned char *rxdata, unsigned int rxdata_len);
void MQTT_UNSUBSCRIBE(char *topic); 
int MQTT_UNSUBACK(unsigned char *rxdata, unsigned int rxdata_len);
void MQTT_PINGREQ(void);
int MQTT_PINGRESP(unsigned char *rxdata, unsigned int rxdata_len);
void MQTT_PUBLISH_LEVEL0(char retain,char *topic,unsigned char *data,unsigned int data_len);
void MQTT_PUBLISH_LEVEL1(char dup,char retain,char *topic,unsigned char *data,unsigned int data_len);
void MQTT_PUBLISH_LEVEL2(char dup,char retain,char *topic,unsigned char *data,unsigned int data_len); 
int MQTT_ProcessPUBLISH(unsigned char *rxdata, unsigned int rxdata_len,unsigned char *qs,unsigned int *messageid);
void MQTT_PUBACK(unsigned int messageid);
int MQTT_ProcessPUBACK(unsigned char *rxdata, unsigned int rxdata_len,unsigned int *messageid);
void MQTT_PUBREC(unsigned int messageid);
int MQTT_ProcessPUBREC(unsigned char *rxdata, unsigned int rxdata_len,unsigned int *messageid);
void MQTT_PUBREL(unsigned int messageid);
int MQTT_ProcessPUBREL(unsigned char *rxdata, unsigned int rxdata_len,unsigned int *messageid);
void MQTT_PUBCOMP(unsigned int messageid); 
int MQTT_ProcessPUBCOMP(unsigned char *rxdata, unsigned int rxdata_len,unsigned int *messageid);

void MQTT_Analysis(void);
#endif
