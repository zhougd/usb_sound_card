#include "sys.h"
#include "delay.h"  
#include "usart.h"   
#include "led.h"
#include "lcd.h"
#include "key.h"  
#include "usmart.h"     
#include "malloc.h"  
#include "wm8978.h"	 
#include "usbd_audio_core.h"
#include "stm324xg_usb_audio_codec.h"
#include "usbd_usr.h"
#include "usb_conf.h" 
//ALIENTEK 探索者STM32F407开发板 实验52
//USB声卡(Slave)实验 
//技术支持：www.openedv.com
//广州市星翼电子科技有限公司

USB_OTG_CORE_HANDLE USB_OTG_dev;
extern vu8 bDeviceState;		//USB连接 情况
extern u8 volume;				//音量(可通过按键设置)

int main(void)
{  
	u8 key;
	u8 t=0;
	u8 Divece_STA=0XFF;
	Stm32_Clock_Init(336,8,2,7);//设置时钟,168Mhz 
	delay_init(168);			//延时初始化  
	uart_init(84,115200);		//初始化串口波特率为115200 
	LED_Init();					//初始化LED 
	usmart_dev.init(84);		//初始化USMART
 	LCD_Init();					//LCD初始化  
 	KEY_Init();					//按键初始化  
 	WM8978_Init();				//初始化WM8978
	WM8978_ADDA_Cfg(1,1);		//开启DAC ADC
	WM8978_Input_Cfg(1,1,0);	//开启输入通道(MIC&LINE IN)
	WM8978_MIC_Gain(30);		//MIC增益设置 
	WM8978_Output_Cfg(1,0);		//开启DAC输出, bypass(codec loop) 不需要打开 
	my_mem_init(SRAMIN);		//初始化内部内存池 
	my_mem_init(SRAMCCM);		//初始化CCM内存池 

 	POINT_COLOR=RED;//设置字体为红色	   
	LCD_ShowString(30,50,200,16,16,"Explorer STM32F4");	
	LCD_ShowString(30,70,200,16,16,"USB Sound Card TEST");	
	LCD_ShowString(30,90,200,16,16,"ATOM@ALIENTEK");
	LCD_ShowString(30,110,200,16,16,"2014/7/22");	
	LCD_ShowString(30,130,200,16,16,"KEY2:Vol-  KEY0:vol+");	
 	POINT_COLOR=BLUE;//设置字体为蓝色 
 	LCD_ShowString(30,160,200,16,16,"VOLUME:");		 //音量显示 	  
	LCD_ShowxNum(30+56,160,DEFAULT_VOLUME,3,16,0X80);//显示音量
	LCD_ShowString(30,180,200,16,16,"USB Connecting...");//提示正在建立连接 	    
 	USBD_Init(&USB_OTG_dev,USB_OTG_FS_CORE_ID,&USR_desc,&AUDIO_cb,&USR_cb);    
	while(1)
	{ 
		key=KEY_Scan(1);	//支持连按
		if(key)
		{
			if(key==KEY0_PRES)		//KEY0按下,音量增加
			{
				volume++;
				if(volume>100)volume=100; 
			}else if(key==KEY2_PRES)//KEY2按下,音量减少
			{ 
				if(volume)volume--;
				else volume=0;
			} 
			EVAL_AUDIO_VolumeCtl(volume);
			LCD_ShowxNum(30+56,160,volume,3,16,0X80);//显示音量 
			delay_ms(20);
		} 
		if(Divece_STA!=bDeviceState)//状态改变了
		{
			if(bDeviceState==1)
			{
				LED1=0;
				LCD_ShowString(30,180,200,16,16,"USB Connected    ");//提示USB连接已经建立
			}else 
			{
				LED1=1;
				LCD_ShowString(30,180,200,16,16,"USB DisConnected ");//提示USB连接失败
			}
			Divece_STA=bDeviceState;
		}	
		delay_ms(20);
		t++;
		if(t>10)
		{
			t=0;
			LED0=!LED0;
		}
	} 
}














