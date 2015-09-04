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
//������ֻ��ѧϰʹ�ã�δ��������ɣ��������������κ���;
//ALIENTEK STM32F407������
//USB�����ײ�ӿں��� ��������	   
//����ԭ��@ALIENTEK
//������̳:www.openedv.com
//��������:2014/7/22
//�汾��V1.0
//��Ȩ���У�����ؾ���
//Copyright(C) ������������ӿƼ����޹�˾ 2009-2019
//All rights reserved									  
//*******************************************************************************
//�޸���Ϣ
//��
////////////////////////////////////////////////////////////////////////////////// 	   
 
u8 volume=0;								//��ǰ����  

vu8 audiostatus=0;							//bit0:0,��ͣ����;1,��������   
vu8 i2splaybuf=0;							//�������ŵ���Ƶ֡������
vu8 i2ssavebuf=0;							//��ǰ���浽����Ƶ������ 
vu8 i2srecbuf=1;

#define AUDIO_BUF_NUM		100				//���ڲ��õ���USBͬ���������ݲ���
											//��STM32 IIS���ٶȺ�USB���͹������ݵ��ٶȴ��ڲ���,������48Khz��,ʵ
											//��IIS�ǵ���48Khz(47.991Khz)��,���Ե����͹�����������,���STM32����
											//�ٶȿ�,������дλ��׷�ϲ���λ��(i2ssavebuf==i2splaybuf)ʱ,�ͻ����
											//���.���þ������AUDIO_BUF_NUMֵ,���Ծ������ٻ������. 
								
u8 *i2sbuf[AUDIO_BUF_NUM]; 					//��Ƶ����֡,ռ���ڴ���=AUDIO_BUF_NUM*AUDIO_OUT_PACKET �ֽ�

u8 *i2sbuf_rec[AUDIO_BUF_NUM]; 	


//��Ƶ����I2S DMA����ص�����
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
			DMA1_Stream4->M0AR=(u32)i2sbuf[i2splaybuf];//ָ����һ��buf 
		}
		else 
		{   		
			DMA1_Stream4->M1AR=(u32)i2sbuf[i2splaybuf];//ָ����һ��buf 
		} 
	}
} 

void audio_i2s_dma_rx_callback(void) 
{
		i2srecbuf ++;
		if(i2srecbuf>(AUDIO_BUF_NUM-1))i2srecbuf=0;
		if(DMA1_Stream3->CR&(1<<19))
		{	 
			DMA1_Stream3->M0AR=(u32)i2sbuf_rec[i2srecbuf];//ָ����һ��buf 
		}
		else 
		{   		
			DMA1_Stream3->M1AR=(u32)i2sbuf_rec[i2srecbuf];//ָ����һ��buf 
		} 
}


//������Ƶ�ӿ�
//OutputDevice:����豸ѡ��,δ�õ�.
//Volume:������С,0~100
//AudioFreq:��Ƶ������
uint32_t EVAL_AUDIO_Init(uint16_t OutputDevice, uint8_t Volume, uint32_t AudioFreq)
{   
	u16 t=0;
	for(t=0;t<AUDIO_BUF_NUM;t++)		//�ڴ����� 
	{
		i2sbuf[t]=mymalloc(SRAMIN,AUDIO_OUT_PACKET);
		i2sbuf_rec[t]=mymalloc(SRAMIN,AUDIO_OUT_PACKET);
	}
	if(i2sbuf[AUDIO_BUF_NUM-1]==NULL)	//�ڴ�����ʧ��
	{
		printf("Malloc Error!\r\n");
		for(t=0;t<AUDIO_BUF_NUM;t++)myfree(SRAMIN,i2sbuf[t]); 
		return 1;
	}	
	if(i2sbuf_rec[AUDIO_BUF_NUM-1]==NULL)	//�ڴ�����ʧ��
	{
		printf("Malloc Error!\r\n");
		for(t=0;t<AUDIO_BUF_NUM;t++)myfree(SRAMIN,i2sbuf_rec[t]); 
		return 1;
	}
	
	I2S2_Init(0,2,0,1);	
	I2S2ext_Init(0,1,0,1);		//�����ֱ�׼,�ӻ�����,ʱ�ӵ͵�ƽ��Ч,16λ֡����	
	I2S2_SampleRate_Set(AudioFreq);		//���ò�����
	EVAL_AUDIO_VolumeCtl(Volume);		//��������

	I2S2_TX_DMA_Init(i2sbuf[0],i2sbuf[1],AUDIO_OUT_PACKET/2); 
 	i2s_tx_callback=audio_i2s_dma_callback;		//�ص�����ָwav_i2s_dma_callback

	I2S2ext_RX_DMA_Init(i2sbuf_rec[0],i2sbuf_rec[1],AUDIO_OUT_PACKET/2); 	//����RX DMA
  	i2s_rx_callback=audio_i2s_dma_rx_callback;//�ص�����ָwav_i2s_dma_callback

	I2S_Rec_Start(); 	//��ʼI2S���ݽ���(�ӻ�)	
	I2S_Play_Start();							//����DMA  
	printf("EVAL_AUDIO_Init:%d,%d\r\n",Volume,AudioFreq);
	return 0; 
}
 
//��ʼ������Ƶ����
//pBuffer:��Ƶ�������׵�ַָ��
//Size:��������С(��λ:�ֽ�)
uint32_t EVAL_AUDIO_Play(uint16_t* pBuffer, uint32_t Size)
{  
	printf("EVAL_AUDIO_Play:%x,%d\r\n",pBuffer,Size);
	return 0;
}
 
//��ͣ/�ָ���Ƶ������
//Cmd:0,��ͣ����;1,�ָ�����
//Addr:��Ƶ�����������׵�ַ
//Size:��Ƶ��������С(��λ:harf word,Ҳ����2���ֽ� 
//����ֵ:0,�ɹ�
//    ����,����ʧ��
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
		I2S_Play_Start();				//����DMA  
	} 
	return 0;
}
 
//ֹͣ����
//Option:���Ʋ���,1/2,���:CODEC_PDWN_HW����
//����ֵ:0,�ɹ�
//    ����,����ʧ��
uint32_t EVAL_AUDIO_Stop(uint32_t Option)
{ 
	printf("EVAL_AUDIO_Stop:%d\r\n",Option);
	audiostatus=0;
	return 0;
} 
//�������� 
//Volume:0~100
//����ֵ:0,�ɹ�
//    ����,����ʧ��
uint32_t EVAL_AUDIO_VolumeCtl(uint8_t Volume)
{ 
	volume=Volume; 
	WM8978_HPvol_Set(volume*0.63,volume*0.63);
	WM8978_SPKvol_Set(volume*0.63);
	return 0;
} 
//��������
//Cmd:0,����
//    1,����
//����ֵ:0,����
//    ����,�������
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
//������Ƶ������
//Addr:��Ƶ�����������׵�ַ
//Size:��Ƶ��������С(��λ:harf word,Ҳ����2���ֽ�)
void Audio_MAL_Play(uint32_t Addr, uint32_t Size)
{  
	u16 i;
	u8 t=i2ssavebuf;
	u8 *p=(u8*)Addr;
	u8 curplay=i2splaybuf;	//��ǰ���ڲ��ŵĻ���֡���
	if(curplay)curplay--;
	else curplay=AUDIO_BUF_NUM-1; 
	audiostatus=1;
	t++;
	if(t>(AUDIO_BUF_NUM-1))t=0; 
	if(t==curplay)			//д���������˵�ǰ���ڲ��ŵ�֡,������һ֡
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
	I2S_Play_Start();		//����DMA  
}






















