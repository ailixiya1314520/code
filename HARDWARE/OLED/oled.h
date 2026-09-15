//////////////////////////////////////////////////////////////////////////////////	 
//������ֻ��ѧϰʹ�ã�δ���������ɣ��������������κ���;
//�о�԰����
//���̵�ַ��http://shop73023976.taobao.com/?spm=2013.1.0.0.M4PqC2
//
//  �� �� ��   : main.c
//  �� �� ��   : v2.0
//  ��    ��   : Evk123
//  ��������   : 2014-0101
//  ����޸�   : 
//  ��������   : 0.69��OLED �ӿ���ʾ����(STM32F103ZEϵ��IIC)
//              ˵��: 
//              ----------------------------------------------------------------
//              GND   ��Դ��
//              VCC   ��5V��3.3v��Դ
//              SCL   ��PB8��SCL��
//              SDA   ��PB9��SDA��            
//              ----------------------------------------------------------------
//Copyright(C) �о�԰����2014/3/16
//All rights reserved
//////////////////////////////////////////////////////////////////////////////////
#ifndef __OLED_H
#define __OLED_H			  	 
#include "sys.h"
#include "stdlib.h"	    	
#define OLED_MODE 0
#define SIZE 8
#define XLevelL		0x00
#define XLevelH		0x10
#define Max_Column	128
#define Max_Row		64
#define	Brightness	0xFF 
#define X_WIDTH 	128
#define Y_WIDTH 	64	    						  
//-----------------OLED IIC�˿ڶ���----------------  					   

#define OLED_SCLK_Clr() GPIO_ResetBits(GPIOB,GPIO_Pin_12)//SPI SCK -> D0
#define OLED_SCLK_Set() GPIO_SetBits(GPIOB,GPIO_Pin_12)

#define OLED_SDIN_Clr() GPIO_ResetBits(GPIOB,GPIO_Pin_13)//SPI MOSI -> D1
#define OLED_SDIN_Set() GPIO_SetBits(GPIOB,GPIO_Pin_13)

#define OLED_RST_Clr()  GPIO_ResetBits(GPIOA,GPIO_Pin_8) //RES -> PA8
#define OLED_RST_Set()  GPIO_SetBits(GPIOA,GPIO_Pin_8)

#define OLED_DC_Clr()   GPIO_ResetBits(GPIOA,GPIO_Pin_2) //DC  -> PA2
#define OLED_DC_Set()   GPIO_SetBits(GPIOA,GPIO_Pin_2)

#define OLED_CS_Clr()   GPIO_ResetBits(GPIOB,GPIO_Pin_15)//CS  -> PB15
#define OLED_CS_Set()   GPIO_SetBits(GPIOB,GPIO_Pin_15)

 		     
#define OLED_CMD  0	//д����
#define OLED_DATA 1	//

/* ---- OLED interface selection ----
 * 1 = 4-pin software I2C (GND/VCC/SCL/SDA): SCL->PB6, SDA->PB7
 * 0 = 7-pin 4-wire SPI (D0->PB12,D1->PB13,RES->PA8,DC->PA2,CS->PB15)
 * I2C addr 0x78 (=0x3C<<1); some modules use 0x7A (SA0=1). */
#define OLED_IF_I2C        1
#define OLED_I2C_ADDR      0x78
#define OLED_I2C_SCL_PIN   GPIO_Pin_6

/* I2C-only: probe an 8-bit address (0x78/0x7A), 1 = ACK from a device */
u8 OLED_I2C_Probe(u8 addr);
#define OLED_I2C_SDA_PIN   GPIO_Pin_7	//д����


//OLED�����ú���
void OLED_WR_Byte(unsigned dat,unsigned cmd);  
void OLED_Display_On(void);
void OLED_Display_Off(void);	   							   		    
void OLED_Init(void);
void OLED_Clear(void);
void OLED_DrawPoint(u8 x,u8 y,u8 t);
void OLED_Fill(u8 x1,u8 y1,u8 x2,u8 y2,u8 dot);
void OLED_ShowChar(u8 x,u8 y,u8 chr,u8 Char_Size);
void OLED_ShowNumber(u8 x,u8 y,u32 num,u8 len,u8 size);
void OLED_ShowString(u8 x,u8 y, char *p,u8 Char_Size);
void OLED_Set_Pos(unsigned char x, unsigned char y);
void OLED_ShowCHinese(u8 x,u8 y,u8 no);
void OLED_DrawBMP(unsigned char x0, unsigned char y0,unsigned char x1, unsigned char y1,unsigned char BMP[]);
void fill_picture(unsigned char fill_Data);
void Picture(void);

void OLED_fuhao_write(unsigned char x,unsigned char y,unsigned char asc);
void OLED_Num_write(unsigned char x,unsigned char y,unsigned char asc) ;
void OLED_Float(unsigned char Y,unsigned char X,double real,unsigned char N);
void OLED_Float2(unsigned char Y,unsigned char X,double real,unsigned char N1,unsigned char N2);
void OLED_Num2(unsigned char x,unsigned char y, int number);
void OLED_Num3(unsigned char x,unsigned char y,int number); 
void OLED_Num4(unsigned char x,unsigned char y, int number);
void OLED_Num5(unsigned char x,unsigned char y,unsigned int number);

	
#endif  
	 



