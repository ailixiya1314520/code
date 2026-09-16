#include "sys.h"
#include "usart.h"
#include "led.h"
#include "delay.h"
#include "ADC.h"
#include "usart3.h"
#include "key.h"
#include "oled.h"
#include "math.h"
#include "dht11.h"
#include "bmp280.h"
#include "thingscloud.h"
#include "esp8266.h"
#include "stdio.h"
#include "string.h"

/* ------------------------------------------------------------------
 * Smart curtain / environment monitor
 * Cloud platform: ThingsCloud (MQTT over ESP8266 AT firmware)
 *
 * Hardware:
 *   USART1 (PA9/PA10) 115200 -> debug log
 *   USART3 (PB10/PB11) 115200 -> ESP8266
 *   PA4 -> ESP8266 EN, PA5 -> ESP8266 RST
 * ------------------------------------------------------------------ */

u32 STM32_xx0 = 0X4E4C4A;
u32 STM32_xx1 = 0X364B1322;
u32 STM32_xx2 = 0X132D13;

#define FLASH_SAVE_ADDR 0x08010000

/* publish every 60 * 100ms = 6s, ping every 600 * 100ms = 60s */
#define PUB_INTERVAL    60
#define PING_INTERVAL   600

/* ==================================================================
 * OLED 独立测试开关(排查花屏用)
 *   1 = 只初始化串口+OLED,跑全屏/棋盘/字符测试,不联网、不初始化
 *       ESP8266 / ADC / 传感器 / 按键
 *   0 = 正常业务程序
 * 屏幕排查完成后改回 0 重新烧录即可,业务代码不受影响
 * ================================================================== */
#define OLED_TEST_ONLY  0

u8  buff[30];
u8  count;
u8  mode = 0;               /* 0 = auto, 1 = manual (cloud/app) */
u8  display_contrl = 0;
u8  curtain_flag = 0;       /* 0 = closed, 1 = open */
u8  last_curtain_flag = 0;
u16 cloudPubCnt = 0;
u16 cloudPingCnt = 0;
u8  cloudFailCnt = 0;
char pubMsg[160];

extern u8 DHT11_Temp, DHT11_Hum;   /* temperature / humidity */
u16 Pre;                           /* pressure  (hPa)  */
u16 gz_value;                      /* light            */
u16 m2_value;                      /* MQ2              */
u16 m7_value;                      /* MQ7              */
u16 m135_value;                     /* MQ135            */

u16 A_DHT11_Temp = 35;      /* thresholds */
u16 A_DHT11_Hum  = 20;
u16 A_pre        = 1500;
u16 A_gz_value   = 1000;
u16 A_m2_value   = 3500;
u16 A_m7_value   = 3500;
u16 A_m135_value = 500;

void Get_Data(u16 count);
void Cloud_KeyTask(u8 key);
void Canshu_Change(u8 key);
void BUJING_Cotrol(u8 mode, u16 time, u16 count);
void Cloud_PublishTask(void);
void Cloud_DownlinkTask(void);

/**********************************************************************
 * Cloud init: ESP8266 -> WiFi -> TCP -> MQTT (ThingsCloud)
 **********************************************************************/
void Cloud_Init(void)
{
    usart3_init(115200);                 /* AT link to ESP8266 */
    delay_ms(500);

    memset(&gTcCmd, 0, sizeof(gTcCmd));

    OLED_Clear();
    OLED_ShowString(0, 0, "cloud init...", 16);

    while (!ThingsCloud_Init())
    {
        printf("cloud connect failed, retry in 3s\r\n");
        OLED_ShowString(2, 0, "retry...", 16);
        delay_ms(1000);
        delay_ms(1000);
        delay_ms(1000);
    }

    OLED_ShowString(2, 0, "cloud ok!     ", 16);
    delay_ms(500);
}

/**********************************************************************
 * Build the JSON payload (temperature + humidity) and publish it
 **********************************************************************/
void Cloud_PublishTask(void)
{
    /* All sensors in the same JSON style as Temp/Humi.
     * Keys must match the attribute identifiers created in the
     * ThingsCloud console: Temp Humi GZ_Value Pre
     * MQ2_Value MQ7_Value MQ135_Value */
    sprintf(pubMsg,
            "{\"" TC_ATTR_TEMP "\":%d,\"" TC_ATTR_HUMI "\":%d,"
            "\"GZ_Value\":%d,\"Pre\":%d,"
            "\"MQ2_Value\":%d,\"MQ7_Value\":%d,\"MQ135_Value\":%d}",
            DHT11_Temp, DHT11_Hum, gz_value, Pre,
            m2_value, m7_value, m135_value);

    if (ThingsCloud_Publish(pubMsg))
    {
        cloudFailCnt = 0;
        printf("pub ok: %s\r\n", pubMsg);
    }
    else
    {
        cloudFailCnt++;
        printf("pub fail (%d)\r\n", cloudFailCnt);
    }
}

/**********************************************************************
 * Handle downlink commands from ThingsCloud
 **********************************************************************/
void Cloud_DownlinkTask(void)
{
    if (!ThingsCloud_Poll())
        return;

    if (gTcCmd.ledUpdate)
    {
        gTcCmd.ledUpdate = 0;
        mode = 1;                                   /* switch to manual */
        LED0 = gTcCmd.led ? 0 : 1;                  /* LED0 low = on    */
        printf("cloud LED = %d\r\n", gTcCmd.led);
    }

    if (gTcCmd.curtainUpdate)
    {
        gTcCmd.curtainUpdate = 0;
        mode = 1;                                   /* switch to manual */
        curtain_flag       = gTcCmd.curtain;
        last_curtain_flag  = gTcCmd.curtain;
        BUJING_Cotrol(gTcCmd.curtain, 3, 270);      /* drive the stepper */
        printf("cloud Curtain = %d\r\n", gTcCmd.curtain);
    }
}

#if OLED_TEST_ONLY
/**********************************************************************
 * OLED 独立测试程序(不联网、不初始化任何传感器)
 *
 * 现象对照:
 *   阶段0 全屏都点不亮,仍是随机散点  -> SPI 物理链路没通
 *        (查 D0->PB12 / D1->PB13 / CS->PB15 / DC->PA2 / RES->PA8
 *         接线,查模块背面电阻是否选在 SPI 档,换杜邦线)
 *   全屏能亮,但阶段2 字符花/缺笔画    -> DC 或 CS 接触不良
 *   字符清楚但整体右移约2像素/底部亮线 -> 控制器是 SH1106,非 SSD1306
 *   四个阶段全部正常                   -> 屏幕与驱动正常,再查联网
 **********************************************************************/
int main(void)
{
    u8  stage = 0;
    u16 loop  = 0;
    char tb[24];

    uart_init(115200);
    delay_init();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

#if OLED_IF_I2C
    printf("\r\n=== OLED standalone test (4-pin I2C, no network) ===\r\n");
#else
    printf("\r\n=== OLED standalone test (7-pin SPI, no network) ===\r\n");
#endif

    OLED_Init();
    OLED_Clear();

#if OLED_IF_I2C
    /* 硬件判定:直接探测 0x78 / 0x7A 是否应答,与显示内容无关 */
    {
        u8 a78 = OLED_I2C_Probe(0x78);
        u8 a7a = OLED_I2C_Probe(0x7A);
        printf("I2C probe -> 0x78:%s  0x7A:%s\r\n",
               a78 ? "ACK" : "no", a7a ? "ACK" : "no");
        if (!a78 && !a7a)
        {
            printf("NO DEVICE ON I2C BUS -> check GND/VCC/SCL->PB6/SDA->PB7, "
                   "pull-ups, and module I2C mode\r\n");
            OLED_ShowString(0, 0, "NO I2C DEVICE", 16);
        }
    }
#endif

    while (1)
    {
        switch (stage)
        {
        case 0:     /* 阶段0:全屏亮 -> 灭 -> 亮,验证所有像素/供电/复位 */
            printf("stage 0: full screen ON\r\n");
            fill_picture(0xFF);
            delay_ms(1500);
            fill_picture(0x00);
            delay_ms(600);
            fill_picture(0xFF);
            delay_ms(1500);
            OLED_Clear();
            break;

        case 1:     /* 阶段1:棋盘格,验证页/列寻址方向 */
            printf("stage 1: checkerboard\r\n");
            fill_picture(0xAA);
            delay_ms(1500);
            OLED_Clear();
            break;

        case 2:     /* 阶段2:4 行 16px ASCII,每行正好写满 128 列 */
            printf("stage 2: ASCII 16px\r\n");
            OLED_ShowString(0, 0, "SSD1306 SPI TEST", 16);
            OLED_ShowString(0, 2, "0123456789ABCDEF", 16);
            OLED_ShowString(0, 4, "abcdefghijklmnop", 16);
            OLED_ShowString(0, 6, "LINE4  DISPLAY OK", 16);
            delay_ms(3000);
            OLED_Clear();
            break;

        case 3:     /* 阶段3:12px 小字 + 中文字库 */
            printf("stage 3: small font + chinese\r\n");
            OLED_ShowString(0, 0, "12px font line 1", 12);
            OLED_ShowString(0, 1, "12px font line 2", 12);
            OLED_ShowCHinese(0, 3, 0);
            OLED_ShowCHinese(16, 3, 2);
            OLED_ShowString(32, 3, ": CN FONT OK", 16);
            OLED_ShowString(0, 6, "test running...", 12);
            delay_ms(3000);
            OLED_Clear();
            break;
        }

        stage++;
        if (stage >= 4)
        {
            stage = 0;
            loop++;
            sprintf(tb, "loop:%d", loop);
            OLED_ShowString(0, 7, tb, 12);
            printf("test loop = %d\r\n", loop);
        }
    }
}
#else
/**********************************************************************
 * main
 **********************************************************************/
int main(void)
{
    u8  t = 0;
    int key_value;

    uart_init(115200);                              /* debug log */
    delay_init();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    Adc1_Channe_Init();
    KEY_Init();
    bmp280Init();
    LED_Init();
    OLED_Init();
    OLED_Clear();

    /* DHT11 presence check at boot (data pin PA11) */
    if (DHT11_Init())
        printf("DHT11 not detected! check DATA->PA11, VCC, GND\r\n");
    else
        printf("DHT11 ok\r\n");

    Cloud_Init();

    while (1)
    {
        Get_Data(0);

        /* ---- auto mode: open the curtain when it is bright ---- */
        if (gz_value <= A_gz_value && mode == 0)
        {
            LED0 = 0;
            curtain_flag = 0;
        }
        else if (gz_value > A_gz_value && mode == 0)
        {
            LED0 = 1;
            curtain_flag = 1;
        }

        if (last_curtain_flag != curtain_flag && mode == 0)
        {
            BUJING_Cotrol(curtain_flag, 3, 270);
            last_curtain_flag = curtain_flag;
        }

        /* ---- alarm ---- */
        if (DHT11_Temp >= A_DHT11_Temp || DHT11_Hum <= A_DHT11_Hum ||
            Pre >= A_pre || m135_value <= A_m135_value ||
            m2_value >= A_m2_value || m7_value >= A_m7_value)
        {
            BEEP = !BEEP;
        }
        else
        {
            BEEP = 0;
        }

        key_value = KEY_Scan(0);
        if (key_value == 2)
            display_contrl++;

        /* ---- OLED pages ---- */
        if (display_contrl % 2 == 0 && t >= 10)
        {
            OLED_ShowCHinese(0, 0, 0);
            OLED_ShowCHinese(16, 0, 2);
            sprintf((char*)buff, ":%2dC", DHT11_Temp);
            OLED_ShowString(32, 0, (char*)buff, 16);

            OLED_ShowCHinese(64, 0, 1);
            OLED_ShowCHinese(82, 0, 2);
            sprintf((char*)buff, ":%2d%%", DHT11_Hum);
            OLED_ShowString(96, 0, (char*)buff, 16);

            OLED_ShowCHinese(0, 2, 3);
            OLED_ShowCHinese(16, 2, 4);
            sprintf((char*)buff, ":%2dhpa     ", Pre);
            OLED_ShowString(32, 2, (char*)buff, 16);

            OLED_ShowCHinese(0, 4, 5);
            OLED_ShowCHinese(16, 4, 6);
            OLED_ShowCHinese(32, 4, 7);
            OLED_ShowCHinese(48, 4, 8);
            sprintf((char*)buff, ":%4dppm", gz_value);
            OLED_ShowString(64, 4, (char*)buff, 16);

            OLED_ShowCHinese(0, 6, 9);
            OLED_ShowCHinese(16, 6, 10);
            OLED_ShowCHinese(32, 6, 11);
            OLED_ShowCHinese(48, 6, 12);
            sprintf((char*)buff, ":%4dppm", m135_value);
            OLED_ShowString(64, 6, (char*)buff, 16);
        }
        else if (display_contrl % 2 == 1 && t >= 10)
        {
            OLED_ShowCHinese(0, 0, 13);
            OLED_ShowCHinese(16, 0, 14);
            OLED_ShowCHinese(32, 0, 15);
            OLED_ShowCHinese(48, 0, 16);
            sprintf((char*)buff, ":%4dppm", m2_value);
            OLED_ShowString(64, 0, (char*)buff, 16);

            OLED_ShowCHinese(0, 2, 17);
            OLED_ShowCHinese(16, 2, 18);
            OLED_ShowCHinese(32, 2, 19);
            OLED_ShowCHinese(48, 2, 20);
            sprintf((char*)buff, ":%4dppm", m7_value);
            OLED_ShowString(64, 2, (char*)buff, 16);

            OLED_ShowCHinese(0, 4, 5);
            OLED_ShowCHinese(16, 4, 6);
            OLED_ShowCHinese(32, 4, 7);
            OLED_ShowCHinese(48, 4, 8);
            sprintf((char*)buff, ":%4dppm", gz_value);
            OLED_ShowString(64, 4, (char*)buff, 16);

            OLED_ShowCHinese(0, 6, 9);
            OLED_ShowCHinese(16, 6, 10);
            OLED_ShowCHinese(32, 6, 11);
            OLED_ShowCHinese(48, 6, 12);
            sprintf((char*)buff, ":%4dppm", m135_value);
            OLED_ShowString(64, 6, (char*)buff, 16);
        }

        /* ---- cloud ---- */
        Cloud_DownlinkTask();
        Cloud_KeyTask(key_value);
        Canshu_Change(key_value);

        if (++cloudPubCnt >= PUB_INTERVAL)
        {
            cloudPubCnt = 0;
            Cloud_PublishTask();
        }

        if (++cloudPingCnt >= PING_INTERVAL)
        {
            cloudPingCnt = 0;
            ThingsCloud_Ping();
        }

        /* link lost -> bring the connection back up */
        if (cloudFailCnt >= 3)
        {
            cloudFailCnt = 0;
            printf("reconnect to ThingsCloud...\r\n");
            ESP8266_CloseTCP();
            delay_ms(500);
            Cloud_Init();
        }

        t++;
        delay_ms(100);
    }
}
#endif /* OLED_TEST_ONLY */

/**********************************************************************
 * Keys:
 *   KEY1 (3) -> publish once immediately
 *   KEY2 (4) -> force reconnect to the cloud
 **********************************************************************/
void Cloud_KeyTask(u8 key)
{
    if (key == 3)
    {
        cloudPubCnt = 0;
        Cloud_PublishTask();
    }

    if (key == 4)
    {
        printf("force reconnect\r\n");
        ESP8266_CloseTCP();
        delay_ms(500);
        Cloud_Init();
    }
}

/**********************************************************************
 * Threshold / mode setting menu (KEY1 enters, KEY2 exits)
 **********************************************************************/
void Canshu_Change(u8 key)
{
    u8 obj = 7;

    if (key != 1)
        return;

    BEEP = 0;
    OLED_Clear();

    while (1)
    {
        key = KEY_Scan(0);

        if (key == 1)
        {
            obj++;
            if (obj >= 8)
                obj = 0;
        }

        sprintf((char *)buff, "Working md:%4d", mode);
        OLED_ShowString(8, 0, (char*)buff, 12);
        sprintf((char *)buff, "A_Temp    :%4d", A_DHT11_Temp);
        OLED_ShowString(8, 1, (char*)buff, 12);
        sprintf((char *)buff, "A_Hum     :%4d", A_DHT11_Hum);
        OLED_ShowString(8, 2, (char*)buff, 12);
        sprintf((char *)buff, "A_pre     :%4d", A_pre);
        OLED_ShowString(8, 3, (char*)buff, 12);
        sprintf((char *)buff, "A_gz_val  :%4d", A_gz_value);
        OLED_ShowString(8, 4, (char*)buff, 12);
        sprintf((char *)buff, "A_m2_val  :%4d", A_m2_value);
        OLED_ShowString(8, 5, (char*)buff, 12);
        sprintf((char *)buff, "A_m7_val  :%4d", A_m7_value);
        OLED_ShowString(8, 6, (char*)buff, 12);
        sprintf((char *)buff, "A_m135_val:%4d", A_m135_value);
        OLED_ShowString(8, 7, (char*)buff, 12);

        /* cursor */
        OLED_ShowString(0, 0, " ", 12);
        OLED_ShowString(0, 1, " ", 12);
        OLED_ShowString(0, 2, " ", 12);
        OLED_ShowString(0, 3, " ", 12);
        OLED_ShowString(0, 4, " ", 12);
        OLED_ShowString(0, 5, " ", 12);
        OLED_ShowString(0, 6, " ", 12);
        OLED_ShowString(0, 7, " ", 12);
        OLED_ShowString(0, (obj == 7) ? 0 : (obj + 1), ">", 12);

        if (obj == 0)
        {
            if (key == 3) A_DHT11_Temp += 1;
            if (key == 4) A_DHT11_Temp -= 1;
        }
        if (obj == 1)
        {
            if (key == 3) A_DHT11_Hum += 1;
            if (key == 4) A_DHT11_Hum -= 1;
        }
        if (obj == 2)
        {
            if (key == 3) A_pre += 20;
            if (key == 4) A_pre -= 20;
        }
        if (obj == 3)
        {
            if (key == 3) A_gz_value += 50;
            if (key == 4) A_gz_value -= 50;
        }
        if (obj == 4)
        {
            if (key == 3) A_m2_value += 50;
            if (key == 4) A_m2_value -= 50;
        }
        if (obj == 5)
        {
            if (key == 3) A_m7_value += 50;
            if (key == 4) A_m7_value -= 50;
        }
        if (obj == 6)
        {
            if (key == 3) A_m135_value += 50;
            if (key == 4) A_m135_value -= 50;
        }
        if (obj == 7)
        {
            if (key == 3) mode += 1;
            if (key == 4) mode -= 1;
            if (mode >= 2) mode = 0;
        }

        /* KEY2 -> save & exit */
        if (key == 2)
        {
            OLED_Clear();
            break;
        }
    }
}

/**********************************************************************
 * Read all sensors
 **********************************************************************/
void Get_Data(u16 count)
{
    static float bmp280_press, bmp280;
    static u8 dht11Tick = 0;

    /* DHT11 needs >= 1s between two reads */
    if (++dht11Tick >= 10)
    {
        dht11Tick = 0;
        if (DHT11_Read_Data(&DHT11_Temp, &DHT11_Hum))
        {
            /* no answer -> Temp/Humi keep old value; warn once */
            static u8 dht11Warned = 0;
            if (!dht11Warned)
            {
                dht11Warned = 1;
                printf("DHT11 read failed, Temp/Humi frozen (check PA11 wiring)\r\n");
            }
        }
    }

    bmp280GetData(&bmp280_press, &bmp280, &bmp280);
    Pre = bmp280_press;

    gz_value   = 4096 - get_Adc_Value(0x04);
    m2_value   = get_Adc_Value(0x07);
    m7_value   = get_Adc_Value(0x05);
    m135_value = 4096 - get_Adc_Value(0x06);
}

/**********************************************************************
 * Stepper motor: mode 0 = one direction, 1 = the other
 **********************************************************************/
void BUJING_Cotrol(u8 mode, u16 time, u16 count)
{
    if (mode == 0)
    {
        while (count--)
        {
            BUJ1 = 1; BUJ2 = 0; BUJ3 = 0; BUJ4 = 0;
            delay_ms(time);
            BUJ1 = 0; BUJ2 = 1; BUJ3 = 0; BUJ4 = 0;
            delay_ms(time);
            BUJ1 = 0; BUJ2 = 0; BUJ3 = 1; BUJ4 = 0;
            delay_ms(time);
            BUJ1 = 0; BUJ2 = 0; BUJ3 = 0; BUJ4 = 1;
            delay_ms(time);
        }
    }
    if (mode == 1)
    {
        while (count--)
        {
            BUJ1 = 0; BUJ2 = 0; BUJ3 = 0; BUJ4 = 1;
            delay_ms(time);
            BUJ1 = 0; BUJ2 = 0; BUJ3 = 1; BUJ4 = 0;
            delay_ms(time);
            BUJ1 = 0; BUJ2 = 1; BUJ3 = 0; BUJ4 = 0;
            delay_ms(time);
            BUJ1 = 1; BUJ2 = 0; BUJ3 = 0; BUJ4 = 0;
            delay_ms(time);
        }
    }
}
