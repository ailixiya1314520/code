#ifndef __MQTT_H
#define __MQTT_H
#include "sys.h"

/* MQTT 3.1.1 packet builder (QoS0 only, no session resend) */

#define MQTT_TX_BUF_SIZE    512

extern u8 mqttTxBuf[MQTT_TX_BUF_SIZE];

/* Encode MQTT remaining length, return number of bytes written */
u16 MQTT_EncodeLength(u8 *buf, int len);

/* CONNECT, return total packet length */
int  MQTT_BuildConnect(u8 *buf, const char *clientId, const char *user, const char *pass, u16 keepAlive);
/* PUBLISH (QoS0), return total packet length */
int  MQTT_BuildPublish(u8 *buf, const char *topic, const char *payload, int payloadLen);
/* SUBSCRIBE (single topic), return total packet length */
int  MQTT_BuildSubscribe(u8 *buf, const char *topic, u8 qos);
/* PINGREQ, return total packet length (always 2) */
int  MQTT_BuildPingReq(u8 *buf);
/* DISCONNECT, return total packet length (always 2) */
int  MQTT_BuildDisconnect(u8 *buf);

#endif
