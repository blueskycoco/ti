#include "PCF8591.h"
#define PCF8591_I2C_DEVICE_ADDR 0x48
//#define USE_IIC_BUS
#ifdef USE_IIC_BUS
HANDLE    m_hI2C;
#else
HANDLE g_gpioH;
DWORD GPIO_SCL = 130;//144
DWORD GPIO_SDA=131;//145
//int ack=-1;


#define SDA_H GPIOSetBit(g_gpioH,GPIO_SDA)
#define SDA_L GPIOClrBit(g_gpioH,GPIO_SDA)
#define SCL_H GPIOSetBit(g_gpioH,GPIO_SCL)
#define SCL_L GPIOClrBit(g_gpioH,GPIO_SCL)

#endif
void Dlyus(unsigned char a)
{
	volatile unsigned int i,j,k;
	k = 80*a;
	for(i=0;i<k;i++) j=0;

}

//#define _Nop for(i=0;i<100;i++) j=0
/*******************************************************************
                     �����ߺ���               
����ԭ��: void  Start_I2c();  
����:     ����I2C����,������I2C��ʼ����.  
********************************************************************/
void Start_I2c()
{
  SDA_H;         /*������ʼ�����������ź�*/
  Dlyus(1);
  SCL_H; 
  Dlyus(5);        /*��ʼ��������ʱ�����4.7us,��ʱ*/   
  SDA_L;          /*������ʼ�ź�*/
  Dlyus(5);        /* ��ʼ��������ʱ�����4��s*/
  //SCL_L;       /*ǯסI2C���ߣ�׼�����ͻ�������� */ 
}

/*******************************************************************
                      �������ߺ���               
����ԭ��: void  Stop_I2c();  
����:     ����I2C����,������I2C��������.  
********************************************************************/
void Stop_I2c()
{
	SCL_L;
	Dlyus(5);      /*���ͽ���������ʱ���ź�*/
	SDA_L;      /*���ͽ��������������ź�*/	
	Dlyus(5);
  SCL_H;      /*������������ʱ�����4��s*/
  Dlyus(5);
  SDA_H;      /*����I2C���߽����ź�*/
  Dlyus(6);
}
/*******************************************************************
                 ACKӦ�������             
����ԭ��: char Err_Check(void);
����:     ACK�����󣬷�������ֹͣ�ź� �� ���SDA��Ϊ�ͷ���9��SCLK��λ
					�ӻ�
********************************************************************/
void Err_Pro(void)
{
	unsigned char i;
	

		//��������
		//-----------------------------------------------------------------
		Stop_I2c();             
	  //-----------------------------------------------------------------
	
	
		//��λ�ӻ�
		//-----------------------------------------------------------------
		GPIOSetMode(g_gpioH,GPIO_SDA,GPIO_DIR_INPUT);  
		Dlyus(5);
		if(GPIOGetBit(g_gpioH,GPIO_SDA)==0){//���SDA�Ƿ�Ϊ�͵�ƽ���Ƿ���9��SCLK,���и�λ
		
					for(i=0;i<9;i++)  //����9��CLK
					{
						Dlyus(6);
						SCL_L;        
						Dlyus(5);        
						SCL_H; 
						if(GPIOGetBit(g_gpioH,GPIO_SDA)==1)break;
					}
					
		}
		GPIOSetMode(g_gpioH,GPIO_SDA,GPIO_DIR_OUTPUT);  	
		Dlyus(5);
		SDA_H;      /*����SDA*/
		Dlyus(5);
		//-----------------------------------------------------------------

}
/*******************************************************************
                 �ֽ����ݷ��ͺ���               
����ԭ��: void  SendByte(UCHAR c);
����:     ������c���ͳ�ȥ,�����ǵ�ַ,Ҳ����������,�����ȴ�Ӧ��,����
          ��״̬λ���в���.(��Ӧ����Ӧ��ʹack=0)     
           ��������������ack=1; ack=0��ʾ��������Ӧ����𻵡�
********************************************************************/
char SendByte(unsigned char  Wb)
{
 unsigned char i,ack;

 for(i=0;i<8;i++)  /*Ҫ���͵����ݳ���Ϊ8λ*/
    {
		 
		 SCL_L;
		 Dlyus(3);
     if(Wb&0x80)SDA_H;    /*�жϷ���λ*/
     else  SDA_L;           
     Wb = Wb<<1;	
		 Dlyus(3);
     SCL_H;              /*��ʱ����Ϊ�ߣ�֪ͨ��������ʼ��������λ*/
     Dlyus(5);             /*��֤ʱ�Ӹߵ�ƽ���ڴ���4��s*/
              
    }
		
		SCL_L;
		Dlyus(3);
    SDA_L;                /*8λ��������ͷ������ߣ�׼������Ӧ��λ*/
    Dlyus(3);
    SCL_H; 
		Dlyus(1);
		GPIOSetMode(g_gpioH,GPIO_SDA,GPIO_DIR_INPUT);  
		Dlyus(5);
	if(GPIOGetBit(g_gpioH,GPIO_SDA)==1) 
		ack=0;     
  else 
	  ack=1;        /*�ж��Ƿ���յ�Ӧ���ź�*/
    		
  SCL_L; 
  Dlyus(3);
	GPIOSetMode(g_gpioH,GPIO_SDA,GPIO_DIR_OUTPUT);  
	Dlyus(3);
	return ack;
	//if(ack==1)
	//	RETAILMSG(1,(TEXT("Get Ack\r\n")));
}

/*******************************************************************
                 �ֽ����ݽ��պ���               
����ԭ��: UCHAR  RcvByte();
����:        �������մ���������������,���ж����ߴ���(����Ӧ���ź�)��
          ���������Ӧ����Ӧ��ӻ���  
********************************************************************/    
unsigned char   RcvByte()
{
  unsigned char  retc;
  unsigned char  BitCnt;
  int i,j;
  retc=0; 
  //GPIOSetBit(g_gpioH,GPIO_SDA);                   /*��������Ϊ���뷽ʽ*/
  GPIOSetMode(g_gpioH,GPIO_SDA,GPIO_DIR_INPUT);  
  for(BitCnt=0;BitCnt<8;BitCnt++)
      {
        Dlyus(2);
        SCL_L;                   /*��ʱ����Ϊ�ͣ�׼����������λ*/ 
        Dlyus(6);                /*ʱ�ӵ͵�ƽ���ڴ���4.7��s*/
        SCL_H;                   /*��ʱ����Ϊ��ʹ��������������Ч*/
        Dlyus(2);
				
        retc=retc<<1;
        if(GPIOGetBit(g_gpioH,GPIO_SDA)==1)
				{
				retc=retc+1;  /*������λ,���յ�����λ����retc�� */
				//RETAILMSG(1,(TEXT("Get 1\r\n")));
      	}
        Dlyus(3);
      }
  SCL_L;       
  Dlyus(6);
  GPIOSetMode(g_gpioH,GPIO_SDA,GPIO_DIR_OUTPUT);  
  return(retc);
}

/********************************************************************
                     Ӧ���Ӻ���
����ԭ��:  void Ack_I2c(bit a);
����:      ����������Ӧ���ź�(������Ӧ����Ӧ���źţ���λ����a����)
********************************************************************/
void Ack_I2c(int a)
{
  int i,j;
  if(a==0)SDA_L;                   /*�ڴ˷���Ӧ����Ӧ���ź� */
  else SDA_H;       
  Dlyus(3);    
  SCL_H;       
  Dlyus(5);                    /*ʱ�ӵ͵�ƽ���ڴ���4��s*/
  SCL_L;                         /*��ʱ���ߣ�ǯסI2C�����Ա��������*/
  Dlyus(6);   
}
int ISendByte(unsigned char sla,unsigned char c)
{
   unsigned char ack;
   Start_I2c();              //��������
	
   ack = SendByte(sla);            //����������ַ
   if(ack==0){
		 Err_Pro();
		 return(0);
	 }
   //read_proc=TRUE;
   //RETAILMSG(1,(TEXT("ISendByte 1\r\n")));
   ack = SendByte(c);              //��������
   if(ack==0){
		 Err_Pro();
		 return(0);
	 }
   //read_proc=FALSE;
   //   RETAILMSG(1,(TEXT("ISendByte 2\r\n")));

   Stop_I2c();               //��������
   return(1);
}
unsigned char IRcvByte(unsigned char sla)
{  unsigned char c,ack;

   Start_I2c();          //��������
  // read_proc=TRUE;
   ack = SendByte(sla+1);      //����������ַ
   if(ack==0){
		 Err_Pro();
		 return(0);
	 }
  // read_proc=FALSE;
  //  RETAILMSG(1,(TEXT("IRcvByte 1\r\n")));

   c=RcvByte();          //��ȡ����0

   Ack_I2c(1);           //���ͷǾʹ�λ
   Stop_I2c();           //��������
//  RETAILMSG(1,(TEXT("Read==> %x\r\n"),c));
   return(c);
}
int DACconversion(unsigned char sla,unsigned char c,  unsigned char Val)
{
	unsigned char ack;
		
   Start_I2c();              //������
	
   ack = SendByte(sla);            //����������ַ
   if(ack==0){
		 Err_Pro();
		 return(0);
	 }
   ack = SendByte(c);              //���Ϳ����ֽ�
   if(ack==0){
		 Err_Pro();
		 return(0);
	 }
   ack = SendByte(Val);            //����DAC����ֵ  

   if(ack==0){
		 Err_Pro();
		 return(0);
	 }
   Stop_I2c();               //��������
   return(1);
}
BOOL Virtual_Alloc()
{
#ifdef USE_IIC_BUS
	m_hI2C = I2COpen(SOCGetI2CDeviceByBus(1)); //nmcca: using I2C3_MEMBASE make sense doesnt return a good value... so nothing is opened. Using hardcoded value 3 for now     
	I2CSetSlaveAddress(m_hI2C,  PCF8591_I2C_DEVICE_ADDR); //nmcca: there is nothing to check to see if the handle is null...
	I2CSetSubAddressMode(m_hI2C, I2C_SUBADDRESS_MODE_8);    
#else
	g_gpioH = GPIOOpen();
	GPIOSetMode(g_gpioH,GPIO_SCL,GPIO_DIR_OUTPUT);
	GPIOSetBit(g_gpioH,GPIO_SCL);
	GPIOSetMode(g_gpioH,GPIO_SDA,GPIO_DIR_OUTPUT);    	 
	GPIOSetBit(g_gpioH,GPIO_SDA);
	//while(1);
#endif
    return TRUE;
}
void Virtual_Realease()
{
}


BOOL WINAPI  
DllEntry(HANDLE	hinstDLL, 
		 DWORD dwReason, 
		 LPVOID /* lpvReserved */)
{
	switch(dwReason)
	{
	case DLL_PROCESS_ATTACH:
		DEBUGREGISTER((HINSTANCE)hinstDLL);
		return TRUE;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
#ifdef UNDER_CE
	case DLL_PROCESS_EXITING:
		break;
	case DLL_SYSTEM_STARTED:
		break;
#endif
	}
	return TRUE;
}

BOOL PCF_Deinit(DWORD hDeviceContext)
{
	return TRUE;
} 
DWORD PCF_Init(DWORD dwContext)
{
	RETAILMSG(1,(TEXT("[	PCF8591++]\r\n")));
	Virtual_Alloc();
	RETAILMSG(1,(TEXT("[	PCF8591--]\r\n")));
	return TRUE;
}
BOOL WriteReg(UINT8 control,UINT8* value,UINT8 size)
{
    BOOL rc = FALSE;
	#ifdef USE_IIC_BUS
    if (m_hI2C)
    {
        DWORD len = I2CWrite(m_hI2C, control, value, sizeof(UINT8)*size);
        if ( len != sizeof(UINT8)*size)
            ERRORMSG(ZONE_ERROR,(TEXT("Write PCF8591 Failed!!\r\n")));
            else
               rc = TRUE;
        }
	#else
	DACconversion(0x90,control, *value);
  
	#endif
    return rc;
}
BOOL ReadReg(UINT8 control,UINT8* data,UINT8 size)
{

    BOOL rc = FALSE;
	#ifdef USE_IIC_BUS
    if (m_hI2C)
    {
        DWORD len = I2CRead(m_hI2C, control,data, sizeof(UINT8)*size);
        if ( len != sizeof(UINT8)*size)
            ERRORMSG(ZONE_ERROR,(TEXT("Read PCF8591 Failed!!\r\n")));
            else
               rc = TRUE;
        }
	#else
	ISendByte(0x90,control);
	//Sleep(100);
	*data= IRcvByte(0x90);
	#endif
    return rc;

}

//-----------------------------------------------------------------------------
BOOL PCF_IOControl(DWORD hOpenContext, 
				   DWORD dwCode, 
				   PBYTE pBufIn, 
				   DWORD dwLenIn, 
				   PBYTE pBufOut, 
				   DWORD dwLenOut, 
				   PDWORD pdwActualOut)
{	
	int i;
	switch(dwCode)
	{
	case IOCTL_PCF8591_WRITE:
		//GPIOSetBit(g_gpioH,GPIO_DIR);
		//IOWRITE(*(BYTE*)pBufIn,(*(BYTE*)(pBufOut+1)<<8)|*(BYTE*)(pBufOut));	
		WriteReg(*(UINT8 *)pBufIn,(BYTE *)pBufOut,dwLenOut);
		//RETAILMSG(1,(TEXT("Driver Write =\r\n")));
		for(i=0;i<dwLenOut;i++)
		//RETAILMSG(1,(TEXT("%x\r\n"),*(BYTE*)(pBufOut+i)));
		break;
	case IOCTL_PCF8591_READ:
		if(pBufOut && dwLenOut>=sizeof(BYTE))
		{
			//USHORT outValue;
			//GPIOClrBit(g_gpioH,GPIO_DIR);
			//*(USHORT*)pBufOut = IOREAD(*(BYTE*)pBufIn);
			//RETAILMSG(0,(TEXT("Read =\r\n")));
			ReadReg(*(UINT8 *)pBufIn,(BYTE*)pBufOut,dwLenOut);
			//RETAILMSG(1,(TEXT("Read %x="),*(BYTE*)pBufIn));
			for(i=0;i<dwLenOut;i++)
			//RETAILMSG(0,(TEXT("%x\r\n"),*(BYTE*)(pBufOut+i)));
			if(pdwActualOut) *(int*)pdwActualOut=sizeof(BYTE)*dwLenOut;
		}

		break;
	default:
		break;		
	}
	return TRUE;
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DWORD PCF_Open(DWORD hDeviceContext, DWORD AccessCode, DWORD ShareMode)
{
	//RETAILMSG(1,(TEXT("USERMUL: PCF_Open\r\n")));
	return TRUE;
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL PCF_Close(DWORD hOpenContext)
{
	return TRUE;
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void PCF_PowerDown(DWORD hDeviceContext)
{
	//RETAILMSG(1,(TEXT("USERMUL: PCF_PowerDown\r\n")));
	
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void PCF_PowerUp(DWORD hDeviceContext)
{
	//RETAILMSG(1,(TEXT("USERMUL: PCF_PowerUp\r\n")));

} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DWORD PCF_Read(DWORD hOpenContext, LPVOID pBuffer, DWORD Count)
{
	//RETAILMSG(1,(TEXT("USERMUL: PCF_Read\r\n")));
	return TRUE;
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DWORD PCF_Seek(DWORD hOpenContext, long Amount, DWORD Type)
{
	//RETAILMSG(1,(TEXT("USERMUL: PCF_Seek\r\n")));
	return 0;
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DWORD PCF_Write(DWORD hOpenContext, LPCVOID pSourceBytes, DWORD NumberOfBytes)
{
	//RETAILMSG(1,(TEXT("USERMUL: PCF_Write\r\n")));
	return 0;
}

