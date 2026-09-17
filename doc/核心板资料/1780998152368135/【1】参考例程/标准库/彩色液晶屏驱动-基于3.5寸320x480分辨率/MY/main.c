/***
	***************************************************************************************************
	*	@file  	main.c
	*	@version V1.4
	*	@author  鹿小班科技	
	*	@brief   驱动3.5寸屏
   ****************************************************************************************************
   *  @description
	*
	*	实验平台：鹿小班STM32F407VET6核心板 （型号：LXB407VE-P1）
	*	客服微信：19949278543
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
	*		修复 LCD_DrawPoint 函数画点颜色不正确的BUG
	*
>>>>> 功能说明：
	*
	*	驱动屏幕演示
	* 		
	***************************************************************************************************
***/


#include "stm32f4xx.h"
#include "led.h"   
#include "delay.h"
#include "key.h"  
#include "usart.h"
#include "lcd_3.5tft.h"

void LCD_ClearTest(void);			// 清屏测试
void LCD_TextTest(void);			// 文本显示测试
void LCD_FillTest(void);			// 矩形填充测试
void LCD_ColorTest(void);			// 颜色测试
void LCD_GrahicTest(void);			// 绘图测试
void LCD_Image_Test(void);			// 图片显示测试(单色图片)
void LCD_HorizontalText(void);	// 横屏测试


/***************************************************************************************************
*	函 数 名: main
*	入口参数: 无
*	返 回 值: 无
*	函数功能: 运行主程序
*	说    明: 无
****************************************************************************************************/

int main(void)
{
	Delay_Init();		//延时函数初始化
	LED_Init();			//LED初始化
	KEY_Init();			//按键IO口初始化
	Usart_Config ();	// USART初始化函数

	printf("鹿小班-STM32F407VET6核心板测试\r\n");
	
	LCD_Init();		// LCD初始化

	while (1)
	{
		LED1_Toggle;

		LCD_DisplayMode(Mode_V);	// 竖屏显示	
		LCD_ClearTest();		// 清屏测试
		LCD_TextTest();		// 文本显示测试
		LCD_FillTest();		// 矩形填充测试
		LCD_ColorTest();		// 颜色测试
		LCD_GrahicTest();		// 绘图测试
		LCD_Image_Test();		// 图片显示测试(单色图片)
		
		LCD_DisplayMode(Mode_H);	// 横屏显示
		LCD_HorizontalText();		// 横屏测试
	}
	
}



/*---------------------------- 测试函数 ---------------------------------*/

// 函数：清屏测试
//
void LCD_ClearTest(void)
{
	u16 time = 1000;	//延时时间

	LCD_SetAsciiFont(&ASCII_Font24);		//设置字体
	LCD_SetColor(LCD_BLACK);	//设置画笔颜色

	LCD_SetBackColor(LCD_RED);    LCD_Clear();  Delay_ms(time);
	LCD_SetBackColor(LCD_GREEN);  LCD_Clear();  Delay_ms(time);
	LCD_SetBackColor(LCD_BLUE);   LCD_Clear();  Delay_ms(time);
	LCD_SetBackColor(LCD_GREY);   LCD_Clear();  Delay_ms(time);	
	LCD_SetBackColor(LCD_WHITE);   LCD_Clear();  Delay_ms(time);
	LCD_SetBackColor(LCD_BLACK);   LCD_Clear();  Delay_ms(time);
}
// 函数：文本测试
//
void LCD_TextTest(void)
{

	LCD_SetBackColor(LCD_BLACK); //设置背景色
	LCD_Clear(); //清屏，刷背景色

	LCD_SetColor(LCD_WHITE);
	LCD_SetAsciiFont(&ASCII_Font32); LCD_DisplayString(0, 0,"!#$%&'()*+,-.01"); 						
	LCD_SetAsciiFont(&ASCII_Font24); LCD_DisplayString(0,32,"!#$%&'()*+,-.01234"); 			
	LCD_SetAsciiFont(&ASCII_Font20); LCD_DisplayString(0,56,"!#$%&'()*+,-.012345"); 				
	LCD_SetAsciiFont(&ASCII_Font16); LCD_DisplayString(0,76,"!#$%&'()*+,-.01234567"); 			
	LCD_SetAsciiFont(&ASCII_Font12); LCD_DisplayString(0,92,"!#$%&'()*+,-.0123456789:"); 		

	LCD_SetColor(LCD_CYAN);                                                      
	LCD_SetAsciiFont(&ASCII_Font12); LCD_DisplayString(0,104,"!#$%&'()*+,-.0123456789:"); 		
	LCD_SetAsciiFont(&ASCII_Font16); LCD_DisplayString(0,116,"!#$%&'()*+,-.01234567"); 			
	LCD_SetAsciiFont(&ASCII_Font20); LCD_DisplayString(0,132,"!#$%&'()*+,-.012345"); 				
	LCD_SetAsciiFont(&ASCII_Font24); LCD_DisplayString(0,152,"!#$%&'()*+,-.01234"); 					
	LCD_SetAsciiFont(&ASCII_Font32); LCD_DisplayString(0,176,"!#$%&'()*+,-.01"); 					

	LCD_SetAsciiFont(&ASCII_Font32);
	LCD_SetColor(LCD_YELLOW);
	LCD_DisplayNumber( 0,220,429496729,9);   
	LCD_ShowNumMode(Fill_Zero);	
	LCD_DisplayNumber( 0,252,123456,9);	     
	LCD_ShowNumMode(Fill_Space);	
	LCD_DisplayNumber( 0,284,1234,9);		  

	LCD_SetTextFont(&CH_Font12);			// 设置1212中文字体,ASCII字体对应为1206
	LCD_SetColor(0Xff8AC6D1);						// 设置画笔
	LCD_DisplayText(0, 330,"鹿小班科技");	

	LCD_SetTextFont(&CH_Font16);			// 设置1616中文字体,ASCII字体对应为1608
	LCD_SetColor(0XffC5E1A5);						// 设置画笔
	LCD_DisplayText(0, 350,"鹿小班科技");		
	
	LCD_SetTextFont(&CH_Font20);			// 设置2020中文字体,ASCII字体对应为2010
	LCD_SetColor(0Xff2D248A);						// 设置画笔	
	LCD_DisplayText(0, 370,"鹿小班科技");		
	
	LCD_SetTextFont(&CH_Font24);			// 设置2424中文字体,ASCII字体对应为2412
	LCD_SetColor(0XffFF585D);						// 设置画笔	
	LCD_DisplayText(0, 400,"鹿小班科技");		

	LCD_SetTextFont(&CH_Font32);			// 设置3232中文字体,ASCII字体对应为3216
	LCD_SetColor(0XffF6003C);						// 设置画笔
	LCD_DisplayText(0, 430,"鹿小班科技");	
	
	Delay_ms(2000);	
}

// 函数：矩形填充测试
//
void LCD_FillTest(void)
{
	LCD_SetBackColor(LCD_BLACK); //设置背景色
	LCD_Clear(); //清屏，刷背景色

	LCD_SetAsciiFont(&ASCII_Font16);
	LCD_SetColor(LCD_BLUE);	  	  LCD_FillRect(110,  3,130,16);	LCD_DisplayString(0,  3,"LCD_BLUE");		
	LCD_SetColor(LCD_GREEN);  	  LCD_FillRect(110, 25,130,16);  LCD_DisplayString(0, 25,"LCD_GREEN");		
	LCD_SetColor(LCD_RED);    	  LCD_FillRect(110, 47,130,16);  LCD_DisplayString(0, 47,"LCD_RED");			
	LCD_SetColor(LCD_CYAN);   	  LCD_FillRect(110, 69,130,16);  LCD_DisplayString(0, 69,"LCD_CYAN");		
	LCD_SetColor(LCD_MAGENTA);	  LCD_FillRect(110, 91,130,16);  LCD_DisplayString(0, 91,"LCD_MAGENTA");	
	LCD_SetColor(LCD_YELLOW); 	  LCD_FillRect(110,113,130,16);  LCD_DisplayString(0,113,"LCD_YELLOW");		
	LCD_SetColor(LCD_GREY);   	  LCD_FillRect(110,135,130,16);	LCD_DisplayString(0,135,"LCD_GREY");		
                                      
	LCD_SetColor(LIGHT_BLUE);	  LCD_FillRect(110,157,130,16);  LCD_DisplayString(0,157,"LIGHT_BLUE");		
	LCD_SetColor(LIGHT_GREEN);   LCD_FillRect(110,179,130,16);  LCD_DisplayString(0,179,"LIGHT_GREEN");	
	LCD_SetColor(LIGHT_RED);     LCD_FillRect(110,201,130,16);  LCD_DisplayString(0,201,"LIGHT_RED");	   
	LCD_SetColor(LIGHT_CYAN);    LCD_FillRect(110,223,130,16);  LCD_DisplayString(0,223,"LIGHT_CYAN");	   
	LCD_SetColor(LIGHT_MAGENTA); LCD_FillRect(110,245,130,16);  LCD_DisplayString(0,245,"LIGHT_MAGENTA");	
	LCD_SetColor(LIGHT_YELLOW);  LCD_FillRect(110,267,130,16);  LCD_DisplayString(0,267,"LIGHT_YELLOW");	
	LCD_SetColor(LIGHT_GREY);    LCD_FillRect(110,289,130,16);	LCD_DisplayString(0,289,"LIGHT_GREY");  	
                                                     
	Delay_ms(2000);
}

// 函数：颜色测试
//
void LCD_ColorTest(void)
{
	u16 i;

	LCD_SetBackColor(LCD_BLACK); //设置背景色
	LCD_Clear(); //清屏，刷背景色
	LCD_SetAsciiFont(&ASCII_Font32);
	LCD_SetColor(LCD_BLACK);

	LCD_SetBackColor(LIGHT_MAGENTA); LCD_DisplayString(0,  0,"   Color Test   "); 
	LCD_SetBackColor(LCD_YELLOW);  	LCD_DisplayString(0, 40,"   Color Test   ");
	LCD_SetBackColor(LCD_CYAN);  	 	LCD_DisplayString(0, 80,"   Color Test   ");

	//使用画线函数绘制三基色色条
	for(i=0;i<240;i++)
	{
		LCD_SetColor( LCD_RED-(i<<16) );
		LCD_DrawLine(i,150,i,190);			
	}
	for(i=0;i<240;i++)
	{
		LCD_SetColor( LCD_GREEN-(i<<8) );
		LCD_DrawLine(i,200,i,240);	
	}
	for(i=0;i<240;i++)
	{
		LCD_SetColor( LCD_BLUE-i );
		LCD_DrawLine(i,250,i,290);	
	}	
	Delay_ms(2000);	
}
// 函数：绘图测试
//
void LCD_GrahicTest(void)
{
	u16 time = 80;

	LCD_SetBackColor(LCD_BLACK); //设置背景色
	LCD_Clear(); //清屏，刷背景色	

	LCD_SetColor(LCD_RED);
	LCD_DrawLine(0,10,240,10);	Delay_ms(time);	//画直线
	LCD_DrawLine(0,20,240,20);	Delay_ms(time);
	LCD_DrawLine(0,30,240,30); Delay_ms(time);
	LCD_DrawLine(0,40,240,40);	Delay_ms(time);	
	
	LCD_SetColor(LCD_YELLOW);	
	LCD_DrawRect( 5, 85,240,150); Delay_ms(time);	//绘制矩形
	LCD_DrawRect(30,100,190,120); Delay_ms(time);
	LCD_DrawRect(55,115,140,90);  Delay_ms(time);
	LCD_DrawRect(80,135,90,60);   Delay_ms(time);	

	LCD_SetColor(LIGHT_CYAN);
	LCD_DrawCircle(120,170,100);	Delay_ms(time);	//绘制圆形
	LCD_DrawCircle(120,170,80);   Delay_ms(time);
	LCD_DrawCircle(120,170,60);   Delay_ms(time);
	LCD_DrawCircle(120,170,40);   Delay_ms(time);

	LCD_SetColor(DARK_CYAN);
	LCD_DrawLine(0,285,240,285);	Delay_ms(time);	//画直线
	LCD_DrawLine(0,295,240,295);	Delay_ms(time);
	LCD_DrawLine(0,305,240,305);  Delay_ms(time);
	LCD_DrawLine(0,315,240,315);	Delay_ms(time);	
	Delay_ms(1000);

	LCD_SetBackColor(LCD_BLACK); //设置背景色
	LCD_Clear(); //清屏，刷背景色	
	
	LCD_SetColor(LCD_RED);    LCD_FillCircle( 60,80,60);		//填充圆形
	LCD_SetColor(LCD_GREEN);  LCD_FillCircle(120,80,60); 	
	LCD_SetColor(LCD_BLUE);   LCD_FillCircle(160,80,60);  	

	LCD_SetColor(LIGHT_BLUE);
	LCD_DrawEllipse(120,250,110,50);  Delay_ms(time);	//绘制椭圆
	LCD_DrawEllipse(120,250, 95,40);  Delay_ms(time);
	LCD_DrawEllipse(120,250, 80,30);  Delay_ms(time);
	LCD_DrawEllipse(120,250, 65,20);  Delay_ms(time);

	Delay_ms(2000);	
}

/*************************************************************************************************
*	函 数 名:	LCD_Test_Image
*
*	函数功能:	图片显示测试
*************************************************************************************************/
void LCD_Image_Test(void)
{
	LCD_SetBackColor(LCD_BLACK); 			//	设置背景色
	LCD_Clear(); 								// 清屏
	
	LCD_SetColor( 0xF6E58D);
	LCD_DrawImage( 19, 55, 83, 83, Image_Android_83x83) ;	   // 显示图片

	LCD_SetColor( 0xDFF9FB);
	LCD_DrawImage( 141, 55, 83, 83, Image_Message_83x83) ;	// 显示图片
	
	LCD_SetColor( 0x9DD3A8);
	LCD_DrawImage( 19, 175, 83, 83, Image_Toys_83x83) ;		// 显示图片
	
	LCD_SetColor( 0xFF8753);
	LCD_DrawImage( 141, 175, 83, 83, Image_Video_83x83) ;		// 显示图片

	Delay_ms(2000);	
}

void LCD_HorizontalText(void)
{
	u16 time = 100;
	
	LCD_ClearTest();	// 清屏测试函数

	
	LCD_SetBackColor(LCD_BLACK); //设置背景色
	LCD_Clear(); //清屏，刷背景色
	LCD_SetColor(LCD_WHITE);
	LCD_SetAsciiFont(&ASCII_Font32); LCD_DisplayString(0, 0,"!#$%&'()*+,-.0123456"); 		Delay_ms(time);	// 字符显示测试
	LCD_SetAsciiFont(&ASCII_Font24); LCD_DisplayString(0,32,"!#$%&'()*+,-.0123456789:"); 	Delay_ms(time);	// 字符显示测试
	LCD_SetAsciiFont(&ASCII_Font20); LCD_DisplayString(0,56,"!#$%&'()*+,-.0123456789:"); 	Delay_ms(time);	// 字符显示测试
	LCD_SetAsciiFont(&ASCII_Font16); LCD_DisplayString(0,76,"!#$%&'()*+,-.0123456789:"); 	Delay_ms(time);	// 字符显示测试
	LCD_SetAsciiFont(&ASCII_Font12); LCD_DisplayString(0,92,"!#$%&'()*+,-.0123456789:"); 	Delay_ms(time);	// 字符显示测试
	LCD_SetColor(LIGHT_YELLOW);                                                      
	LCD_SetAsciiFont(&ASCII_Font12); LCD_DisplayString(0,104,"!#$%&'()*+,-.0123456789:"); Delay_ms(time);	// 字符显示测试
	LCD_SetAsciiFont(&ASCII_Font16); LCD_DisplayString(0,116,"!#$%&'()*+,-.0123456789:"); Delay_ms(time);	// 字符显示测试
	LCD_SetAsciiFont(&ASCII_Font20); LCD_DisplayString(0,132,"!#$%&'()*+,-.0123456789:"); Delay_ms(time);	// 字符显示测试	
	LCD_SetAsciiFont(&ASCII_Font24); LCD_DisplayString(0,152,"!#$%&'()*+,-.0123456789:"); Delay_ms(time);	// 字符显示测试	
	LCD_SetAsciiFont(&ASCII_Font32); LCD_DisplayString(0,176,"!#$%&'()*+,-.0123456"); 		Delay_ms(time);	// 字符显示测试
	Delay_ms(2000);		
	
	LCD_SetBackColor(LCD_BLACK); //设置背景色
	LCD_Clear(); //清屏，刷背景色
	LCD_SetAsciiFont(&ASCII_Font16);
	LCD_SetColor(LCD_BLUE);	  	  LCD_FillRect(110,  3,210,16);	LCD_DisplayString(0,  3,"LCD_BLUE");		// 矩形填充测试
	LCD_SetColor(LCD_GREEN);  	  LCD_FillRect(110, 25,210,16);  LCD_DisplayString(0, 25,"LCD_GREEN");		// 矩形填充测试
	LCD_SetColor(LCD_RED);    	  LCD_FillRect(110, 47,210,16);  LCD_DisplayString(0, 47,"LCD_RED");			// 矩形填充测试
	LCD_SetColor(LCD_CYAN);   	  LCD_FillRect(110, 69,210,16);  LCD_DisplayString(0, 69,"LCD_CYAN");		// 矩形填充测试
	LCD_SetColor(LCD_MAGENTA);	  LCD_FillRect(110, 91,210,16);  LCD_DisplayString(0, 91,"LCD_MAGENTA");	// 矩形填充测试
	LCD_SetColor(LCD_YELLOW); 	  LCD_FillRect(110,113,210,16);  LCD_DisplayString(0,113,"LCD_YELLOW");		// 矩形填充测试
	LCD_SetColor(LCD_GREY);   	  LCD_FillRect(110,135,210,16);	LCD_DisplayString(0,135,"LCD_GREY");		// 矩形填充测试
                                                     
	LCD_SetColor(LIGHT_BLUE);	  LCD_FillRect(110,157,210,16);  LCD_DisplayString(0,157,"LIGHT_BLUE");		// 矩形填充测试
	LCD_SetColor(LIGHT_GREEN);   LCD_FillRect(110,179,210,16);  LCD_DisplayString(0,179,"LIGHT_GREEN");	// 矩形填充测试
	LCD_SetColor(LIGHT_RED);     LCD_FillRect(110,201,210,16);  LCD_DisplayString(0,201,"LIGHT_RED");	   // 矩形填充测试
	LCD_SetColor(LIGHT_CYAN);    LCD_FillRect(110,223,210,16);  LCD_DisplayString(0,223,"LIGHT_CYAN");	   // 矩形填充测试                                         
	Delay_ms(2000);	
	
	LCD_SetBackColor(LCD_BLACK); //设置背景色
	LCD_Clear(); //清屏，刷背景色		
	LCD_SetColor(LCD_RED);    LCD_FillCircle( 80,120,80);		//填充圆形
	LCD_SetColor(LCD_GREEN);  LCD_FillCircle(160,120,80); 	//填充圆形
	LCD_SetColor(LCD_BLUE);   LCD_FillCircle(240,120,80);  	//填充圆形
	Delay_ms(2000);		
}




