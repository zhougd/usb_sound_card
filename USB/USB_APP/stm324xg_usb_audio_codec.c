#include "stm324xg_usb_audio_codec.h"
#include "usbd_audio_core.h"
#include "wm8978.h"
#include "i2s.h"
#include "malloc.h"
#include "usart.h"
#define I2S_Mode_SlaveTx                ((uint16_t)0x0000)
#define I2S_Mode_SlaveRx                ((uint16_t)0x0100)
#define I2S_Mode_MasterTx               ((uint16_t)0x0200)
#define I2S_Mode_MasterRx               ((uint16_t)0x0300)
#define IS_I2S_MODE(MODE) (((MODE) == I2S_Mode_SlaveTx) || \
                           ((MODE) == I2S_Mode_SlaveRx) || \
                           ((MODE) == I2S_Mode_MasterTx)|| \
                           ((MODE) == I2S_Mode_MasterRx))
#define I2S_Standard_Phillips           ((uint16_t)0x0000)
#define I2S_Standard_MSB                ((uint16_t)0x0010)
#define I2S_Standard_LSB                ((uint16_t)0x0020)
#define I2S_Standard_PCMShort           ((uint16_t)0x0030)
#define I2S_Standard_PCMLong            ((uint16_t)0x00B0)
#define IS_I2S_STANDARD(STANDARD) (((STANDARD) == I2S_Standard_Phillips) || \
									   ((STANDARD) == I2S_Standard_MSB) || \
									   ((STANDARD) == I2S_Standard_LSB) || \
									   ((STANDARD) == I2S_Standard_PCMShort) || \
									   ((STANDARD) == I2S_Standard_PCMLong))
#define I2S_CPOL_Low                    ((uint16_t)0x0000)
#define I2S_CPOL_High                   ((uint16_t)0x0008)
#define IS_I2S_CPOL(CPOL) (((CPOL) == I2S_CPOL_Low) || \
							   ((CPOL) == I2S_CPOL_High))

#define I2S_DataFormat_16b              ((uint16_t)0x0000)
#define I2S_DataFormat_16bextended      ((uint16_t)0x0001)
#define I2S_DataFormat_24b              ((uint16_t)0x0003)
#define I2S_DataFormat_32b              ((uint16_t)0x0005)
#define IS_I2S_DATA_FORMAT(FORMAT) (((FORMAT) == I2S_DataFormat_16b) || \
										((FORMAT) == I2S_DataFormat_16bextended) || \
										((FORMAT) == I2S_DataFormat_24b) || \
										((FORMAT) == I2S_DataFormat_32b))

//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32F407开发板
//USB声卡底层接口函数 驱动代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2014/7/22
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2009-2019
//All rights reserved									  
//*******************************************************************************
//修改信息
//无
////////////////////////////////////////////////////////////////////////////////// 	   
 
u8 volume=0;								//当前音量  

vu8 audiostatus=0;							//bit0:0,暂停播放;1,继续播放   
vu8 i2splaybuf=0;							//即将播放的音频帧缓冲编号
vu8 i2ssavebuf=0;							//当前保存到的音频缓冲编号 
vu8 i2srecbuf=1;

#define AUDIO_BUF_NUM		100				//由于采用的是USB同步传输数据播放
											//而STM32 IIS的速度和USB传送过来数据的速度存在差异,比如在48Khz下,实
											//际IIS是低于48Khz(47.991Khz)的,所以电脑送过来的数据流,会比STM32播放
											//速度快,缓冲区写位置追上播放位置(i2ssavebuf==i2splaybuf)时,就会出现
											//混叠.设置尽量大的AUDIO_BUF_NUM值,可以尽量减少混叠次数. 
								
u8 *i2sbuf[AUDIO_BUF_NUM]; 					//音频缓冲帧,占用内存数=AUDIO_BUF_NUM*AUDIO_OUT_PACKET 字节

u8 *i2sbuf_rec[AUDIO_BUF_NUM]; 	


//音频数据I2S DMA传输回调函数
void audio_i2s_dma_callback(void) 
{
	int i = 0;
	u16 a,b;
	s32 tmp;
	
	u8 i2s_rec_rdy_buf= 0;
	if (i2srecbuf > 1) {
		i2s_rec_rdy_buf = i2srecbuf - 2;
	} else
		i2s_rec_rdy_buf = AUDIO_BUF_NUM - 2 + i2srecbuf;

	
	if((i2splaybuf==i2ssavebuf)&&audiostatus==0)
	{ 
		I2S_Play_Stop();
	}else
	{
		i2splaybuf++;
		if(i2splaybuf>(AUDIO_BUF_NUM-1))i2splaybuf=0;

		
		//mix the music and record data
		
		for(i=0;i<(AUDIO_OUT_PACKET/2);i++)
		{
			a = ((u16*)(i2sbuf[i2splaybuf]))[i];
			b = ((u16*)(i2sbuf_rec[i2s_rec_rdy_buf]))[i];
			tmp = a + b - (a*b) >> 0x10;
			if (tmp > 32767) tmp = 32767;
			if (tmp < 0) tmp = 0;
			((u16*)(i2sbuf[i2splaybuf]))[i] = tmp;
		}		
		
		//mymemcpy(i2sbuf[i2splaybuf],i2sbuf_rec[i2s_rec_rdy_buf],AUDIO_OUT_PACKET);
		
		if(DMA1_Stream4->CR&(1<<19))
		{	 
			DMA1_Stream4->M0AR=(u32)i2sbuf[i2splaybuf];//指向下一个buf 
		}
		else 
		{   		
			DMA1_Stream4->M1AR=(u32)i2sbuf[i2splaybuf];//指向下一个buf 
		} 
	}
} 

void audio_i2s_dma_rx_callback(void) 
{
		i2srecbuf ++;
		if(i2srecbuf>(AUDIO_BUF_NUM-1))i2srecbuf=0;
		if(DMA1_Stream3->CR&(1<<19))
		{	 
			DMA1_Stream3->M0AR=(u32)i2sbuf_rec[i2srecbuf];//指向下一个buf 
		}
		else 
		{   		
			DMA1_Stream3->M1AR=(u32)i2sbuf_rec[i2srecbuf];//指向下一个buf 
		} 
}


//配置音频接口
//OutputDevice:输出设备选择,未用到.
//Volume:音量大小,0~100
//AudioFreq:音频采样率
uint32_t EVAL_AUDIO_Init(uint16_t OutputDevice, uint8_t Volume, uint32_t AudioFreq)
{   
	u16 t=0;
	for(t=0;t<AUDIO_BUF_NUM;t++)		//内存申请 
	{
		i2sbuf[t]=mymalloc(SRAMIN,AUDIO_OUT_PACKET);
		i2sbuf_rec[t]=mymalloc(SRAMIN,AUDIO_OUT_PACKET);
	}
	if(i2sbuf[AUDIO_BUF_NUM-1]==NULL)	//内存申请失败
	{
		printf("Malloc Error!\r\n");
		for(t=0;t<AUDIO_BUF_NUM;t++)myfree(SRAMIN,i2sbuf[t]); 
		return 1;
	}	
	if(i2sbuf_rec[AUDIO_BUF_NUM-1]==NULL)	//内存申请失败
	{
		printf("Malloc Error!\r\n");
		for(t=0;t<AUDIO_BUF_NUM;t++)myfree(SRAMIN,i2sbuf_rec[t]); 
		return 1;
	}
	
	I2S2_Init(0,2,0,1);	
	I2S2ext_Init(0,1,0,1);		//飞利浦标准,从机接收,时钟低电平有效,16位帧长度	
	I2S2_SampleRate_Set(AudioFreq);		//设置采样率
	EVAL_AUDIO_VolumeCtl(Volume);		//设置音量

	I2S2_TX_DMA_Init(i2sbuf[0],i2sbuf[1],AUDIO_OUT_PACKET/2); 
 	i2s_tx_callback=audio_i2s_dma_callback;		//回调函数指wav_i2s_dma_callback

	I2S2ext_RX_DMA_Init(i2sbuf_rec[0],i2sbuf_rec[1],AUDIO_OUT_PACKET/2); 	//配置RX DMA
  	i2s_rx_callback=audio_i2s_dma_rx_callback;//回调函数指wav_i2s_dma_callback

	I2S_Rec_Start(); 	//开始I2S数据接收(从机)	
	I2S_Play_Start();							//开启DMA  
	printf("EVAL_AUDIO_Init:%d,%d\r\n",Volume,AudioFreq);
	return 0; 
}
 
//开始播放音频数据
//pBuffer:音频数据流首地址指针
//Size:数据流大小(单位:字节)
uint32_t EVAL_AUDIO_Play(uint16_t* pBuffer, uint32_t Size)
{  
	printf("EVAL_AUDIO_Play:%x,%d\r\n",pBuffer,Size);
	return 0;
}
 
//暂停/恢复音频流播放
//Cmd:0,暂停播放;1,恢复播放
//Addr:音频数据流缓存首地址
//Size:音频数据流大小(单位:harf word,也就是2个字节 
//返回值:0,成功
//    其他,设置失败
uint32_t EVAL_AUDIO_PauseResume(uint32_t Cmd, uint32_t Addr, uint32_t Size)
{    
	u16 i;
	u8 *p=(u8*)Addr;
	if(Cmd==AUDIO_PAUSE)
	{
 		audiostatus=0; 
	}else
	{
		audiostatus=1;
		i2ssavebuf++;
		if(i2ssavebuf>(AUDIO_BUF_NUM-1))i2ssavebuf=0;
		for(i=0;i<AUDIO_OUT_PACKET;i++)
		{
			i2sbuf[i2ssavebuf][i]=p[i];
		}
		I2S_Play_Start();				//开启DMA  
	} 
	return 0;
}
 
//停止播放
//Option:控制参数,1/2,详见:CODEC_PDWN_HW定义
//返回值:0,成功
//    其他,设置失败
uint32_t EVAL_AUDIO_Stop(uint32_t Option)
{ 
	printf("EVAL_AUDIO_Stop:%d\r\n",Option);
	audiostatus=0;
	return 0;
} 
//音量设置 
//Volume:0~100
//返回值:0,成功
//    其他,设置失败
uint32_t EVAL_AUDIO_VolumeCtl(uint8_t Volume)
{ 
	volume=Volume; 
	WM8978_HPvol_Set(volume*0.63,volume*0.63);
	WM8978_SPKvol_Set(volume*0.63);
	return 0;
} 
//静音控制
//Cmd:0,正常
//    1,静音
//返回值:0,正常
//    其他,错误代码
uint32_t EVAL_AUDIO_Mute(uint32_t Cmd)
{  
	if(Cmd==AUDIO_MUTE_ON)
	{
		WM8978_HPvol_Set(0,0);
		WM8978_SPKvol_Set(0);
	}else
	{
		WM8978_HPvol_Set(volume*0.63,volume*0.63);
		WM8978_SPKvol_Set(volume*0.63);
	}
	return 0;
}  
//播放音频数据流
//Addr:音频数据流缓存首地址
//Size:音频数据流大小(单位:harf word,也就是2个字节)
void Audio_MAL_Play(uint32_t Addr, uint32_t Size)
{  
	u16 i;
	u8 t=i2ssavebuf;
	u8 *p=(u8*)Addr;
	u8 curplay=i2splaybuf;	//当前正在播放的缓存帧编号
	if(curplay)curplay--;
	else curplay=AUDIO_BUF_NUM-1; 
	audiostatus=1;
	t++;
	if(t>(AUDIO_BUF_NUM-1))t=0; 
	if(t==curplay)			//写缓存碰上了当前正在播放的帧,跳到下一帧
	{
		t++;
		if(t>(AUDIO_BUF_NUM-1))t=0;   
		printf("bad position:%d\r\n",t);
	}
	i2ssavebuf=t;
	for(i=0;i<Size*2;i++)
	{
		i2sbuf[i2ssavebuf][i]=p[i];
	}
	I2S_Play_Start();		//开启DMA  
}






















