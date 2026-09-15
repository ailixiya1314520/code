#ifndef __THINGSCLOUD_H
#define __THINGSCLOUD_H
#include "sys.h"

/* ---------------- User configuration ---------------- */

/* 0 = ThingsCloud 官方云,  1 = 自建 MQTT Broker（本项目 Java 后端大屏） */
#define TC_SELF_HOSTED      1

/* Router (2.4GHz only, ESP8266 does not support 5GHz) */
#define TC_WIFI_SSID        "wjy"
#define TC_WIFI_PWD         "w2679479160"

#if TC_SELF_HOSTED
/* ---- 自建 Broker：填运行 Java 后端那台电脑的局域网 IP ---- */
#define TC_SERVER           "172.20.10.2"
#define TC_PORT             1883

/* 教室编号 = 设备ID，多台机器请改成 classroom-02 / classroom-03 ... */
#define TC_CLIENTID         "classroom-01"
#define TC_USERNAME         "stm32"
#define TC_PASSWORD         "12345678"

/* 必须和后端 classroom.mqtt 配置一致 */
#define TC_TOPIC_PUB        "classroom/" TC_CLIENTID "/attributes"
#define TC_TOPIC_SUB        "classroom/" TC_CLIENTID "/attributes/push"

#else
/* ---- ThingsCloud 官方云 ---- */
#define TC_SERVER           "gz-5-mqtt.iot-api.com"
#define TC_PORT             1883

#define TC_CLIENTID         "2g2wslq6"
#define TC_USERNAME         "h0g0aq6tqi68pjby"
#define TC_PASSWORD         "Z5JPyQpjSt"

#define TC_TOPIC_PUB        "attributes"        /* device -> cloud */
#define TC_TOPIC_SUB        "attributes/push"   /* cloud  -> device */
#endif

/* Reported attribute identifiers, must match the ones created
 * in the ThingsCloud console (same names as the demo project) */
#define TC_ATTR_TEMP        "Temp"              /* temperature */
#define TC_ATTR_HUMI        "Humi"              /* humidity   */

#define TC_KEEPALIVE        120                 /* seconds */

/* ---------------------------------------------------- */

/* Downlink (cloud -> device) parsed result */
typedef struct
{
    u8  led;             /* 0=off 1=on */
    u8  ledUpdate;       /* 1=new value arrived */
    u8  curtain;         /* 0=close 1=open */
    u8  curtainUpdate;   /* 1=new value arrived */
} TC_Command_t;

extern TC_Command_t gTcCmd;

/* Full bring-up: ESP8266 -> WiFi -> TCP -> MQTT CONNECT -> SUBSCRIBE.
 * Returns 1 on success. Blocking, may take several seconds. */
u8   ThingsCloud_Init(void);

/* Publish a raw JSON string to TC_TOPIC_PUB */
u8   ThingsCloud_Publish(const char *payload);

/* Send MQTT PINGREQ (call it periodically to keep the link alive) */
u8   ThingsCloud_Ping(void);

/* Call this in the main loop: parses any downlink frame into gTcCmd.
 * Returns 1 when a frame was received. */
u8   ThingsCloud_Poll(void);

#endif
