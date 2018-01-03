/*
  BrewBuddy by Marcelo Barros de Almeida is licensed under a
  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.

  For commercial usage, please contact the author via marcelobarrosalmeida@gmail.com

  Inspired on original work of Tilen Majerle, stm32f4-discovery.net
  but with a rewritten code for memory saving (no video buffer).
  Font binding idea kept as well, but using new and smaller fonts and data types.
  The font code came from Tracker-MicroAPRS project (https://is.gd/wIOgUF)
  New transmit and receive function using standard STM32 HAL.
  Only initialization was kept (almost the same).
*/

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ssd1306_fonts.h>
#include <ssd1306.h>
#include "stm32f0xx_hal.h"

extern I2C_HandleTypeDef hi2c1;

#define SSD1306_I2C       &hi2c1
#define SSD1306_I2C_ADDR  (0x78)

uint8_t SSD1306_Write_Command(uint8_t command)
{
	uint8_t data[2] = {0x00, command};

    if(HAL_I2C_Master_Transmit(SSD1306_I2C, SSD1306_I2C_ADDR, data, 2, 2) != HAL_OK)
        return 0;
    else
        return 1;
}

uint8_t SSD1306_Write_Data(uint8_t value)
{
	uint8_t data[2] = {0x40, value};

    if(HAL_I2C_Master_Transmit(SSD1306_I2C, SSD1306_I2C_ADDR, data, 2, 2) != HAL_OK)
        return 0;
    else
        return 1;
}

void SSD1306_Goto(uint8_t col, uint8_t row)
{
	if(row >= SSD1306_HEIGHT/8)
		row = SSD1306_HEIGHT/8 - 1;

	if(col >= SSD1306_WIDTH)
		col = SSD1306_WIDTH - 1;

	SSD1306_Write_Command(0xB0 | row);
	SSD1306_Write_Command(0x00 | (col & 0x0F));
	SSD1306_Write_Command(0x10 | (col >> 4));
}

void SSD1306_Write_Char(uint8_t c, SSD1306_Font_t* font)
{
	const uint8_t *base;
	uint8_t width, n;
	uint8_t data[16];

	if(c < 32 || c > 129)
		c = 63;

#if SSD1306_FONT_UPPERCASE_ONLY == 1
	if(c > 96 && c < 123)
		c -= 32;
	else if(c >= 123)
		c -= 26;
#endif

	c -= 32;

	width = font->width;
	data[0] = 0x40;
	base = font->data + width*c;
	for(n = 0 ; n < width ; n++)
		data[1+n] = base[n];

	data[width+1] = 0x00;

	HAL_I2C_Master_Transmit(SSD1306_I2C,SSD1306_I2C_ADDR, data, width+2, 10);
}

void SSD1306_Write_Buffer(uint8_t *str, uint8_t size, SSD1306_Font_t* font)
{
	uint8_t  n;
	for(n = 0 ; n < size ; n++)
		SSD1306_Write_Char(str[n],font);
}

void SSD1306_Write_String(uint8_t *str)
{
	SSD1306_Write_Buffer(str,strlen((char *)str),&SSD1306_Font_07X05);
}

void SSD1306_Write_Centered_String(uint8_t *str, uint8_t row)
{
	int16_t col = (SSD1306_WIDTH - strlen((char *)str)*6)/2;
	col = col < 0 ? 0 : col;
	SSD1306_Goto(col,row);
	SSD1306_Write_String(str);
}

void SDD1306_Clear_Line(uint8_t row)
{
	uint8_t  n;
	uint8_t size = SSD1306_WIDTH/(SSD1306_Font_07X05.width+1);
	SSD1306_Goto(0,row);
	for(n = 0 ; n < size ; n++)
		SSD1306_Write_Char(' ',&SSD1306_Font_07X05);
}

//void SDD1306_Draw_HLine(uint8_t col, uint8_t row, uint8_t len)
//{
//	uint8_t n;
//	uint8_t data[129];
//	uint8_t v = col - col/SSD1306_Font_07X05.height;
//	v = 1 << v;
//
//	SSD1306_Goto(col,row);
//	data[0] = 0x40;
//
//	for(n = 0 ; n < len ; n++)
//		data[1+n] = v;
//
//	HAL_I2C_Master_Transmit(SSD1306_I2C,SSD1306_I2C_ADDR, data, len+1, 10);
//}

void SDD1306_Clear_Screen(void)
{
	uint8_t n,m;
	uint8_t data[] = {0x40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	SSD1306_Goto(0,0);

	for(n = 0 ; n < SSD1306_HEIGHT/8 ; n++)
		for(m = 0 ; m < SSD1306_WIDTH/16 ; m++)
			HAL_I2C_Master_Transmit(SSD1306_I2C,SSD1306_I2C_ADDR, data, 17, 10);
}

uint8_t SSD1306_Init(void)
{
	
	HAL_Delay(100);

	SSD1306_Write_Command(0xAE); //display off
	SSD1306_Write_Command(0x20); //Set Memory Addressing Mode
	SSD1306_Write_Command(0x10); //00,Horizontal Addressing Mode;01,Vertical Addressing Mode;10,Page Addressing Mode (RESET);11,Invalid
	SSD1306_Write_Command(0xB0); //Set Page Start Address for Page Addressing Mode,0-7
	SSD1306_Write_Command(0xC8); //Set COM Output Scan Direction
	SSD1306_Write_Command(0x00); //---set low column address
	SSD1306_Write_Command(0x10); //---set high column address
	SSD1306_Write_Command(0x40); //--set start line address
	SSD1306_Write_Command(0x81); //--set contrast control register
	SSD1306_Write_Command(0xFF);
	SSD1306_Write_Command(0xA1); //--set segment re-map 0 to 127
	SSD1306_Write_Command(0xA6); //--set normal display
	SSD1306_Write_Command(0xA8); //--set multiplex ratio(1 to 64)
	SSD1306_Write_Command(0x3F); //
	SSD1306_Write_Command(0xA4); //0xa4,Output follows RAM content;0xa5,Output ignores RAM content
	SSD1306_Write_Command(0xD3); //-set display offset
	SSD1306_Write_Command(0x00); //-not offset
	SSD1306_Write_Command(0xD5); //--set display clock divide ratio/oscillator frequency
	SSD1306_Write_Command(0xf0); //--set divide ratio 0xF0
	SSD1306_Write_Command(0xD9); //--set pre-charge period
	SSD1306_Write_Command(0x22); //
	SSD1306_Write_Command(0xDA); //--set com pins hardware configuration
	SSD1306_Write_Command(0x12);
	SSD1306_Write_Command(0xDB); //--set vcomh
	SSD1306_Write_Command(0x20); //0x20,0.77xVcc,
	SSD1306_Write_Command(0x8D); //--set DC-DC enable
	SSD1306_Write_Command(0x14); //
	SSD1306_Write_Command(0xAF); //--turn on SSD1306 panel

	SDD1306_Clear_Screen();
	
	return 1;
}
 
void SSD1306_On(void)
{
	SSD1306_Write_Command(0x8D);  
	SSD1306_Write_Command(0x14);  
	SSD1306_Write_Command(0xAF);  
}

void SSD1306_Off(void)
{
	SSD1306_Write_Command(0x8D);  
	SSD1306_Write_Command(0x10);
	SSD1306_Write_Command(0xAE);  
}
