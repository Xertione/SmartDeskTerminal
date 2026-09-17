/***
	************************************************************************************************
	*	@file  	lcd_3.5tft.c
	*	@version V1.4
	*	@author  鹿小班科技
	*	@brief   3.5寸液晶屏驱动函数，控制器ILI9488，使用FSMC接口驱动
   ************************************************************************************************
   *  @description
	*
	*	实验平台：鹿小班STM32F407VET6核心板 （型号：LXB407VE-P1）
	*	客服微信：19949278543
	*
	*
>>>>	V1.4版本变更说明（2023-5-16）：	
	*		
	*		1.解决标准库例程高优化编译，初始化屏幕后卡死的问题
	*		2.优化复位时间，降低等待时间
	*		3.增加单色图片显示函数
	*		
>>>>	V1.3版本变更说明：	
	*
	*		增加汉字小字库，增加汉字显示的函数
	*		
>>>>	V1.2版本变更说明：	
	*
	*		将初始化时LCD的复位时间加长，由于屏幕的批次不一样，部分屏幕复位时间太短的话无法正常显示
	*
>>>>	V1.1版本变更说明：
	*	
	*		1.修复 LCD_DrawPoint 函数画点颜色不正确的BUG
	*	
>>>>	重要说明：
	*
	*	1. 屏幕分辨率仅为320*240		
	*	2. 取模方式为（以PCtolCD2002为例）：阴码、逆向、逐行式、C51格式
	*	3. 字库存储在 lcd_fonts.c 文件，相关声明则在 lcd_fonts.h文件
	*		
	****************************************************************************************************
***/

#include "lcd_3.5tft.h"


static pFONT *LCD_AsciiFonts;		// 英文字体，ASCII字符集
static pFONT *LCD_CHFonts;		   // 中文字体（同时也包含英文字体）


//LCD相关参数结构体
struct	
{
	uint16_t ID;        //控制器ID	
	uint16_t Width;		// 水平分辨率
	uint16_t Height;		// 垂直分辨率
	uint32_t Color;  		//	LCD当前画笔颜色
	uint32_t BackColor;		//	背景色
	uint8_t  Direction;		//	显示方向
	uint8_t  ShowNum_Mode;	// 数字填充模式
}LCD;

/*****************************************************************************************
*	函 数 名: LCD_Delay
*	入口参数: a - 延时时间，单位ms
*	返 回 值: 无
*	函数功能: 简单延时函数，实现ms级的延时
*	说    明: 为了移植的简便性且对延时精度要求不高，所以不需要使用定时器做延时
******************************************************************************************/

void LCD_Delay(volatile uint16_t a)
{
	volatile uint16_t i;
	while(a--)
	{
		for ( i = 0; i < 15000; i++);
	}
}

/*****************************************************************************************
*	函 数 名: LCD_WR_REG
*	入口参数: reg - 要操作的寄存器
*	返 回 值: 无
*	函数功能: 选择要操作的寄存器
*	说    明: 
******************************************************************************************/

void LCD_WR_REG(uint16_t reg)
{   
	*(__IO uint16_t*) FSMC_REG = reg;	 
}

/*****************************************************************************************
*	函 数 名: LCD_WR_DATA
*	入口参数: data - 要写入的数据
*	返 回 值: 无
*	函数功能: 往相应的寄存器写入数据
*	说    明: 
******************************************************************************************/

void LCD_WR_DATA(uint16_t data)
{	 
	*(__IO uint16_t*) FSMC_DATA = data;	
}
		
/*****************************************************************************************
*	函 数 名: LCD_RD_DATA
*	入口参数: reg - 要读取的寄存器
*	返 回 值: 从该寄存器获取到的数值
*	函数功能: 从相应的寄存器读出数据
*	说    明: 无
******************************************************************************************/
uint16_t LCD_RD_DATA(void)
{
   uint16_t data;	
	data = *(__IO uint16_t*) FSMC_DATA;	
	return data;	 
}

/*****************************************************************************************
*	函 数 名: LCD_RD_OnePoint
*	入口参数: 要读取点的坐标
*	返 回 值: 读取到点的坐标值,超出范围返回0
*	函数功能: 读取一个像素点的RGB值(RGB565格式)
*	说    明: 发送读GRAM指令后，先进行一次假读，之后ILI9488会返回两个16位的值，前一个是RG的值，
             各8位，后面一个是B的值，占高8位。
******************************************************************************************/
uint16_t LCD_RD_OnePoint(uint16_t x,uint16_t y)
{
   uint16_t R_Value = 0, G_Value = 0, B_Value = 0, RGB_Value = 0;
	
	if(x >= LCD.Width||y >= LCD.Height)return 0;
	
   LCD_SetCursor(x,y);                       //要读取点的坐标
	LCD_WR_REG(0X2E);                         //读GRAM指令
	R_Value = LCD_RD_DATA();                  //假读
	R_Value = LCD_RD_DATA();                  //获取RG的值
	G_Value = (R_Value&0X00FC)<<3;            //得到G的值
	R_Value = R_Value&0XF800;                 //得到R的值
	B_Value = (LCD_RD_DATA()&0XF800)>>11;     //获取并得到B的值
	
	RGB_Value = R_Value | G_Value | B_Value;  //合成完整的RGB值
	return RGB_Value;
}

/*****************************************************************************************
*	函 数 名: LCD_GPIO_Config
*	入口参数: 无
*	返 回 值: 无
*	函数功能: 初始化LCD的GPIO
*	说    明: 屏幕的RS线连到FSMC的A16
******************************************************************************************/

void LCD_GPIO_Config(void)
{ 					
 	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(LCD_CLK,ENABLE);//使能GPIO的复用功能时钟
	
 	/*--------------------- ---初始化FSMC引脚-----------------------------------*/ 
	
   GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;//复用输出
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//100MHz
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;//上拉

	GPIO_InitStructure.GPIO_Pin = LCD_CS_PIN;				 
 	GPIO_Init(LCD_CS_PORT, &GPIO_InitStructure); 
   GPIO_PinAFConfig(LCD_CS_PORT,LCD_CS_PINS,GPIO_AF_FSMC);

	GPIO_InitStructure.GPIO_Pin = LCD_RS_PIN;				 
 	GPIO_Init(LCD_RS_PORT, &GPIO_InitStructure); 
   GPIO_PinAFConfig(LCD_RS_PORT,LCD_RS_PINS,GPIO_AF_FSMC);

	GPIO_InitStructure.GPIO_Pin = LCD_WR_PIN;				 
 	GPIO_Init(LCD_WR_PORT, &GPIO_InitStructure); 
   GPIO_PinAFConfig(LCD_WR_PORT,LCD_WR_PINS,GPIO_AF_FSMC);

	GPIO_InitStructure.GPIO_Pin = LCD_RD_PIN;				 
 	GPIO_Init(LCD_RD_PORT, &GPIO_InitStructure); 
   GPIO_PinAFConfig(LCD_RD_PORT,LCD_RD_PINS,GPIO_AF_FSMC);

	GPIO_InitStructure.GPIO_Pin = LCD_D0_PIN;				 
 	GPIO_Init(LCD_D0_PORT, &GPIO_InitStructure); 
   GPIO_PinAFConfig(LCD_D0_PORT,LCD_D0_PINS,GPIO_AF_FSMC);

	GPIO_InitStructure.GPIO_Pin = LCD_D1_PIN;				 
 	GPIO_Init(LCD_D1_PORT, &GPIO_InitStructure); 
   GPIO_PinAFConfig(LCD_D1_PORT,LCD_D1_PINS,GPIO_AF_FSMC);

	GPIO_InitStructure.GPIO_Pin = LCD_D2_PIN;				 
 	GPIO_Init(LCD_D2_PORT, &GPIO_InitStructure); 
   GPIO_PinAFConfig(LCD_D2_PORT,LCD_D2_PINS,GPIO_AF_FSMC);

	GPIO_InitStructure.GPIO_Pin = LCD_D3_PIN;				 
 	GPIO_Init(LCD_D3_PORT, &GPIO_InitStructure); 
   GPIO_PinAFConfig(LCD_D3_PORT,LCD_D3_PINS,GPIO_AF_FSMC);

	GPIO_InitStructure.GPIO_Pin = LCD_D4_PIN;				 
 	GPIO_Init(LCD_D4_PORT, &GPIO_InitStructure); 
   GPIO_PinAFConfig(LCD_D4_PORT,LCD_D4_PINS,GPIO_AF_FSMC);

	GPIO_InitStructure.GPIO_Pin = LCD_D5_PIN;				 
 	GPIO_Init(LCD_D5_PORT, &GPIO_InitStructure); 
   GPIO_PinAFConfig(LCD_D5_PORT,LCD_D5_PINS,GPIO_AF_FSMC);

	GPIO_InitStructure.GPIO_Pin = LCD_D6_PIN;				 
 	GPIO_Init(LCD_D6_PORT, &GPIO_InitStructure); 
   GPIO_PinAFConfig(LCD_D6_PORT,LCD_D6_PINS,GPIO_AF_FSMC);

	GPIO_InitStructure.GPIO_Pin = LCD_D7_PIN;				 
 	GPIO_Init(LCD_D7_PORT, &GPIO_InitStructure); 
   GPIO_PinAFConfig(LCD_D7_PORT,LCD_D7_PINS,GPIO_AF_FSMC);

	GPIO_InitStructure.GPIO_Pin = LCD_D8_PIN;				 
 	GPIO_Init(LCD_D8_PORT, &GPIO_InitStructure); 
   GPIO_PinAFConfig(LCD_D8_PORT,LCD_D8_PINS,GPIO_AF_FSMC);

	GPIO_InitStructure.GPIO_Pin = LCD_D9_PIN;				 
 	GPIO_Init(LCD_D9_PORT, &GPIO_InitStructure); 
   GPIO_PinAFConfig(LCD_D9_PORT,LCD_D9_PINS,GPIO_AF_FSMC);
	
	GPIO_InitStructure.GPIO_Pin = LCD_D10_PIN;				 
 	GPIO_Init(LCD_D10_PORT, &GPIO_InitStructure); 
   GPIO_PinAFConfig(LCD_D10_PORT,LCD_D10_PINS,GPIO_AF_FSMC);

	GPIO_InitStructure.GPIO_Pin = LCD_D11_PIN;				 
 	GPIO_Init(LCD_D11_PORT, &GPIO_InitStructure); 
	GPIO_PinAFConfig(LCD_D11_PORT,LCD_D11_PINS,GPIO_AF_FSMC);
	
	GPIO_InitStructure.GPIO_Pin = LCD_D12_PIN;				 
 	GPIO_Init(LCD_D12_PORT, &GPIO_InitStructure); 
   GPIO_PinAFConfig(LCD_D12_PORT,LCD_D12_PINS,GPIO_AF_FSMC);

	GPIO_InitStructure.GPIO_Pin = LCD_D13_PIN;				 
 	GPIO_Init(LCD_D13_PORT, &GPIO_InitStructure); 
   GPIO_PinAFConfig(LCD_D13_PORT,LCD_D13_PINS,GPIO_AF_FSMC);

	GPIO_InitStructure.GPIO_Pin = LCD_D14_PIN;				 
 	GPIO_Init(LCD_D14_PORT, &GPIO_InitStructure); 
   GPIO_PinAFConfig(LCD_D14_PORT,LCD_D14_PINS,GPIO_AF_FSMC);

	GPIO_InitStructure.GPIO_Pin = LCD_D15_PIN;				 
 	GPIO_Init(LCD_D15_PORT, &GPIO_InitStructure); 
   GPIO_PinAFConfig(LCD_D15_PORT,LCD_D15_PINS,GPIO_AF_FSMC);
	
 	/*-------------------------初始化其他控制引脚--------------------------------*/ 

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;//普通输出模式
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;//2MHz
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;//上拉
	
	//初始化复位引脚
 	GPIO_InitStructure.GPIO_Pin = LCD_RST_PIN;				
 	GPIO_Init(LCD_RST_PORT, &GPIO_InitStructure);	
	
	//初始化背光引脚
 	GPIO_InitStructure.GPIO_Pin = LCD_BL_PIN;			 
 	GPIO_Init(LCD_BL_PORT, &GPIO_InitStructure);		

	GPIO_ResetBits(LCD_BL_PORT,LCD_BL_PIN);  //关闭背光
}  

/*****************************************************************************************
*	函 数 名: LCD_FSMC_Config
*	入口参数: 无
*	返 回 值: 无
*	函数功能: 初始化FSMC配置
*	说    明: 使用BANK1
******************************************************************************************/

void	LCD_FSMC_Config(void)
{ 
	FSMC_NORSRAMTimingInitTypeDef  SET;	
	FSMC_NORSRAMInitTypeDef  FSMC_NORSRAMInitStructure;	
	
   RCC_AHB3PeriphClockCmd(RCC_AHB3Periph_FSMC,ENABLE);	//使能FSMC时钟
	
	SET.FSMC_AddressSetupTime = 0x00;	 		//地址建立时间
	SET.FSMC_AddressHoldTime = 0x00;		 		//地址保持时间
	SET.FSMC_DataSetupTime = 0x03;		 		//数据保存时间
	SET.FSMC_BusTurnAroundDuration = 0x00;
	SET.FSMC_CLKDivision = 0x4;
	SET.FSMC_DataLatency = 0x00;
	SET.FSMC_AccessMode = FSMC_AccessMode_A;	 //模式A 

	FSMC_NORSRAMInitStructure.FSMC_Bank 				  = FSMC_Bank1_NORSRAM1;			
	FSMC_NORSRAMInitStructure.FSMC_DataAddressMux 	  = FSMC_DataAddressMux_Disable; 
	FSMC_NORSRAMInitStructure.FSMC_MemoryType 		  = FSMC_MemoryType_SRAM;			
	FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth 	  = FSMC_MemoryDataWidth_16b;		
	FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode 	  = FSMC_BurstAccessMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
	FSMC_NORSRAMInitStructure.FSMC_AsynchronousWait	  = FSMC_AsynchronousWait_Disable; 
	FSMC_NORSRAMInitStructure.FSMC_WrapMode 			  = FSMC_WrapMode_Disable;   
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive   = FSMC_WaitSignalActive_BeforeWaitState;  
	FSMC_NORSRAMInitStructure.FSMC_WriteOperation 	  = FSMC_WriteOperation_Enable;					
	FSMC_NORSRAMInitStructure.FSMC_WaitSignal 		  = FSMC_WaitSignal_Disable;   
	FSMC_NORSRAMInitStructure.FSMC_ExtendedMode		  = FSMC_ExtendedMode_Enable; 	
	FSMC_NORSRAMInitStructure.FSMC_WriteBurst 		  = FSMC_WriteBurst_Disable; 
	FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct  = &SET;  
	FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct = &SET; 

	FSMC_NORSRAMInit(&FSMC_NORSRAMInitStructure);  //初始化FSMC
	FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM1, ENABLE);  // 使能BANK1  
	

}
	
/*****************************************************************************************
*	函 数 名: LCD_Init
*	入口参数: 无
*	返 回 值: 无
*	函数功能: 初始化LCD的配置
*	说    明: 主要是操作ILI9488的相关寄存器，进行必要的设置，默认竖屏显示
******************************************************************************************/

void LCD_Init(void)
{
	LCD_GPIO_Config();	// 初始化GPIO
	LCD_FSMC_Config();	// 初始化FSMC配置
	
  // 复位ILI9488
	GPIO_ResetBits(LCD_RST_PORT,LCD_RST_PIN);	LCD_Delay(10); 	
	GPIO_SetBits(LCD_RST_PORT,LCD_RST_PIN);	LCD_Delay(10);

	//读取LCD控制器的ID号
	LCD_WR_REG(0XD3);	
   LCD.ID=LCD_RD_DATA();     //假读
	LCD.ID=LCD_RD_DATA();     //读0x00
	LCD.ID=LCD_RD_DATA();     //读0x94
	LCD.ID<<=8;               //0x94移入高8位
	LCD.ID|=LCD_RD_DATA();    //读0x88并获取完整ID号
	printf(" LCD ID:ILI%x\r\n",LCD.ID);
	
	
   // 初始化ILI9341
	LCD_WR_REG(0xE0); //P-Gamma
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x03);
	LCD_WR_DATA(0x0C);
	LCD_WR_DATA(0x09);
	LCD_WR_DATA(0x17);
	LCD_WR_DATA(0x09);
	LCD_WR_DATA(0x3E);
	LCD_WR_DATA(0x89);
	LCD_WR_DATA(0x49);
	LCD_WR_DATA(0x08);
	LCD_WR_DATA(0x0D);
	LCD_WR_DATA(0x0A);
	LCD_WR_DATA(0x13);
	LCD_WR_DATA(0x15);
	LCD_WR_DATA(0x0F);
	LCD_WR_REG(0XE1); //N-Gamma
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x11);
	LCD_WR_DATA(0x15);
	LCD_WR_DATA(0x03);
	LCD_WR_DATA(0x0F);
	LCD_WR_DATA(0x05);
	LCD_WR_DATA(0x2D);
	LCD_WR_DATA(0x34);
	LCD_WR_DATA(0x41);
	LCD_WR_DATA(0x02);
	LCD_WR_DATA(0x0B);
	LCD_WR_DATA(0x0A);
	LCD_WR_DATA(0x33);
	LCD_WR_DATA(0x37);
	LCD_WR_DATA(0x0F);

	LCD_WR_REG(0XC0); //Power Control 1
	LCD_WR_DATA(0x17); //Vreg1out
	LCD_WR_DATA(0x15); //Verg2out
	LCD_WR_REG(0xC1); //Power Control 2
	LCD_WR_DATA(0x41); //VGH,VGL
	LCD_WR_REG(0xC5); //Power Control 3
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x12); //Vcom
	LCD_WR_DATA(0x80);

	LCD_WR_REG(0x36); //Memory Access
	LCD_WR_DATA(0x48);
	LCD_WR_REG(0x3A); // Interface Pixel Format
	LCD_WR_DATA(0x55);
	LCD_WR_REG(0XB0); // Interface Mode Control
	LCD_WR_DATA(0x00);
	LCD_WR_REG(0xB1); //Frame rate
	LCD_WR_DATA(0xA0); //60Hz
	LCD_WR_REG(0xB4); //Display Inversion Control
	LCD_WR_DATA(0x02); //2-dot
	LCD_WR_REG(0XB6); //RGB/MCU Interface Control
	LCD_WR_DATA(0x02); //MCU
	LCD_WR_DATA(0x02); //Source,Gate scan dieection
	LCD_WR_REG(0XE9); // Set Image Function
	LCD_WR_DATA(0x00); // Disable 24 bit data input
	LCD_WR_REG(0xF7);// Adjust Control
	LCD_WR_DATA(0xA9);
	LCD_WR_DATA(0x51);
	LCD_WR_DATA(0x2C);
	LCD_WR_DATA(0x82);
	// D7 stream, loose
	LCD_WR_REG(0x11); //Sleep out
	LCD_Delay(150);
	LCD_WR_REG(0x29); //Display on
	
	LCD_SetBackColor(LCD_BLACK); 	// 设置背景色
	LCD_SetColor(LCD_WHITE);		// 设置画笔颜色
	LCD_SetAsciiFont(&ASCII_Font24);  			// 设置默认字体
	LCD_ShowNumMode(Fill_Space);	// 设置数字填充模式
	LCD_DisplayMode(Mode_V);		// 设置显示方向，竖屏
	LCD_Clear();		// 清屏
		
	GPIO_SetBits(LCD_BL_PORT,LCD_BL_PIN);			//开启LCD背光	
	
}

/*****************************************************************************************
*	函 数 名:	 LCD_SetCursor
*	入口参数:	 x - 水平坐标
*			  	 y - 垂直坐标
*	返 回 值: 无
*	函数功能: 设置屏幕要显示数据的坐标
*	说    明: 0x2A 为水平坐标寄存器地址，0x2B为垂直坐标寄存器地址
******************************************************************************************/

void LCD_SetCursor(uint16_t x, uint16_t y)
{	   
   //ILI9488设置坐标时必须把END坐标也写进去。因为END坐标通常不确定，这里直接写最大坐标
	LCD_WR_REG(0x2A); 							
	LCD_WR_DATA(x>>8);LCD_WR_DATA(x&0XFF); 	//设置水平坐标，高位在前	
	LCD_WR_DATA(0x01);   // 设置最大X坐标为480
	LCD_WR_DATA(0xDF); 	
	LCD_WR_REG(0x2B); 						
	LCD_WR_DATA(y>>8);LCD_WR_DATA(y&0XFF); 	//设置垂直坐标，高位在前	
	LCD_WR_DATA(0x01);   // 设置最大Y坐标为480
	LCD_WR_DATA(0xDF); 
} 

/*****************************************************************************************
*	函 数 名:	 LCD_SetColor
*	入口参数:	 Color - 要设置的颜色
*	返 回 值: 无
*	函数功能: 设置画笔的颜色，用于显示字符、绘制图形等
*	说    明: 虽然鹿小班3.5寸的屏幕使用的颜色格式是16位的RGB565，但是这里为了方便用户使用，
*				 入口参数使用的是24位的RGB888颜色，然后再通过代码转换成16位色。用户可以很方便
*				 的在电脑用取色器获取24位的颜色，再将此24位颜色值输入该函数即可。		 
******************************************************************************************/

void LCD_SetColor(uint32_t Color)
{
	uint16_t Red_Value = 0, Green_Value = 0, Blue_Value = 0; //各个颜色通道的值
	
	Red_Value   = (uint16_t)((Color&0xF80000)>>8);		// 提取红色的值
	Green_Value = (uint16_t)((Color&0x00FC00)>>5);		// 提取绿色的值
	Blue_Value  = (uint16_t)((Color&0x0000F8)>>3);		// 提取蓝色的值
	
	LCD.Color = (uint16_t)(Red_Value | Green_Value | Blue_Value);		// 合成16位色，颜色格式RGB565
}

/*****************************************************************************************
*	函 数 名:	 LCD_SetBackColor
*	入口参数:	 Color - 要设置的颜色
*	返 回 值: 无
*	函数功能: 用于清屏或字体的显示背景颜色
*	说    明: 虽然鹿小班3.5寸的屏幕使用的颜色格式是16位的RGB565，但是这里为了方便用户使用，
*				 入口参数使用的是24位的RGB888颜色，然后再通过代码转换成16位色。用户可以很方便
*				 的在电脑用取色器获取24位的颜色，再将此24位颜色值输入该函数即可。		 
******************************************************************************************/

void LCD_SetBackColor(uint32_t Color)
{
	uint16_t Red_Value = 0, Green_Value = 0, Blue_Value = 0; //各个颜色通道的值
	
	Red_Value   = (uint16_t)((Color&0xF80000)>>8);	// 提取红色的值
	Green_Value = (uint16_t)((Color&0x00FC00)>>5); // 提取绿色的值
	Blue_Value  = (uint16_t)((Color&0x0000F8)>>3); // 提取蓝色的值
	
	LCD.BackColor = (uint16_t)(Red_Value | Green_Value | Blue_Value);	// 合成16位色，颜色格式RGB565
}

/*****************************************************************************************
*	函 数 名:	 LCD_DisplayMode
*	入口参数:	 direction - 横屏或竖屏显示，可选参数 Mode_H 横屏，Mode_V 竖直
*	返 回 值: 无
*	函数功能: 设置液晶屏的显示方向，横屏或竖屏
*	说    明: 无	 
******************************************************************************************/

void LCD_DisplayMode(uint8_t direction)
{
	LCD.Direction = direction;		// 修改全局变量，方向标志位
	
	if (LCD.Direction == Mode_H) 		// 横屏显示
	{
		LCD_WR_REG(0X36);		// 扫描方向设置寄存器
		LCD_WR_DATA(0X28); 	//	扫描方向，从上到下	，从右到左
		
		LCD_WR_REG(0x2A); 	
		LCD_WR_DATA(0x00);	// 设置x坐标（在这里没有作用，只是需要先写入而已）
		LCD_WR_DATA(0x00); 			
		LCD_WR_DATA(0x01);	// 设置最大x坐标为320
		LCD_WR_DATA(0XDF); 
			
		LCD_WR_REG(0x2B); 
		LCD_WR_DATA(0x00);	// 设置y坐标（在这里没有作用，只是需要先写入而已）
		LCD_WR_DATA(0x00); 		
		LCD_WR_DATA(0x01);	// 设置最大y坐标为240
		LCD_WR_DATA(0x3F); 		
		
		LCD.Width  = 480;		// 水平宽度320
		LCD.Height = 320;		// 垂直宽度240
	}
	else if(LCD.Direction == Mode_V)	// 竖屏显示
	{
		LCD_WR_REG(0X36);		// 扫描方向设置寄存器
		LCD_WR_DATA(0X48); 	//	扫描方向，从左到右，从上到下	
		
		LCD_WR_REG(0x2A); 	
		LCD_WR_DATA(0x00);	// 设置x坐标（在这里没有作用，只是需要先写入而已）
		LCD_WR_DATA(0x00); 			
		LCD_WR_DATA(0x01);   // 设置最大x坐标为240
		LCD_WR_DATA(0x3F); 				
                           
		LCD_WR_REG(0x2B);    
		LCD_WR_DATA(0x00);   // 设置y坐标（在这里没有作用，只是需要先写入而已）
		LCD_WR_DATA(0x00); 			
		LCD_WR_DATA(0x01);   // 设置最大y坐标为320
		LCD_WR_DATA(0XDF); 			
		                     
		LCD.Width  = 320;    // 水平宽度240
		LCD.Height = 480;		// 垂直宽度320
	}
}

/*****************************************************************************************
*	函 数 名:	 LCD_Clear
*	入口参数: 无
*	返 回 值: 无
*	函数功能: 清除整个屏幕的显示
*	说    明: 清除的颜色为背景色，用LCD_SetBackColor设置
******************************************************************************************/

void LCD_Clear(void)
{ 
	uint32_t i = 0;
	
	LCD_SetCursor(0,0);	// 设置起始坐标
	LCD_WR_REG(0X2C);		// 开始写入GRAM

	for(i=0;i<320*480;i++)	// 总共需要写入320*240个点，ILI9341会根据屏幕的分辨率自动换行显示
	{
		*(__IO uint16_t*) FSMC_DATA = LCD.BackColor;	// 写入背景色
	}
}

/*****************************************************************************************
*	函 数 名:	 LCD_DrawPoint
*	入口参数:	 x - 水平坐标
*			  	 y - 垂直坐标
*				 color - 要显示的颜色
*	返 回 值: 无
*	函数功能: 在制定坐标处绘制指定颜色的点
*	说    明: 无
******************************************************************************************/

void LCD_DrawPoint(uint16_t x,uint16_t y)
{
	LCD_SetCursor(x,y);	// 设置坐标
	LCD_WR_REG(0X2C);		// 写入GRAM
	*(__IO uint16_t*) FSMC_DATA = LCD.Color; 	// 写入颜色
}

/****************************************************************************************************************************************
*	函 数 名:	LCD_SetAsciiFont
*
*	入口参数:	*fonts - 要设置的ASCII字体
*
*	函数功能:	设置ASCII字体，可选择使用 3216/2412/2010/1608/1206 五种大小的字体
*
*	说    明:	1. 使用示例 LCD_SetAsciiFont(&ASCII_Font24) ，即设置 2412的 ASCII字体
*					2. 相关字模存放在 lcd_fonts.c 			
*
*****************************************************************************************************************************************/

void LCD_SetAsciiFont(pFONT *Asciifonts)
{
  LCD_AsciiFonts = Asciifonts;
}


/*****************************************************************************************
*	函 数 名:	 LCD_DisplayChar
*	入口参数:	 x - 水平坐标
*			  	 y - 垂直坐标
*				 c - 字符的ASCII值
*	返 回 值: 无
*	函数功能: 在指定坐标处显示单个ASCII字符
*	说    明: 可通过 LCD_SetFont() 函数设置相应的字体
******************************************************************************************/

void LCD_DisplayChar(uint16_t x, uint16_t y,uint8_t add)
{
	uint16_t  index = 0, counter = 0;
   uint8_t   disChar;	//字模的值
	uint16_t  Xaddress = x; //水平坐标
	
	add = add - 32; 
	for(index = 0; index < LCD_AsciiFonts->Sizes; index++)
	{
		disChar = LCD_AsciiFonts->pTable[add*LCD_AsciiFonts->Sizes + index]; //获取字符的模值
		for(counter = 0; counter < 8; counter++)
		{ 
			if(disChar & 0x01)	
			{				
				LCD_SetCursor(Xaddress,y);	// 设置坐标
				LCD_WR_REG(0X2C);		// 写入GRAM
				*(__IO uint16_t*) FSMC_DATA = LCD.Color; 	//当前模值不为0时，使用画笔色绘点
			}
			else		
			{		
				LCD_SetCursor(Xaddress,y);	// 设置坐标
				LCD_WR_REG(0X2C);		// 写入GRAM
				*(__IO uint16_t*) FSMC_DATA = LCD.BackColor; 	//否则使用背景色绘制点
			}
			disChar >>= 1;
			Xaddress++;  //水平坐标自加
			
			if( (Xaddress - x)==LCD_AsciiFonts->Width ) //如果水平坐标达到了字符宽度，则退出当前循环
			{													//进入下一行的绘制
				Xaddress = x;
				y++;
				break;
			}
		}	
	}	
}

/*****************************************************************************************
*	函 数 名:	 LCD_DisplayString
*	入口参数:	 x - 水平坐标
*			  	 y - 垂直坐标
*				 *p - 要显示的字符串
*	返 回 值: 无
*	函数功能: 在指定坐标处显示字符串
*	说    明: 可通过 LCD_SetFont() 函数设置相应的字体
******************************************************************************************/

void LCD_DisplayString( uint16_t x, uint16_t y, char *p) 
{  
	while( (x<LCD.Width)&&(*p != 0) )	//判断显示坐标是否超出显示区域并且字符是否为空字符
	{
		 LCD_DisplayChar( x,y,*p);
		 x += LCD_AsciiFonts->Width; //显示下一个字符
		 p++;	//取下一个字符地址
	}
}

/****************************************************************************************************************************************
*	函 数 名:	LCD_SetTextFont
*
*	入口参数:	*fonts - 要设置的文本字体
*
*	函数功能:	设置文本字体，包括中文和ASCII字符，
*
*	说    明:	1. 可选择使用 3232/2424/2020/1616/1212 五种大小的中文字体，
*						并且对应的设置ASCII字体为 3216/2412/2010/1608/1206
*					2. 相关字模存放在 lcd_fonts.c 
*					3. 中文字库使用的是小字库，即用到了对应的汉字再去取模
*					4. 使用示例 LCD_SetTextFont(&CH_Font24) ，即设置 2424的中文字体以及2412的ASCII字符字体
*
*****************************************************************************************************************************************/

void LCD_SetTextFont(pFONT *fonts)
{
	LCD_CHFonts = fonts;		// 设置中文字体
	switch(fonts->Width )
	{
		case 12:	LCD_AsciiFonts = &ASCII_Font12;	break;	// 设置ASCII字符的字体为 1206
		case 16:	LCD_AsciiFonts = &ASCII_Font16;	break;	// 设置ASCII字符的字体为 1608
		case 20:	LCD_AsciiFonts = &ASCII_Font20;	break;	// 设置ASCII字符的字体为 2010	
		case 24:	LCD_AsciiFonts = &ASCII_Font24;	break;	// 设置ASCII字符的字体为 2412
		case 32:	LCD_AsciiFonts = &ASCII_Font32;	break;	// 设置ASCII字符的字体为 3216		
		default: break;
	}
}

/***************************************************************************************************************
*	函 数 名:	LCD_DisplayChinese
*
*	入口参数:		x - 起始水平坐标
*					y - 起始垂直坐标
*					pText - 中文字符
*
*	函数功能:	在指定坐标显示指定的单个中文字符
*
*	说    明:	1. 可设置要显示的字体，例如使用 LCD_SetTextFont(&CH_Font24) 设置为 2424的中文字体以及2412的ASCII字符字体
*					2.	可设置要显示的颜色，例如使用 LCD_SetColor(128) 设置为查色表第128号颜色
*					3. 可设置对应的背景色，例如使用 LCD_SetBackColor(128) 设置为查色表第128号颜色
*					4. 使用示例 LCD_DisplayChinese( 10, 10, "反") ，在坐标(10,10)显示中文字符"反"
*
***************************************************************************************************************/

void LCD_DisplayChinese(uint16_t x, uint16_t y, char *pText) 
{
	uint16_t  i=0,index = 0, counter = 0;	// 计数变量
	uint16_t  addr;	// 字模地址
   uint8_t   disChar;	//字模的值
	uint16_t  Xaddress = x; //水平坐标

	while(1)
	{		
		// 对比数组中的汉字编码，用以定位该汉字字模的地址		
		if ( *(LCD_CHFonts->pTable + (i+1)*LCD_CHFonts->Sizes + 0)==*pText && *(LCD_CHFonts->pTable + (i+1)*LCD_CHFonts->Sizes + 1)==*(pText+1) )	
		{   
			addr=i;	// 字模地址偏移
			break;
		}				
		i+=2;	// 每个中文字符编码占两字节

		if(i >= LCD_CHFonts->Table_Rows)	break;	// 字模列表中无相应的汉字	
	}	
	

	for(index = 0; index <LCD_CHFonts->Sizes; index++)
	{	
		disChar = *(LCD_CHFonts->pTable + (addr)*LCD_CHFonts->Sizes + index);	// 获取相应的字模地址
		
		for(counter = 0; counter < 8; counter++)
		{ 
			if(disChar & 0x01)	
			{		
				LCD_SetCursor(Xaddress,y);	// 设置坐标
				LCD_WR_REG(0X2C);		// 写入GRAM
				*(__IO uint16_t*) FSMC_DATA = LCD.Color; 	//当前模值不为0时，使用画笔色绘点
			}
			else		
			{		
				LCD_SetCursor(Xaddress,y);	// 设置坐标
				LCD_WR_REG(0X2C);		// 写入GRAM
				*(__IO uint16_t*) FSMC_DATA = LCD.BackColor; 	//否则使用背景色绘制点
			}
			disChar >>= 1;
			Xaddress++;  //水平坐标自加
			
			if( (Xaddress - x)==LCD_CHFonts->Width ) 	//	如果水平坐标达到了字符宽度，则退出当前循环
			{														//	进入下一行的绘制
				Xaddress = x;
				y++;
				break;
			}
		}	
	}	
}

/***************************************************************************************************************
*	函 数 名:	LCD_DisplayText
*
*	入口参数:	x - 起始水平坐标，取值范围0~799
*					y - 起始垂直坐标，取值范围0~479
*					pText - 字符串，可以显示中文或者ASCII字符
*
*	函数功能:	在指定坐标显示指定的字符串
*
*	说    明:	1. 可设置要显示的字体，例如使用 LCD_SetTextFont(&CH_Font24) 设置为 2424的中文字体以及2412的ASCII字符字体
*					2.	可设置要显示的颜色，例如使用 LCD_SetColor(128) 设置为查色表第128号颜色
*					3. 可设置对应的背景色，例如使用 LCD_SetBackColor(128) 设置为查色表第128号颜色
*					4. 使用示例 LCD_DisplayChinese( 10, 10, "鹿小班科技STM32") ，在坐标(10,10)显示字符串"鹿小班科技STM32"
*
***************************************************************************************************************/

void LCD_DisplayText(uint16_t x, uint16_t y, char *pText) 
{  
 	
	while(*pText != 0)	// 判断是否为空字符
	{
		if(*pText<=0x7F)	// 判断是否为ASCII码
		{
			LCD_DisplayChar(x,y,*pText);	// 显示ASCII
			x+=LCD_AsciiFonts->Width;				// 水平坐标调到下一个字符处
			pText++;								// 字符串地址+1
		}
		else					// 若字符为汉字
		{			
			LCD_DisplayChinese(x,y,pText);	// 显示汉字
			x+=LCD_CHFonts->Width;				// 水平坐标调到下一个字符处
			pText+=2;								// 字符串地址+2，汉字的编码要2字节
		}
	}	
}

/*****************************************************************************************
*	函 数 名:	 GetNumber
*	入口参数:	 number - 目标整数
*			  	 size - 目标数的长度
*	返 回 值: 取整数的第N位的数，用于拆分整数以进行显示
*	函数功能: 取出的第N位的数
*	说    明: 在 LCD_DisplayNumber()里被调用
******************************************************************************************/

uint8_t GetNumber(uint32_t num,uint8_t size)
{
	uint32_t numPow = 1;
	uint8_t  vaule;
	
	while(size>0)
	{
		numPow *=10;
		size--;
	}
	vaule = (num/numPow)%10;
	return vaule;	
}

/*****************************************************************************************
*	函 数 名:	 LCD_ShowNumMode
*	入口参数:	 mode - 数字填充模式，Fill_Space 填充空格，Fill_Zero 填充零
*	返 回 值: 无
*	函数功能: 设置数字显示的填充模式
*	说    明: 使用 LCD_DisplayNumber() 显示数字时，调用此函数设置高位的填充模式，
*				 通常在显示时间或日期的时候调用
******************************************************************************************/

void LCD_ShowNumMode(uint8_t mode)
{
	LCD.ShowNum_Mode = mode;
}

/*****************************************************************************************
*	函 数 名:	 LCD_DisplayNumber
*	入口参数:	 x - 水平坐标
*			  	 y - 垂直坐标
*				 number - 要显示的数字
*				 len - 数字的长度
*	返 回 值: 无
*	函数功能: 显示整数变量
*	说    明: 需要指定显示的位数
******************************************************************************************/

void LCD_DisplayNumber( uint16_t x, uint16_t y, uint32_t number, uint8_t len) 
{  
	uint8_t i,value;
	uint8_t zero_Flag = 0;

	for(i=0;i<len;i++)
	{
		value = GetNumber(number,len-i-1);	//获取当前整数的第N位数
		if( zero_Flag==0 && (i<len-1) )	//判断前面为0的部分
		{
			if(value == 0)
			{
				if(LCD.ShowNum_Mode == 0)	
					LCD_DisplayChar( x+i*LCD_AsciiFonts->Width, y,48);	//填充0
				else
					LCD_DisplayChar( x+i*LCD_AsciiFonts->Width, y,32);	//填充空格
				continue;
			}
			else
				zero_Flag = 1;	//当遇到第一个非0数时置1
		}									
		LCD_DisplayChar( x+i*LCD_AsciiFonts->Width, y, value+48 );	//将拆分的数进行显示
	}
}

/*****************************************************************************************
*	函 数 名:	 LCD_DrawLine
*	入口参数:	 x1、y1 - 起点坐标
*			  	 x2、y2 - 终点坐标
*	返 回 值: 无
*	函数功能: 绘制直线
*	说    明: 无
******************************************************************************************/

#define ABS(X)  ((X) > 0 ? (X) : -(X))    

void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	int16_t deltax = 0, deltay = 0, x = 0, y = 0, xinc1 = 0, xinc2 = 0, 
	yinc1 = 0, yinc2 = 0, den = 0, num = 0, numadd = 0, numpixels = 0, 
	curpixel = 0;

	deltax = ABS(x2 - x1);        /* The difference between the x's */
	deltay = ABS(y2 - y1);        /* The difference between the y's */
	x = x1;                       /* Start x off at the first pixel */
	y = y1;                       /* Start y off at the first pixel */

	if (x2 >= x1)                 /* The x-values are increasing */
	{
	 xinc1 = 1;
	 xinc2 = 1;
	}
	else                          /* The x-values are decreasing */
	{
	 xinc1 = -1;
	 xinc2 = -1;
	}

	if (y2 >= y1)                 /* The y-values are increasing */
	{
	 yinc1 = 1;
	 yinc2 = 1;
	}
	else                          /* The y-values are decreasing */
	{
	 yinc1 = -1;
	 yinc2 = -1;
	}

	if (deltax >= deltay)         /* There is at least one x-value for every y-value */
	{
	 xinc1 = 0;                  /* Don't change the x when numerator >= denominator */
	 yinc2 = 0;                  /* Don't change the y for every iteration */
	 den = deltax;
	 num = deltax / 2;
	 numadd = deltay;
	 numpixels = deltax;         /* There are more x-values than y-values */
	}
	else                          /* There is at least one y-value for every x-value */
	{
	 xinc2 = 0;                  /* Don't change the x for every iteration */
	 yinc1 = 0;                  /* Don't change the y when numerator >= denominator */
	 den = deltay;
	 num = deltay / 2;
	 numadd = deltax;
	 numpixels = deltay;         /* There are more y-values than x-values */
	}
	for (curpixel = 0; curpixel <= numpixels; curpixel++)
	{
	 LCD_DrawPoint(x,y);             /* Draw the current pixel */
	 num += numadd;              /* Increase the numerator by the top of the fraction */
	 if (num >= den)             /* Check if numerator >= denominator */
	 {
		num -= den;               /* Calculate the new numerator value */
		x += xinc1;               /* Change the x as appropriate */
		y += yinc1;               /* Change the y as appropriate */
	 }
	 x += xinc2;                 /* Change the x as appropriate */
	 y += yinc2;                 /* Change the y as appropriate */
	}
}

/*****************************************************************************************
*	函 数 名:	 LCD_DrawRect
*	入口参数:	 x、y - 起点坐标
*			  	 width - 矩形长度
*				 height - 矩形宽度
*	返 回 值: 无
*	函数功能: 绘制矩形框
*	说    明: 无
******************************************************************************************/

void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
	/* draw horizontal lines */
	LCD_DrawLine(x, y, x+width, y);
	LCD_DrawLine(x, y+height, x+width, y+height);

	/* draw vertical lines */
	LCD_DrawLine(x, y, x, y+height);
	LCD_DrawLine(x+width, y, x+width, y+height);
}

/*****************************************************************************************
*	函 数 名:	 LCD_DrawCircle
*	入口参数:	 x、y - 起点坐标
*			  	 r - 半径
*	返 回 值: 无
*	函数功能: 绘制圆形框
*	说    明: 半径不能大于xy坐标的值
******************************************************************************************/

void LCD_DrawCircle(uint16_t x, uint16_t y, uint16_t r)
{
	int Xadd = -r, Yadd = 0, err = 2-2*r, e2;
	do {   

		LCD_DrawPoint(x-Xadd,y+Yadd);
		LCD_DrawPoint(x+Xadd,y+Yadd);
		LCD_DrawPoint(x+Xadd,y-Yadd);
		LCD_DrawPoint(x-Xadd,y-Yadd);
		
		e2 = err;
		if (e2 <= Yadd) {
			err += ++Yadd*2+1;
			if (-Xadd == Yadd && e2 <= Xadd) e2 = 0;
		}
		if (e2 > Xadd) err += ++Xadd*2+1;
    }
    while (Xadd <= 0); 
}

/*****************************************************************************************
*	函 数 名:	 LCD_DrawEllipse
*	入口参数:	 x、y - 中心坐标
*			  	 r1、r2 - 分别为水平和垂直半轴的长度
*	返 回 值: 无
*	函数功能: 绘制椭圆框
*	说    明: 无
******************************************************************************************/

void LCD_DrawEllipse(int x, int y, int r1, int r2)
{
  int Xadd = -r1, Yadd = 0, err = 2-2*r1, e2;
  float K = 0, rad1 = 0, rad2 = 0;
   
  rad1 = r1;
  rad2 = r2;
  
  if (r1 > r2)
  { 
    do {
      K = (float)(rad1/rad2);
		 
		LCD_DrawPoint(x-Xadd,y+(uint16_t)(Yadd/K));
		LCD_DrawPoint(x+Xadd,y+(uint16_t)(Yadd/K));
		LCD_DrawPoint(x+Xadd,y-(uint16_t)(Yadd/K));
		LCD_DrawPoint(x-Xadd,y-(uint16_t)(Yadd/K));     
		 
      e2 = err;
      if (e2 <= Yadd) {
        err += ++Yadd*2+1;
        if (-Xadd == Yadd && e2 <= Xadd) e2 = 0;
      }
      if (e2 > Xadd) err += ++Xadd*2+1;
    }
    while (Xadd <= 0);
  }
  else
  {
    Yadd = -r2; 
    Xadd = 0;
    do { 
      K = (float)(rad2/rad1);

		LCD_DrawPoint(x-(uint16_t)(Xadd/K),y+Yadd);
		LCD_DrawPoint(x+(uint16_t)(Xadd/K),y+Yadd);
		LCD_DrawPoint(x+(uint16_t)(Xadd/K),y-Yadd);
		LCD_DrawPoint(x-(uint16_t)(Xadd/K),y-Yadd);  
		 
      e2 = err;
      if (e2 <= Xadd) {
        err += ++Xadd*3+1;
        if (-Yadd == Xadd && e2 <= Yadd) e2 = 0;
      }
      if (e2 > Yadd) err += ++Yadd*3+1;     
    }
    while (Yadd <= 0);
  }
}

/*****************************************************************************************
*	函 数 名:	 LCD_FillRect
*	入口参数:	 x、y - 中心坐标
*				 width - 长度
*				 height - 宽度	
*	返 回 值: 无
*	函数功能: 填充矩形区域
*	说    明: 无
******************************************************************************************/

void LCD_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
	uint16_t i = 0,j = 0;

	for(i=0;i<height;i++)
	{
		LCD_SetCursor(x,y+i);   // 设置光标位置 
		LCD_WR_REG(0X2C);			// 开始写入GRAM
		for(j=0;j<width;j++)
		{
			*(__IO uint16_t*) FSMC_DATA = LCD.Color;	//写入数据 
		}
	}
}

/*****************************************************************************************
*	函 数 名:	 LCD_FillCircle
*	入口参数:	 x、y - 中心坐标
*				 r - 半径
*	返 回 值: 无
*	函数功能: 填充圆形区域
*	说    明: 无
******************************************************************************************/

void LCD_FillCircle(uint16_t x, uint16_t y, uint16_t r)
{
  int32_t  D;    /* Decision Variable */ 
  uint32_t  CurX;/* Current X Value */
  uint32_t  CurY;/* Current Y Value */ 
  
  D = 3 - (r << 1);
  
  CurX = 0;
  CurY = r;
  
  while (CurX <= CurY)
  {
    if(CurY > 0) 
    { 
		LCD_DrawLine(x - CurX, y - CurY,x - CurX,y - CurY + 2*CurY);
		LCD_DrawLine(x + CurX, y - CurY,x + CurX,y - CurY + 2*CurY); 
    }
    
    if(CurX > 0) 
    {
		LCD_DrawLine(x - CurY, y - CurX,x - CurY,y - CurX + 2*CurX);
		LCD_DrawLine(x + CurY, y - CurX,x + CurY,y - CurX + 2*CurX); 		 
    }
    if (D < 0)
    { 
      D += (CurX << 2) + 6;
    }
    else
    {
      D += ((CurX - CurY) << 2) + 10;
      CurY--;
    }
    CurX++;
  }
  
  LCD_DrawCircle(x, y, r);  
}

/***************************************************************************************************************************************
*	函 数 名: LCD_DrawImage
*
*	入口参数: x - 起始水平坐标
*				 y - 起始垂直坐标
*			 	 width  - 图片的水平宽度
*				 height - 图片的垂直宽度
*				*pImage - 图片数据存储区的首地址
*
*	函数功能: 在指定坐标处显示图片
*
*	说    明: 1.要显示的图片需要事先进行取模、获悉图片的长度和宽度
*            2.使用 LCD_SetColor() 函数设置画笔色，LCD_SetBackColor() 设置背景色
*						 
*****************************************************************************************************************************************/

void 	LCD_DrawImage(uint16_t x,uint16_t y,uint16_t width,uint16_t height,const uint8_t *pImage) 
{  
   uint8_t   disChar;	//字模的值
	uint16_t  Xaddress = x; //水平坐标
	uint16_t  i=0,j=0,m=0;
	
	for(i = 0; i <height; i++)
	{
		for(j = 0; j <(float)width/8; j++)
		{
			disChar = *pImage;

			for(m = 0; m < 8; m++)
			{ 
				if(disChar & 0x01)	
				{		
//					LCD_DrawPoint(Xaddress,y);	//当前模值不为0时，使用画笔色绘点
					
					LCD_SetCursor(Xaddress,y);	// 设置坐标
					LCD_WR_REG(0X2C);		// 写入GRAM
					*(__IO uint16_t*) FSMC_DATA = LCD.Color; 	// 写入颜色
				}
				else		
				{		
					LCD_SetCursor(Xaddress,y);	// 设置坐标
					LCD_WR_REG(0X2C);				// 写入GRAM
					*(__IO uint16_t*) FSMC_DATA = LCD.BackColor; 	// 写入颜色
				}
				disChar >>= 1;
				Xaddress++;  //水平坐标自加
				
				if( (Xaddress - x)==width ) //如果水平坐标达到了字符宽度，则退出当前循环
				{													//进入下一行的绘制
					Xaddress = x;
					y++;
					break;
				}
			}	
			pImage++;			
		}
	}	
}




