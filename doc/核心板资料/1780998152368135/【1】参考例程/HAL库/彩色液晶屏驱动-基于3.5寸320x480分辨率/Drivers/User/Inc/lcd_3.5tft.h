#ifndef __LCD_H
#define __LCD_H		

#include "stm32f4xx_hal.h"
#include "lcd_fonts.h"		// 图片和字库文件不是必须，用户可自行删减
#include	"lcd_image.h" 

#include "usart.h" 

/*--------------------------------------- 常用颜色 ----------------------------------*/
//	 虽然反客3.5寸的屏幕使用的颜色格式是16位的RGB565，但是这里为了方便用户使用，
//	 入口参数使用的是24位的RGB888颜色，然后再通过代码自动转换成16位色。用户可以很方便
//	 的在电脑用取色器获取24位的颜色，再将此24位颜色输入LCD_SetColor()或LCD_SetBackColor()
//  就可以要显示出相应的颜色。

#define 	LCD_WHITE       0xFFFFFF	// 白色
#define 	LCD_BLACK       0x000000	// 黑色
                           
#define 	LCD_BLUE        0x0000FF	// 蓝色
#define 	LCD_GREEN       0x00FF00   // 绿色
#define 	LCD_RED         0xFF0000   // 红色
#define 	LCD_CYAN        0x00FFFF   // 蓝绿色
#define 	LCD_MAGENTA     0xFF00FF   // 紫色
#define 	LCD_YELLOW      0xFFFF00   // 黄色
#define 	LCD_GREY        0x2C2C2C   // 灰色
                                    
#define 	LIGHT_BLUE      0x8080FF   // 亮蓝色
#define 	LIGHT_GREEN     0x80FF80   // 亮绿色
#define 	LIGHT_RED       0xFF8080   // 亮红色
#define 	LIGHT_CYAN      0x80FFFF   // 亮蓝绿色
#define 	LIGHT_MAGENTA   0xFF80FF   // 亮紫色
#define 	LIGHT_YELLOW    0xFFFF80   // 亮黄色
#define 	LIGHT_GREY      0xA3A3A3   // 亮灰色
                                     
#define 	DARK_BLUE       0x000080   // 暗蓝色
#define 	DARK_GREEN      0x008000   // 暗绿色
#define 	DARK_RED        0x800000   // 暗红色
#define 	DARK_CYAN       0x008080   // 暗蓝绿色
#define 	DARK_MAGENTA    0x800080   // 暗紫色
#define 	DARK_YELLOW     0x808000   // 暗黄色
#define 	DARK_GREY       0x404040   // 暗灰色

/*------------------------------------- 函数声明 -------------------------------------*/

void 	LCD_Init(void);	// 初始化LCD
void 	LCD_Clear(void);	// 清屏

void 	LCD_SetColor(uint32_t Color);			// 设置画笔颜色
void 	LCD_SetBackColor(uint32_t Color);		// 设置背景色
void 	LCD_SetAsciiFont(pFONT *Asciifonts);			// 设置字体
void 	LCD_SetCursor(uint16_t x, uint16_t y);		// 设置坐标
void 	LCD_DisplayMode(uint8_t direction);   // 设置显示方向

void 	LCD_DisplayChar(uint16_t x, uint16_t y,uint8_t add);				// 在指定坐标处显示单个ASCII字符
void 	LCD_DisplayString( uint16_t x, uint16_t y, char *p);				// 在指定坐标处显示字符串
void 	LCD_ShowNumMode(uint8_t mode);											// 设置数字显示的填充模式
void 	LCD_DisplayNumber( uint16_t x, uint16_t y, uint32_t number, uint8_t len) ;   // 显示十进制数

//>>>>>	显示中文字符，包括ASCII码
void 	LCD_SetTextFont(pFONT *fonts);										// 设置文本字体，包括中文和ASCII字体
void 	LCD_DisplayChinese(uint16_t x, uint16_t y, char *pText);		// 显示单个汉字
void 	LCD_DisplayText(uint16_t x, uint16_t y, char *pText) ;		// 显示字符串，包括中文和ASCII字符


void	LCD_DrawPoint(uint16_t x,uint16_t y);					// 画点
void  LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);		// 画线
void  LCD_DrawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height);	// 画矩形
void  LCD_DrawCircle(uint16_t x, uint16_t y, uint16_t r);					// 画圆
void  LCD_DrawEllipse(int x, int y, int r1, int r2);		// 画椭圆

void  LCD_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height);		// 填充矩形
void  LCD_FillCircle(uint16_t x, uint16_t y, uint16_t r);						// 填充圆

//>>>>>	绘制单色图片
void 	LCD_DrawImage(uint16_t x,uint16_t y,uint16_t width,uint16_t height,const uint8_t *pImage)  ;

/*-------------------------------LCD相关参数宏定义-------------------------------------*/

#define  Fill_Zero 	 0		//填充0
#define  Fill_Space 	 1		//填充空格

#define	Mode_H		 0		//LCD水平显示
#define	Mode_V		 1		//LCD竖屏显示

#define 	FSMC_REG     0x60000000		// FSMC 写寄存器地址
#define 	FSMC_DATA    0x60020000		// FSMC 写数据地址

/*------------------------------------- LCD配置宏 -------------------------------------*/

#define LCD_RST_PIN            			 		 GPIO_PIN_2        				 	// LCD_RST 引脚      
#define LCD_RST_PORT           			 		 GPIOC                 			 	// LCD_RST GPIO端口     
#define __HAL_RCC_LCD_RST_CLK_ENABLE    		 __HAL_RCC_GPIOC_CLK_ENABLE() 	// LCD_RST GPIO端口时钟
 
#define LCD_Backlight_PIN            			 GPIO_PIN_12        				 	// LCD_Backlight 引脚      
#define LCD_Backlight_PORT           			 GPIOD                 			 	// LCD_Backlight GPIO端口     
#define __HAL_RCC_LCD_Backlight_CLK_ENABLE    __HAL_RCC_GPIOD_CLK_ENABLE() 	// LCD_Backlight GPIO端口时钟
 
 
#define LCD_RST_L	  		HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_RESET)		// 输出低电平，复位屏幕
#define LCD_RST_H  	  	HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET)			// 输出高电平

 
#define LCD_Backlight_OFF	  HAL_GPIO_WritePin(LCD_Backlight_PORT, LCD_Backlight_PIN, GPIO_PIN_RESET)		// 输出低电平，关闭背光
#define LCD_Backlight_ON  	  HAL_GPIO_WritePin(LCD_Backlight_PORT, LCD_Backlight_PIN, GPIO_PIN_SET)		// 输出高电平，点亮背光


#endif  /*__LCD_H*/
	 
	 
