/*
  BrewBuddy by Marcelo Barros de Almeida is licensed under a
  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.

  For commercial usage, please contact the author via marcelobarrosalmeida@gmail.com

  This file is a modification of
  https://github.com/stanleyseow/ArduinoTracker-MicroAPRS/blob/master/libraries/SSD1306_text/ssdfont.h

  (original license not found)
*/

#ifndef SSD1306_FONTS_H
#define SSD1306_FONTS_H

#ifdef __cplusplus
extern "C" {
#endif

#define SSD1306_FONT_07X05 1
#define SSD1306_FONT_UPPERCASE_ONLY 0

typedef struct
{
	uint8_t width;
	uint8_t height;
	const uint8_t *data;
} SSD1306_Font_t;

#if SSD1306_FONT_07X05 == 1
extern SSD1306_Font_t SSD1306_Font_07X05;
#endif

#ifdef __cplusplus
}
#endif

 
#endif

