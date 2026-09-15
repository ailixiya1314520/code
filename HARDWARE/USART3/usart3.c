#include "usart3.h"
#include "sys.h"
#include "string.h"

//////////////////////////////////////////////////////////////////////////////////
// USART3 driver (PB10=TX, PB11=RX)
// Used as the AT command link to the ESP8266 WiFi module.
// Reception is line/frame based: IDLE interrupt marks the end of a frame.
//////////////////////////////////////////////////////////////////////////////////

u8          USART3_RX_BUF[USART3_RX_BUF_SIZE];
volatile u16 USART3_RX_CNT  = 0;
volatile u8  USART3_RX_OVER = 0;

/* Clear the receive buffer */
void USART3_ClearRx(void)
{
    USART3_RX_CNT  = 0;
    USART3_RX_OVER = 0;
    memset(USART3_RX_BUF, 0, USART3_RX_BUF_SIZE);
}

/* Copy out the received frame and clear the buffer, returns copied length */
u16 USART3_GetFrame(u8 *dst, u16 maxLen)
{
    u16 len = USART3_RX_CNT;

    if (len > maxLen)
        len = maxLen;

    memcpy(dst, (const void *)USART3_RX_BUF, len);
    USART3_ClearRx();

    return len;
}

/* Send len bytes */
void USART3_SendBuf(u8 *buf, u16 len)
{
    u16 i;

    for (i = 0; i < len; i++)
    {
        USART_SendData(USART3, (u16)buf[i]);
        while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
    }
}

/* USART3 interrupt: RXNE + IDLE */
void USART3_IRQHandler(void)
{
    u8 res;

    if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
    {
        res = (u8)USART_ReceiveData(USART3);
        if (USART3_RX_CNT < USART3_RX_BUF_SIZE)
            USART3_RX_BUF[USART3_RX_CNT++] = res;
    }

    if (USART_GetITStatus(USART3, USART_IT_IDLE) != RESET)
    {
        res = (u8)USART_ReceiveData(USART3);   /* read DR to clear IDLE flag */
        (void)res;
        if (USART3_RX_CNT > 0)
            USART3_RX_OVER = 1;
    }
}

/* USART3 init, bound = baudrate (115200 for ESP8266 AT firmware) */
void usart3_init(u32 bound)
{
    NVIC_InitTypeDef   NVIC_InitStructure;
    GPIO_InitTypeDef   GPIO_InitStructure;
    USART_InitTypeDef  USART_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

    USART_DeInit(USART3);

    /* USART3_TX  PB10 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* USART3_RX  PB11 */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate            = bound;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl  = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART3, &USART_InitStructure);

    USART3_ClearRx();

    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
    USART_ITConfig(USART3, USART_IT_IDLE, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel                   = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority  = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority         = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd                 = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART3, ENABLE);
}
