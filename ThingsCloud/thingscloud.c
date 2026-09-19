#include "thingscloud.h"
#include "esp8266.h"
#include "mqtt.h"
#include "usart3.h"
#include "delay.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"

TC_Command_t gTcCmd;

/* Alarm thresholds live in main.c; the cloud writes them directly */
extern u16 A_DHT11_Temp, A_DHT11_Hum, A_pre, A_gz_value,
           A_m2_value, A_m7_value, A_m135_value;

static const struct { const char *key; u16 *var; } tcThresholds[] = {
    { "A_Temp",        &A_DHT11_Temp },
    { "A_Hum",         &A_DHT11_Hum  },
    { "A_Pre",         &A_pre        },
    { "A_GZ_Value",    &A_gz_value   },
    { "A_MQ2_Value",   &A_m2_value   },
    { "A_MQ7_Value",   &A_m7_value   },
    { "A_MQ135_Value", &A_m135_value },
};
#define TC_THRESHOLD_NUM  (sizeof(tcThresholds) / sizeof(tcThresholds[0]))

/* ------------------------------------------------------------------ */
/* Search for "key":<number> in a JSON string. Returns 1 and writes
 * *out when found, 0 otherwise. */
static u8 JSON_FindU16(const char *json, const char *key, u16 *out)
{
    char pat[24];
    const char *p;

    sprintf(pat, "\"%s\":", key);
    p = strstr(json, pat);
    if (p == NULL) return 0;
    p += strlen(pat);
    while (*p == ' ') p++;
    if (*p < '0' || *p > '9') return 0;
    *out = (u16)atoi(p);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Search for "key":true / "key":false / "key":1 / "key":0 in a JSON
 * string. Returns 1 for true, 0 for false, -1 when the key is absent.  */
static int JSON_FindBool(const char *json, const char *key)
{
    char patT[40];
    char patF[40];

    sprintf(patT, "\"%s\":true",  key);
    sprintf(patF, "\"%s\":false", key);
    if (strstr(json, patT) != NULL) return 1;
    if (strstr(json, patF) != NULL) return 0;

    sprintf(patT, "\"%s\":1", key);
    sprintf(patF, "\"%s\":0", key);
    if (strstr(json, patT) != NULL) return 1;
    if (strstr(json, patF) != NULL) return 0;

    return -1;
}

/* ------------------------------------------------------------------ */
/* Wait for MQTT CONNACK (bytes 0x20 0x02 0x00 rc) inside the raw
 * ESP8266 frame. rc must be 0 (accepted). Returns 1 on success. */
static u8 ThingsCloud_WaitConnack(u16 timeoutMs)
{
    u16 i, n;

    for (i = 0; i < timeoutMs / 10; i++)
    {
        delay_ms(10);
        for (n = 0; n + 3 < USART3_RX_CNT; n++)
        {
            if (USART3_RX_BUF[n] == 0x20 && USART3_RX_BUF[n+1] == 0x02)
            {
                if (USART3_RX_BUF[n+3] == 0x00)
                    return 1;
                /* rc: 1=bad proto 2=id rejected 3=server unavail
                 *     4=bad user/pass 5=not authorized */
                printf("MQTT refused, rc=%d\r\n", USART3_RX_BUF[n+3]);
                return 0;
            }
        }
    }
    printf("no CONNACK (broker did not accept)\r\n");
    return 0;
}

/* ------------------------------------------------------------------ */
u8 ThingsCloud_Init(void)
{
    int len;

    ESP8266_GPIOInit();
    delay_ms(500);

    printf("--- ThingsCloud init ---\r\n");

    if (!ESP8266_TestAT())
    {
        printf("ESP8266 no response!\r\n");
        return 0;
    }

    ESP8266_SendCmd("ATE0", "OK", NULL, 500);      /* echo off */

    if (!ESP8266_SetMode(1))                       /* STA */
    {
        printf("CWMODE failed\r\n");
        return 0;
    }

    printf("joining AP %s ...\r\n", TC_WIFI_SSID);
    if (!ESP8266_JoinAP(TC_WIFI_SSID, TC_WIFI_PWD))
    {
        printf("join AP failed\r\n");
        return 0;
    }

    ESP8266_SendCmd("AT+CIPMUX=0", "OK", NULL, 1000);   /* single link */

    printf("connecting %s:%d ...\r\n", TC_SERVER, TC_PORT);
    if (!ESP8266_ConnectTCP(TC_SERVER, TC_PORT))
    {
        printf("TCP connect failed\r\n");
        return 0;
    }

    /* MQTT CONNECT + wait for CONNACK (this is the real "connected") */
    len = MQTT_BuildConnect(mqttTxBuf, TC_CLIENTID, TC_USERNAME, TC_PASSWORD, TC_KEEPALIVE);
    if (!ESP8266_SendData(mqttTxBuf, (u16)len))
    {
        printf("MQTT CONNECT send failed\r\n");
        return 0;
    }
    if (!ThingsCloud_WaitConnack(3000))
        return 0;

    /* MQTT SUBSCRIBE, dump SUBACK frame for diagnosis */
    len = MQTT_BuildSubscribe(mqttTxBuf, TC_TOPIC_SUB, 0);
    if (!ESP8266_SendData(mqttTxBuf, (u16)len))
    {
        printf("MQTT SUBSCRIBE send failed\r\n");
        return 0;
    }
    delay_ms(500);
    printf("SUBACK raw: ");
    {
        u16 i;
        for (i = 0; i < USART3_RX_CNT && i < 32; i++)
            printf("%02X ", USART3_RX_BUF[i]);
        printf("\r\n");
    }

#if !TC_SELF_HOSTED
    /* Also listen for the reply to attributes/get, then ask the cloud for
     * the stored thresholds so a reboot does not fall back to defaults.
     * The reply lands in the main loop and is parsed by ThingsCloud_Poll. */
    len = MQTT_BuildSubscribe(mqttTxBuf, "attributes/get/response/+", 0);
    if (!ESP8266_SendData(mqttTxBuf, (u16)len))
        printf("MQTT SUBSCRIBE get/response failed\r\n");
    delay_ms(300);
#endif

    USART3_ClearRx();       /* 清空 CONNACK/SUBACK，交给主循环 Poll 干净缓冲 */

#if !TC_SELF_HOSTED
    {
        char req[128];
        u8   i;
        int  n = sprintf(req, "{\"keys\":[");
        for (i = 0; i < TC_THRESHOLD_NUM; i++)
            n += sprintf(req + n, "\"%s\",", tcThresholds[i].key);
        strcpy(req + n - 1, "]}");                  /* overwrite last ',' */
        len = MQTT_BuildPublish(mqttTxBuf, "attributes/get/1", req, strlen(req));
        ESP8266_SendData(mqttTxBuf, (u16)len);      /* do not clear RX: reply may already be in it */
    }
#endif

    printf("--- ThingsCloud connected ---\r\n");
    return 1;
}

/* ------------------------------------------------------------------ */
u8 ThingsCloud_Publish(const char *payload)
{
    int len;
    u8  res;

    len = MQTT_BuildPublish(mqttTxBuf, TC_TOPIC_PUB, payload, strlen(payload));
    if (len <= 0 || len > MQTT_TX_BUF_SIZE)
        return 0;

    res = ESP8266_SendData(mqttTxBuf, (u16)len);
    USART3_ClearRx();       /* QoS0 无响应，丢掉 "SEND OK" 避免被 Poll 误解析 */
    return res;
}

/* ------------------------------------------------------------------ */
u8 ThingsCloud_Ping(void)
{
    int len = MQTT_BuildPingReq(mqttTxBuf);
    u8  res;

    res = ESP8266_SendData(mqttTxBuf, (u16)len);
    USART3_ClearRx();       /* 丢掉 "SEND OK" 和 PINGRESP */
    return res;
}

/* ------------------------------------------------------------------ */
u8 ThingsCloud_Poll(void)
{
    static u8 frame[256];
    u16 n;
    int v;

    if (!USART3_RX_OVER)
        return 0;

    n = USART3_GetFrame(frame, sizeof(frame) - 1);
    frame[n] = 0;

    /* The frame is a raw MQTT packet ("+IPD,n:" 0x30 len 0x00 topiclen topic
     * payload). The 0x00 topic-length byte would end the C string before the
     * JSON, so blank every NUL first; strstr then reaches the payload. */
    {
        u16 i;
        for (i = 0; i < n; i++)
            if (frame[i] == 0) frame[i] = ' ';
    }

    if (strchr((const char *)frame, '{') == NULL)
        return 0;                       /* PINGRESP / SEND OK etc: nothing to parse */

    printf("cloud<%s\r\n", frame);

    /* RGB lamp: a push may carry any subset, untouched channels keep last value */
    {
        static const struct { const char *key; u8 *var; } rgb[] = {
            { "R", &gTcCmd.r }, { "G", &gTcCmd.g }, { "B", &gTcCmd.b },
        };
        u8 i;
        for (i = 0; i < 3; i++)
        {
            v = JSON_FindBool((const char *)frame, rgb[i].key);
            if (v >= 0)
            {
                *rgb[i].var      = (u8)v;
                gTcCmd.rgbUpdate = 1;
            }
        }
    }

    /* thresholds: same parser serves attributes/push and the attributes/get reply */
    {
        u8 i;
        for (i = 0; i < TC_THRESHOLD_NUM; i++)
            if (JSON_FindU16((const char *)frame, tcThresholds[i].key, tcThresholds[i].var))
                printf("cloud %s=%d\r\n", tcThresholds[i].key, *tcThresholds[i].var);
    }

    return 1;
}
