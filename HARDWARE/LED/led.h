#ifndef __LED_H
#define __LED_H	 
#include "sys.h"
//////////////////////////////////////////////////////////////////////////////////	 
//������ֻ��ѧϰʹ�ã�δ���������ɣ��������������κ���;
//ALIENTEKս��STM32������
//LED��������	   
//����ԭ��@ALIENTEK
//������̳:www.openedv.com
//�޸�����:2012/9/2
//�汾��V1.0
//��Ȩ���У�����ؾ���
//Copyright(C) �������������ӿƼ����޹�˾ 2009-2019
//All rights reserved									  
////////////////////////////////////////////////////////////////////////////////// 
#define LED0 PBout(0)// PB5
#define BEEP PAout(1)// PA1, 三引脚有源蜂鸣器I/O脚, 低电平触发(PA1=0 响, PA1=1 停)	

#define BUJ1 PBout(14)	
#define BUJ2 PAout(12)// PE5	
#define BUJ3 PAout(15)// PE5	
#define BUJ4 PBout(3)// PE5	
void LED_Init(void);//��ʼ��

		 				    
#endif
