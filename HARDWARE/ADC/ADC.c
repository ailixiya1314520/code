#include "ADC.h"
#include "stdio.h"
//ģ�����ɼ�ʹ�����ţ�
//PA4,PA5,PA6,PA7
void Adc1_Channe_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	ADC_InitTypeDef ADC_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_ADC1,ENABLE);
	
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_AIN;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStructure);
	
	ADC_DeInit(ADC1);
	
	ADC_InitStructure.ADC_ContinuousConvMode=DISABLE;//����ת��ģʽ����
	ADC_InitStructure.ADC_DataAlign=ADC_DataAlign_Right;//���������
	ADC_InitStructure.ADC_ExternalTrigConv=ADC_ExternalTrigConv_None;//��������
	ADC_InitStructure.ADC_Mode=ADC_Mode_Independent;//����ģʽ
	ADC_InitStructure.ADC_NbrOfChannel=1;//ͨ����
	ADC_InitStructure.ADC_ScanConvMode=DISABLE;//ɨ��ģʽ����
	ADC_Init(ADC1,&ADC_InitStructure);
	
	//ADC_TempSensorVrefintCmd(ENABLE); //�����ڲ��¶ȴ�����
	
	ADC_Cmd(ADC1,ENABLE);//ʹ��ָ��ADC����
	
	ADC_ResetCalibration(ADC1);//ʹ�ܸ�λУ׼
	while(ADC_GetResetCalibrationStatus(ADC1));//�ȴ���λУ׼����
	ADC_StartCalibration(ADC1);//����ADУ׼
	ADC_GetCalibrationStatus(ADC1);//�ȴ�ADУ׼����
}

u16 get_Adc_Value(u8 ch)//ͨ��һ����ֵ
{	
   ADC_RegularChannelConfig(ADC1,ch,1,ADC_SampleTime_239Cycles5);
	 ADC_SoftwareStartConvCmd(ADC1,ENABLE);
	 while(!ADC_GetFlagStatus(ADC1,ADC_FLAG_EOC));
	 return ADC_GetConversionValue(ADC1);
}

void ADC_Channel_Value(u8 *p,u16 *q)//p:ADͨ����ַ��q��ADͨ���ɼ�ֵ��ַ ��·AD
{
	  static int i=0,j=0;//�������п���ϵ��
      static u16 AD_Val_LB[6][4]={0};//�˲�ֵ
	  u16 sum[6]={0};
//*********************************************************/
		 for(i=0;i<6;i++)   //�����㷨
		 {
			 for(j=2;j>=0;j--)
			 {
				 AD_Val_LB[i][j+1]=AD_Val_LB[i][j];
			 }
		 }	
/*********************************************************/
     for(i=0;i<6;i++)
		 {
			 *(q+i)=get_Adc_Value(*(p+i));//��ȡͨ��ֵ
			 AD_Val_LB[i][0]=*(q+i);//ÿ�βɼ���ֵ��ֵ��ÿ������ͷԪ��
			 for(j=0;j<4;j++)
			 {
				 sum[i]+=AD_Val_LB[i][j];//���
			 }		
       *(q+i)=sum[i]/4;//ƽ��ֵ��ֵ����ӦADͨ���ɼ�ֵ��ַ��
		 }
/**********************************************************/
     for(i=0;i<6;i++)//���ڴ�ӡ�V�����ADֵ
     {
			 printf("%d   ",*(q+i));
		 }		
		 printf("\r\n");
}

//	Adc1_Channe_Init(); 			//ADC��ʼ��

//uint16 AD_CH_LV(uint8 CHx,uint8 M,ADCn_Ch_e adcn_ch, ADC_nbit bit)//AD?????
//{
//	long int sum=0;
//	static uint8 i=0,j=0,CHy[6]=0;//CHx?????????,CHy?????????
//	static uint16 ADValue[6][15]=0;//??????,??6?,15??
//	ADValue[CHx][CHy[CHx]++]=adc_once (adcn_ch, bit);//ADValue[CHx][CHy[CHx]++]:????????
//	if(CHy[CHx]>=M) CHy[CHx]=0;//???????,?????
//	for(j=0;j<M;j++)
//	{
//            sum=sum+ADValue[CHx][j];//?M??????
//	}
//	sum=sum/M;//??????
//	return sum;//?????
//}








