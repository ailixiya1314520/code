#ifndef __USART3_H
#define __USART3_H	 
#include "sys.h"
//////////////////////////////////////////////////////////////////////////////////
// USART3 driver (PB10=TX, PB11=RX), AT link to the ESP8266 WiFi module
//////////////////////////////////////////////////////////////////////////////////

#define USART3_RX_BUF_SIZE   256

extern u8          USART3_RX_BUF[USART3_RX_BUF_SIZE];
extern volatile u16 USART3_RX_CNT;
extern volatile u8  USART3_RX_OVER;

void usart3_init(u32 bound);            // init
void USART3_SendBuf(u8 *buf, u16 len);  // send
void USART3_ClearRx(void);              // clear rx buffer
u16  USART3_GetFrame(u8 *dst, u16 maxLen); // fetch a frame and clear

#endif
