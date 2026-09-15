#include "esp8266.h"
#include "usart3.h"
#include "delay.h"
#include "string.h"
#include "stdio.h"

/* Uncomment to echo every AT command and response to USART1 (debug) */
#define ESP8266_DEBUG

#ifdef ESP8266_DEBUG
#define WIFI_LOG(...)  printf(__VA_ARGS__)
#else
#define WIFI_LOG(...)
#endif

/* ------------------------------------------------------------------ */
void ESP8266_GPIOInit(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    WIFI_EN_Set();      /* module enabled */
    WIFI_RST_Set();     /* out of reset   */
}

/* ------------------------------------------------------------------ */
u8 ESP8266_SendCmd(const char *cmd, const char *ack1, const char *ack2, u16 timeoutMs)
{
    u16 i;
    u8  res = 0;

    USART3_ClearRx();

    WIFI_LOG("AT> %s\r\n", cmd);
    USART3_SendBuf((u8 *)cmd, strlen(cmd));
    USART3_SendBuf((u8 *)"\r\n", 2);

    if (ack1 == NULL && ack2 == NULL)
        return 1;

    for (i = 0; i < timeoutMs / 10; i++)
    {
        delay_ms(10);
        if (ack1 != NULL && strstr((const char *)USART3_RX_BUF, ack1) != NULL)
        {
            res = 1;
            break;
        }
        if (ack2 != NULL && strstr((const char *)USART3_RX_BUF, ack2) != NULL)
        {
            res = 1;
            break;
        }
    }

    if (!res)
    {
        WIFI_LOG("AT< timeout (%s)\r\n", cmd);
        WIFI_LOG("AT< resp: %s\r\n", USART3_RX_BUF);   /* keep +CWJAP:<err> for diagnosis */
    }

    USART3_ClearRx();
    return res;
}

/* ------------------------------------------------------------------ */
u8 ESP8266_TestAT(void)
{
    u8 i;

    for (i = 0; i < 5; i++)
    {
        if (ESP8266_SendCmd("AT", "OK", NULL, 500))
            return 1;

        WIFI_LOG("reset ESP8266...\r\n");
        WIFI_RST_Clr();
        delay_ms(200);
        WIFI_RST_Set();
        delay_ms(800);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
u8 ESP8266_SetMode(u8 mode)
{
    char cmd[24];

    sprintf(cmd, "AT+CWMODE=%d", mode);
    return ESP8266_SendCmd(cmd, "OK", "no change", 1000);
}

/* ------------------------------------------------------------------ */
u8 ESP8266_JoinAP(const char *ssid, const char *pwd)
{
    char cmd[96];

    /* Must wait until DHCP finishes ("WIFI GOT IP"), not just
     * "WIFI CONNECTED", otherwise the following CIPSTART has no IP yet.
     * Timeout 30s: phone hotspots can be slow to associate. */
    sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"", ssid, pwd);
    return ESP8266_SendCmd(cmd, "GOT IP", "OK", 30000);
}

/* ------------------------------------------------------------------ */
u8 ESP8266_ConnectTCP(const char *ip, u16 port)
{
    char cmd[80];

    sprintf(cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d", ip, port);
    return ESP8266_SendCmd(cmd, "OK", "ALREADY CONNECTED", 8000);
}

/* ------------------------------------------------------------------ */
u8 ESP8266_CloseTCP(void)
{
    return ESP8266_SendCmd("AT+CIPCLOSE", "OK", "CLOSED", 2000);
}

/* ------------------------------------------------------------------ */
u8 ESP8266_SendData(u8 *buf, u16 len)
{
    char cmd[32];
    u16  i;
    u8   res = 0;

    sprintf(cmd, "AT+CIPSEND=%d", len);
    if (!ESP8266_SendCmd(cmd, ">", NULL, 3000))
        return 0;

    USART3_ClearRx();
    USART3_SendBuf(buf, len);

    for (i = 0; i < 200; i++)           /* up to 2s waiting for SEND OK */
    {
        delay_ms(10);
        if (strstr((const char *)USART3_RX_BUF, "SEND OK") != NULL)
        {
            res = 1;
            break;
        }
    }

    if (!res)
        WIFI_LOG("CIPSEND failed, len=%d\r\n", len);

    /* 注意：这里不能 USART3_ClearRx()！
     * 本地 Broker 响应只有 1~2ms，CONNACK/SUBACK 常常和 "SEND OK"
     * 在同一批数据里到达，清掉后上层就永远等不到 CONNACK。
     * 改为由调用方读完响应后自行清空。 */
    return res;
}
