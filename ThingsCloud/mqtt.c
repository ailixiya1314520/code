#include "mqtt.h"
#include "string.h"

u8 mqttTxBuf[MQTT_TX_BUF_SIZE];

/*
 * Remaining Length: 1~4 bytes, low 7 bits are data, bit7 is continuation flag.
 * Returns number of bytes written to buf.
 */
u16 MQTT_EncodeLength(u8 *buf, int len)
{
    u16 i = 0;
    u8 enc;

    do
    {
        enc = (u8)(len % 128);
        len = len / 128;
        if (len > 0)
            enc |= 0x80;
        buf[i++] = enc;
    } while (len > 0 && i < 4);

    return i;
}

/*
 * CONNECT packet
 * Payload: ClientID [+ UserName] [+ Password]
 * Flags:   bit7 UserName, bit6 Password, bit1 CleanSession
 *          -> 用户名/密码留空时不再置位, 避免发出畸形 CONNECT 包
 *          (broker 开了 allow_anonymous 时可以安全地把账号清空)
 */
int MQTT_BuildConnect(u8 *buf, const char *clientId, const char *user, const char *pass, u16 keepAlive)
{
    int index = 0;
    int i;
    int remain = 0;
    int cidLen = (clientId == NULL) ? 0 : (int)strlen(clientId);
    int usrLen = (user     == NULL) ? 0 : (int)strlen(user);
    int pwdLen = (pass     == NULL) ? 0 : (int)strlen(pass);
    u8  flags  = 0x02;                          /* CleanSession */

    /* variable header = 10 bytes, payload = 2+len for each field present */
    remain = 10;
    remain += 2 + cidLen;
    if (usrLen > 0) { remain += 2 + usrLen; flags |= 0x80; }   /* User Name Flag */
    if (pwdLen > 0) { remain += 2 + pwdLen; flags |= 0x40; }   /* Password Flag */

    buf[index++] = 0x10;                        /* CONNECT */
    index += MQTT_EncodeLength(&buf[index], remain);

    /* variable header */
    buf[index++] = 0x00;                        /* protocol name length MSB */
    buf[index++] = 0x04;                        /* protocol name length LSB */
    buf[index++] = 'M';
    buf[index++] = 'Q';
    buf[index++] = 'T';
    buf[index++] = 'T';
    buf[index++] = 0x04;                        /* protocol level 4 (MQTT 3.1.1) */
    buf[index++] = flags;                       /* connect flags */
    buf[index++] = (u8)(keepAlive >> 8);        /* keep alive MSB */
    buf[index++] = (u8)(keepAlive & 0xFF);      /* keep alive LSB */

    /* payload: client id */
    buf[index++] = (u8)(cidLen >> 8);
    buf[index++] = (u8)(cidLen & 0xFF);
    for (i = 0; i < cidLen; i++)
        buf[index++] = (u8)clientId[i];

    /* payload: user name */
    if (usrLen > 0)
    {
        buf[index++] = (u8)(usrLen >> 8);
        buf[index++] = (u8)(usrLen & 0xFF);
        for (i = 0; i < usrLen; i++)
            buf[index++] = (u8)user[i];
    }

    /* payload: password */
    if (pwdLen > 0)
    {
        buf[index++] = (u8)(pwdLen >> 8);
        buf[index++] = (u8)(pwdLen & 0xFF);
        for (i = 0; i < pwdLen; i++)
            buf[index++] = (u8)pass[i];
    }

    return index;
}

/*
 * PUBLISH packet, QoS0 (no packet identifier)
 */
int MQTT_BuildPublish(u8 *buf, const char *topic, const char *payload, int payloadLen)
{
    int index = 0;
    int i;
    int topicLen = (topic == NULL) ? 0 : (int)strlen(topic);
    int remain   = topicLen + payloadLen + 2;

    buf[index++] = 0x30;                        /* PUBLISH, QoS0 */
    index += MQTT_EncodeLength(&buf[index], remain);

    buf[index++] = (u8)(topicLen >> 8);
    buf[index++] = (u8)(topicLen & 0xFF);
    for (i = 0; i < topicLen; i++)
        buf[index++] = (u8)topic[i];

    for (i = 0; i < payloadLen; i++)
        buf[index++] = (u8)payload[i];

    return index;
}

/*
 * SUBSCRIBE packet, one topic filter
 */
int MQTT_BuildSubscribe(u8 *buf, const char *topic, u8 qos)
{
    int index = 0;
    int i;
    int topicLen = (topic == NULL) ? 0 : (int)strlen(topic);
    int remain   = topicLen + 5;                /* pid(2) + topiclen(2) + topic + qos(1) */

    buf[index++] = 0x82;                        /* SUBSCRIBE */
    index += MQTT_EncodeLength(&buf[index], remain);

    buf[index++] = 0x00;                        /* packet id MSB */
    buf[index++] = 0x01;                        /* packet id LSB */

    buf[index++] = (u8)(topicLen >> 8);
    buf[index++] = (u8)(topicLen & 0xFF);
    for (i = 0; i < topicLen; i++)
        buf[index++] = (u8)topic[i];

    buf[index++] = qos;

    return index;
}

/*
 * PINGREQ: 0xC0 0x00
 */
int MQTT_BuildPingReq(u8 *buf)
{
    buf[0] = 0xC0;
    buf[1] = 0x00;
    return 2;
}

/*
 * DISCONNECT: 0xE0 0x00
 */
int MQTT_BuildDisconnect(u8 *buf)
{
    buf[0] = 0xE0;
    buf[1] = 0x00;
    return 2;
}
