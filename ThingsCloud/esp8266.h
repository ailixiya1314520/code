#ifndef __ESP8266_H
#define __ESP8266_H
#include "sys.h"

/* ------------------------------------------------------------------
 * Hardware connection (change here if your board is different)
 *   STM32 PB10 (USART3_TX) -> ESP8266 RX
 *   STM32 PB11 (USART3_RX) <- ESP8266 TX
 *   STM32 PA4              -> ESP8266 EN  (CH_PD)
 *   STM32 PA5              -> ESP8266 RST
 * ------------------------------------------------------------------ */

#define WIFI_EN_Set()    GPIO_SetBits(GPIOA, GPIO_Pin_4)
#define WIFI_EN_Clr()    GPIO_ResetBits(GPIOA, GPIO_Pin_4)
#define WIFI_RST_Set()   GPIO_SetBits(GPIOA, GPIO_Pin_5)
#define WIFI_RST_Clr()   GPIO_ResetBits(GPIOA, GPIO_Pin_5)

void ESP8266_GPIOInit(void);

/* Send one AT command, wait for ack1 or ack2 (either can be NULL).
 * Returns 1 on success, 0 on timeout. */
u8   ESP8266_SendCmd(const char *cmd, const char *ack1, const char *ack2, u16 timeoutMs);

u8   ESP8266_TestAT(void);
u8   ESP8266_SetMode(u8 mode);                 /* 1=STA 2=AP 3=STA+AP */
u8   ESP8266_JoinAP(const char *ssid, const char *pwd);
u8   ESP8266_ConnectTCP(const char *ip, u16 port);
u8   ESP8266_CloseTCP(void);

/* Send raw binary buffer through the TCP link (AT+CIPSEND=len -> wait '>' -> send) */
u8   ESP8266_SendData(u8 *buf, u16 len);

#endif
