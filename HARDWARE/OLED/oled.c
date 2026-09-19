//////////////////////////////////////////////////////////////////////////////////	 
//������ֻ��ѧϰʹ�ã�δ���������ɣ��������������κ���;
//�о�԰����
//���̵�ַ��http://shop73023976.taobao.com/?spm=2013.1.0.0.M4PqC2
//
//  �� �� ��   : main.c
//  �� �� ��   : v2.0
//  ��    ��   : Evk123
//  ��������   : 2014-0101
//  ����޸�?  : 
//  ��������   : 0.69��OLED �ӿ���ʾ����(STM32F103ZEϵ��IIC)
//              ˵��: 
//              ----------------------------------------------------------------
//              GND   ��Դ��
//              VCC   ��5V��3.3v��Դ
//              D0/SCK  -> PB12 (4-wire SPI clock)
//              D1/MOSI -> PB13 (4-wire SPI data)
//              RES     -> PA8
//              DC      -> PA2
//              CS      -> PB15
//              ----------------------------------------------------------------
//Copyright(C) �о�԰����2014/3/16
//All rights reserved
//////////////////////////////////////////////////////////////////////////////////

#include "oled.h"
#include "stdlib.h"
#include "oledfont.h"
//OLED���Դ�
//��Ÿ�ʽ����?
//[0]0 1 2 3 ... 127	
//[1]0 1 2 3 ... 127	
//[2]0 1 2 3 ... 127	
//[3]0 1 2 3 ... 127	
//[4]0 1 2 3 ... 127	
//[5]0 1 2 3 ... 127	
//[6]0 1 2 3 ... 127	
//[7]0 1 2 3 ... 127 			   
/**********************************************
//IIC Start
**********************************************/
/**********************************************
//IIC Start
**********************************************/
#if !OLED_IF_I2C
/**********************************************
// 4-wire SPI: send one byte, MSB first
// CPOL=0, CPHA=0: data latched on SCK rising edge
**********************************************/
static void OLED_SPI_Dly(volatile u16 n)
{
	while(n--) { __NOP(); }
}

static void OLED_SPI_WriteByte(u8 dat)
{
	u8 i;
	for(i=0;i<8;i++)
	{
		OLED_SCLK_Clr();
		if(dat&0x80) OLED_SDIN_Set();   /* MSB first */
		else         OLED_SDIN_Clr();
		OLED_SPI_Dly(60);               /* data setup ~3us */
		OLED_SCLK_Set();                /* sample on rising edge */
		OLED_SPI_Dly(60);               /* high/hold ~3us -> SCK ~160kHz */
		dat <<= 1;
	}
	OLED_SCLK_Clr();
	OLED_SPI_Dly(20);
}

void OLED_WR_Byte(unsigned dat,unsigned cmd)
{
	if(cmd) OLED_DC_Set();   //DC=1: data  (stable before CS low)
	else    OLED_DC_Clr();   //DC=0: command
	OLED_CS_Clr();
	OLED_SPI_WriteByte(dat);
	OLED_CS_Set();
}
#else
/**********************************************
// 4-pin software I2C for SSD1306 (SCL->PB6, SDA->PB7)
// Pins are open-drain, rely on the pull-up resistors
// mounted on the OLED module (~4.7k).
**********************************************/
#define IIC_SCL_Set()   GPIO_SetBits(GPIOB, OLED_I2C_SCL_PIN)
#define IIC_SCL_Clr()   GPIO_ResetBits(GPIOB, OLED_I2C_SCL_PIN)
#define IIC_SDA_Set()   GPIO_SetBits(GPIOB, OLED_I2C_SDA_PIN)
#define IIC_SDA_Clr()   GPIO_ResetBits(GPIOB, OLED_I2C_SDA_PIN)
#define IIC_SDA_Read()  GPIO_ReadInputDataBit(GPIOB, OLED_I2C_SDA_PIN)

static void IIC_Dly(void)
{
	volatile u8 t = 6;
	while (t--) { __NOP(); }
}

static void IIC_Start(void)
{
	IIC_SDA_Set();
	IIC_SCL_Set();
	IIC_Dly();
	IIC_SDA_Clr();              /* SDA falls while SCL high */
	IIC_Dly();
	IIC_SCL_Clr();
}

static void IIC_Stop(void)
{
	IIC_SDA_Clr();
	IIC_SCL_Set();
	IIC_Dly();
	IIC_SDA_Set();              /* SDA rises while SCL high */
	IIC_Dly();
}

/* Returns 1 if ACK seen (SDA pulled low by slave at clock 9) */
static u8 IIC_SendByte(u8 dat)
{
	u8 i, ack;

	for (i = 0; i < 8; i++)
	{
		if (dat & 0x80) IIC_SDA_Set();
		else            IIC_SDA_Clr();
		dat <<= 1;
		IIC_Dly();
		IIC_SCL_Set();
		IIC_Dly();
		IIC_SCL_Clr();
		IIC_Dly();
	}

	IIC_SDA_Set();              /* release SDA, slave drives ACK */
	IIC_Dly();
	IIC_SCL_Set();
	IIC_Dly();
	ack = (IIC_SDA_Read() == Bit_RESET) ? 1 : 0;
	IIC_SCL_Clr();
	IIC_Dly();
	return ack;
}

static void IIC_WriteCmd(u8 cmd)
{
	IIC_Start();
	IIC_SendByte(OLED_I2C_ADDR);    /* slave address + W */
	IIC_SendByte(0x00);             /* control byte: command */
	IIC_SendByte(cmd);
	IIC_Stop();
}

static void IIC_WriteData(u8 dat)
{
	IIC_Start();
	IIC_SendByte(OLED_I2C_ADDR);    /* slave address + W */
	IIC_SendByte(0x40);             /* control byte: data */
	IIC_SendByte(dat);
	IIC_Stop();
}

void OLED_WR_Byte(unsigned dat,unsigned cmd)
{
	if (cmd) IIC_WriteData((u8)dat);
	else     IIC_WriteCmd((u8)dat);
}

/* Probe an 8-bit I2C address. Returns 1 if the device ACKs.
 * Used to decide hardware/wiring problems independent of the display. */
u8 OLED_I2C_Probe(u8 addr)
{
	u8 ack;
	IIC_Start();
	ack = IIC_SendByte(addr);
	IIC_Stop();
	IIC_Dly(); IIC_Dly();
	return ack;
}
#endif


/********************************************
// fill_Picture
********************************************/
void fill_picture(unsigned char fill_Data)
{
	unsigned char m,n;
	for(m=0;m<8;m++)
	{
		OLED_WR_Byte(0xb0+m,0);		//page0-page1
		OLED_WR_Byte(0x00,0);		//low column start address
		OLED_WR_Byte(0x10,0);		//high column start address
		for(n=0;n<128;n++)
			{
				OLED_WR_Byte(fill_Data,1);
			}
	}
}


/***********************Delay****************************************/
void Delay_50ms(unsigned int Del_50ms)
{
	unsigned int m;
	for(;Del_50ms>0;Del_50ms--)
		for(m=6245;m>0;m--);
}

void Delay_1ms(unsigned int Del_1ms)
{
	unsigned char j;
	while(Del_1ms--)
	{	
		for(j=0;j<123;j++);
	}
}

//��������

	void OLED_Set_Pos(unsigned char x, unsigned char y) 
{ 	OLED_WR_Byte(0xb0+y,OLED_CMD);
	OLED_WR_Byte(((x&0xf0)>>4)|0x10,OLED_CMD);
	OLED_WR_Byte((x&0x0f),OLED_CMD); 
}   	  
//����OLED��ʾ    
void OLED_Display_On(void)
{
	OLED_WR_Byte(0X8D,OLED_CMD);  //SET DCDC����
	OLED_WR_Byte(0X14,OLED_CMD);  //DCDC ON
	OLED_WR_Byte(0XAF,OLED_CMD);  //DISPLAY ON
}
//�ر�OLED��ʾ     
void OLED_Display_Off(void)
{
	OLED_WR_Byte(0X8D,OLED_CMD);  //SET DCDC����
	OLED_WR_Byte(0X10,OLED_CMD);  //DCDC OFF
	OLED_WR_Byte(0XAE,OLED_CMD);  //DISPLAY OFF
}		   			 
//��������,������,������Ļ�Ǻ�ɫ��!��û����һ��!!!	  
void OLED_Clear(void)  
{  
	u8 i,n;		    
	for(i=0;i<8;i++)  
	{  
		OLED_WR_Byte (0xb0+i,OLED_CMD);    //����ҳ��ַ��0~7��
		OLED_WR_Byte (0x00,OLED_CMD);      //������ʾλ�á��е͵�ַ
		OLED_WR_Byte (0x10,OLED_CMD);      //������ʾλ�á��иߵ�ַ   
		for(n=0;n<128;n++)OLED_WR_Byte(0,OLED_DATA); 
	} //������ʾ
}
void OLED_On(void)  
{  
	u8 i,n;		    
	for(i=0;i<8;i++)  
	{  
		OLED_WR_Byte (0xb0+i,OLED_CMD);    //����ҳ��ַ��0~7��
		OLED_WR_Byte (0x00,OLED_CMD);      //������ʾλ�á��е͵�ַ
		OLED_WR_Byte (0x10,OLED_CMD);      //������ʾλ�á��иߵ�ַ   
		for(n=0;n<128;n++)OLED_WR_Byte(1,OLED_DATA); 
	} //������ʾ
}
//��ָ��λ����ʾһ���ַ�,���������ַ�
//x:0~127
//y:0~63
//mode:0,������ʾ;1,������ʾ				 
//size:ѡ������ 16/12 
void OLED_ShowChar(u8 x,u8 y,u8 chr,u8 Char_Size)
{      	
	unsigned char c=0,i=0;	
		c=chr-' ';//�õ�ƫ�ƺ���?		
		if(x>Max_Column-1){x=0;y=y+2;}
		if(Char_Size ==16)
			{
			OLED_Set_Pos(x,y);	
			for(i=0;i<8;i++)
			OLED_WR_Byte(F8X16[c*16+i],OLED_DATA);
			OLED_Set_Pos(x,y+1);
			for(i=0;i<8;i++)
			OLED_WR_Byte(F8X16[c*16+i+8],OLED_DATA);
			}
			else {	
				OLED_Set_Pos(x,y);
				for(i=0;i<6;i++)
				OLED_WR_Byte(F6x8[c][i],OLED_DATA);
				
			}
}
//m^n����
u32 oled_pow(u8 m,u8 n)
{
	u32 result=1;	 
	while(n--)result*=m;    
	return result;
}				  
//��ʾ2������
//x,y :�������? 
//len :���ֵ�λ��
//size:������?
//mode:ģʽ	0,���ģ�?1,����ģʽ
//num:��ֵ(0~4294967295);	 		  
void OLED_ShowNumber(u8 x,u8 y,u32 num,u8 len,u8 size2)
{         	
	u8 t,temp;
	u8 enshow=0;						   
	for(t=0;t<len;t++)
	{
		temp=(num/oled_pow(10,len-t-1))%10;
		if(enshow==0&&t<(len-1))
		{
			if(temp==0)
			{
				OLED_ShowChar(x+(size2/2)*t,y,' ',size2);
				continue;
			}else enshow=1; 
		 	 
		}
	 	OLED_ShowChar(x+(size2/2)*t,y,temp+'0',size2); 
	}
} 
//��ʾһ���ַ��Ŵ�
void OLED_ShowString(u8 x,u8 y, char *p,u8 Char_Size)
{
	while (*p!='\0')
	{		OLED_ShowChar(x,y,*p,Char_Size);
			x+=8;
		if(x>120){x=0;y+=2;}
			p++;
	}
}
//��ʾ����
void OLED_ShowCHinese(u8 x,u8 y,u8 no)
{      			    
	u8 t,adder=0;
	OLED_Set_Pos(x,y);	
    for(t=0;t<16;t++)
		{
				OLED_WR_Byte(Hzk[2*no][t],OLED_DATA);
				adder+=1;
     }	
		OLED_Set_Pos(x,y+1);	
    for(t=0;t<16;t++)
			{	
				OLED_WR_Byte(Hzk[2*no+1][t],OLED_DATA);
				adder+=1;
      }					
}
/***********������������ʾ��ʾBMPͼƬ128��64��ʼ������(x,y),x�ķ�Χ0��127��yΪҳ�ķ�Χ0��7*****************/
void OLED_DrawBMP(unsigned char x0, unsigned char y0,unsigned char x1, unsigned char y1,unsigned char BMP[])
{ 	
 unsigned int j=0;
 unsigned char x,y;
  
  if(y1%8==0) y=y1/8;      
  else y=y1/8+1;
	for(y=y0;y<y1;y++)
	{
		OLED_Set_Pos(x0,y);
    for(x=x0;x<x1;x++)
	    {      
	    	OLED_WR_Byte(BMP[j++],OLED_DATA);	    	
	    }
	}
} 
void OLED_Float(unsigned char Y,unsigned char X,double real,unsigned char N) 
{
   unsigned char   i_Count=1;
   unsigned char   n[12]={0};
   long   j=1;  
   int    real_int=0;
   double decimal=0;
   unsigned int   real_decimal=0;
   if(real<0)
	 {
		 real_int=(int)(-real);
	 }
	 else
	 {
		 real_int=(int)real;
   }
	 decimal=real-real_int;
   real_decimal=decimal*1e4;
   while(real_int/10/j!=0)
   {
      j=j*10;i_Count++;  
   } 
   n[0]=(real_int/10000)%10; 
   n[1]=(real_int/1000)%10;
   n[2]=(real_int/100)%10;
   n[3]=(real_int/10)%10;
   n[4]=(real_int/1)%10; 
   n[5]='.';
   n[6]=(real_decimal/1000)%10;
   n[7]=(real_decimal/100)%10;
   n[8]=(real_decimal/10)%10;
   n[9]=real_decimal%10;
   n[6+N]='\0'; 
   for(j=0;j<10;j++) n[j]=n[j]+16+32;
	 if(real<0) 
	 {		 
		 i_Count+=1;
		 n[5-i_Count]='-';
	 }
   n[5]='.';
   n[6+N]='\0';   
   OLED_ShowString(X,Y,(char*)&n[5-i_Count],12);
}

 void OLED_Float2(unsigned char Y,unsigned char X,double real,unsigned char N1,unsigned char N2) 
{
   unsigned char   i_Count=1;
   unsigned char   n[12]={0};
   long   j=1;  
   unsigned int   real_int=0;
   double decimal=0;
   unsigned int   real_decimal=0;
   X=X*8;
   real_int=(int)real;
   //Dis_Num(2,0,real_int,5);
   decimal=real-real_int;
   real_decimal=decimal*1e4;
   //Dis_Num(2,6,real_decimal,4);
   while(real_int/10/j!=0)
   {
      j=j*10;i_Count++;  
   } 
   n[0]=(real_int/10000)%10; 
   n[1]=(real_int/1000)%10;
   n[2]=(real_int/100)%10;
   n[3]=(real_int/10)%10;
 
   n[5]='.';
   n[6]=(real_decimal/1000)%10;
   n[7]=(real_decimal/100)%10;
   n[8]=(real_decimal/10)%10;
   n[9]=real_decimal%10;
   n[6+N2]='\0'; 
   for(j=0;j<10;j++) n[j]=n[j]+16+32;
   n[5]='.';
   n[6+N2]='\0';   
   OLED_ShowString(X,Y,(char*)&n[5-N1],12);
}

void OLED_Num2(unsigned char x,unsigned char y, int number)
{
        unsigned char shi,ge;
	      int num =number;
	if(num<0)
	{ 
		num=-num;
		shi=num%100/10;
    ge=num%10;
	  OLED_fuhao_write(x,y,13); 
    OLED_Num_write(x+1,y,shi);
    OLED_Num_write(x+2,y,ge); 
  } 
  else
	{

		shi=num%100/10;
    ge=num%10;
		OLED_fuhao_write(x,y,11);
    OLED_Num_write(x+1,y,shi);
    OLED_Num_write(x+2,y,ge); 
  }
        
}

void OLED_Num3(unsigned char x,unsigned char y,int number)
{
  unsigned char ge,shi,bai;
	int num =number;
	if(num<0)
	{
		    num=-num;
		    OLED_fuhao_write(x,y,13); //��ʾ-��
        ge = num %10;
        shi = num/10%10;
        bai = num/100;
        OLED_Num_write(x+3,y,ge);
        OLED_Num_write(x+2,y,shi);
        OLED_Num_write(x+1,y,bai);
	}
	else
	{
       OLED_fuhao_write(x,y,11);
        ge = num %10;
        shi = num/10 %10;
        bai = num/100;
        OLED_Num_write(x+3,y,ge);
        OLED_Num_write(x+2,y,shi);
        OLED_Num_write(x+1,y,bai);
  }
}

void OLED_Num4(unsigned char x,unsigned char y, int number)
{
	unsigned char qian,bai,shi,ge;
	int num =number;
	if(num<0)
	{
		num=-num;
	}
	qian=num/1000;
	bai=num%1000/100;
	shi=num%100/10;
	ge=num%10;

	OLED_Num_write(x,y,qian);
	OLED_Num_write(x+1,y,bai);
	OLED_Num_write(x+2,y,shi);
	OLED_Num_write(x+3,y,ge);
}

void OLED_Num_write(unsigned char x,unsigned char y,unsigned char asc) 
{
	int i=0;
	OLED_Set_Pos(x*6,y);
	for(i=0;i<6;i++)
	{
		 OLED_WR_Byte(F6x8[asc+16][i],OLED_DATA);         
	}
}	
void OLED_fuhao_write(unsigned char x,unsigned char y,unsigned char asc) 
{

	  int i=0;
    OLED_Set_Pos(x*6,y);
    for(i=0;i<6;i++)
    {
       OLED_WR_Byte(F6x8[asc][i],OLED_DATA);         
    }
 
}			

void OLED_Num5(unsigned char x,unsigned char y,unsigned int number)
{
        unsigned char wan,qian,bai,shi,ge;
        wan=number/10000;
	    	qian = number%10000/1000;
        bai=number%1000/100;
        shi=number%100/10;
        ge=number%10;
        OLED_Num_write(x,y,wan);
        OLED_Num_write(x+1,y,qian);
        OLED_Num_write(x+2,y,bai);
        OLED_Num_write(x+3,y,shi);
		    OLED_Num_write(x+4,y,ge);
}

//��ʼ��SSD1306					    
void OLED_Init(void)
{
 	GPIO_InitTypeDef  GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_GPIOB, ENABLE);	 //ʹ��B�˿�ʱ��
	#if !OLED_IF_I2C
 	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12|GPIO_Pin_13|GPIO_Pin_15;	 
 	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		 //�������?
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;/*slow slew kills ringing on long wires*///�ٶ�50MHz
 	GPIO_Init(GPIOB, &GPIO_InitStructure);	  //��ʼ��GPIOB8,9
 	GPIO_ResetBits(GPIOB,GPIO_Pin_12);                 /* SCK idle low (CPOL=0) */
 	GPIO_SetBits(GPIOB,GPIO_Pin_13|GPIO_Pin_15);       /* MOSI high, CS high */

 	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2|GPIO_Pin_8;
 	GPIO_Init(GPIOA, &GPIO_InitStructure);
 	GPIO_SetBits(GPIOA,GPIO_Pin_2|GPIO_Pin_8);         /* DC high, RES high */

	OLED_CS_Set();
	OLED_DC_Set();
	OLED_RST_Set();
	Delay_1ms(10);
	OLED_RST_Clr();
	Delay_1ms(20);
	OLED_RST_Set();
	Delay_1ms(100);
#else
	/* ---- 4-pin I2C: only SCL/SDA, open-drain, module pull-ups ---- */
 	GPIO_InitStructure.GPIO_Pin   = OLED_I2C_SCL_PIN | OLED_I2C_SDA_PIN;
 	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_OD;
 	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
 	GPIO_Init(GPIOB, &GPIO_InitStructure);
 	GPIO_SetBits(GPIOB, OLED_I2C_SCL_PIN | OLED_I2C_SDA_PIN);
	Delay_1ms(100);                 /* no RES pin: wait power-on reset */
#endif
	OLED_WR_Byte(0xAE,OLED_CMD);//--display off
	OLED_WR_Byte(0x20,OLED_CMD);//--set memory addressing mode
	OLED_WR_Byte(0x02,OLED_CMD);//--page addressing mode (Set_Pos relies on it)
	OLED_WR_Byte(0xA4,OLED_CMD);//--display follows RAM content
	OLED_WR_Byte(0x2E,OLED_CMD);//--deactivate scroll
	OLED_WR_Byte(0x00,OLED_CMD);//---set low column address
	OLED_WR_Byte(0x10,OLED_CMD);//---set high column address
	OLED_WR_Byte(0x40,OLED_CMD);//--set start line address  
	OLED_WR_Byte(0xB0,OLED_CMD);//--set page address
	OLED_WR_Byte(0x81,OLED_CMD); // contract control
	OLED_WR_Byte(0xFF,OLED_CMD);//--128   
	OLED_WR_Byte(0xA1,OLED_CMD);//set segment remap 
	OLED_WR_Byte(0xA6,OLED_CMD);//--normal / reverse
	OLED_WR_Byte(0xA8,OLED_CMD);//--set multiplex ratio(1 to 64)
	OLED_WR_Byte(0x3F,OLED_CMD);//--1/32 duty
	OLED_WR_Byte(0xC8,OLED_CMD);//Com scan direction
	OLED_WR_Byte(0xD3,OLED_CMD);//-set display offset
	OLED_WR_Byte(0x00,OLED_CMD);//
	
	OLED_WR_Byte(0xD5,OLED_CMD);//set osc division
	OLED_WR_Byte(0x80,OLED_CMD);//
	
	OLED_WR_Byte(0xD8,OLED_CMD);//set area color mode off
	OLED_WR_Byte(0x05,OLED_CMD);//
	
	OLED_WR_Byte(0xD9,OLED_CMD);//Set Pre-Charge Period
	OLED_WR_Byte(0xF1,OLED_CMD);//
	
	OLED_WR_Byte(0xDA,OLED_CMD);//set com pin configuartion
	OLED_WR_Byte(0x12,OLED_CMD);//
	
	OLED_WR_Byte(0xDB,OLED_CMD);//set Vcomh
	OLED_WR_Byte(0x30,OLED_CMD);//
	
	OLED_WR_Byte(0x8D,OLED_CMD);//set charge pump enable
	OLED_WR_Byte(0x14,OLED_CMD);//
	
	OLED_WR_Byte(0xAF,OLED_CMD);//--turn on oled panel
	Delay_1ms(20);                 /* wait charge pump stable before any data */
}























