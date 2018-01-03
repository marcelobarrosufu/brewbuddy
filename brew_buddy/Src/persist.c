/*
  BrewBuddy by Marcelo Barros de Almeida is licensed under a
  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.

  For commercial usage, please contact the author via marcelobarrosalmeida@gmail.com
*/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "main.h"
#include "stm32f0xx_hal.h"
#include "bb.h"
#include "ctrl.h"
#include "persist.h"

#define PERSIST_START       0x08003C00L
#define PERSIST_SIZE        (2 + 10*4 + 10*4 + 4 + 2)
#define PERSIST_CRC32_ADDR  (PERSIST_START + PERSIST_SIZE)
#define PERSIST_CRC32_GET() (*((uint32_t *)(PERSIST_CRC32_ADDR)))
#define PERSIST_HDR_GET()   (*((uint16_t *)(PERSIST_START)))

extern CRC_HandleTypeDef hcrc;

/*
Flash Memory Layout for Persistence
Sector 3, page 3

OFFSET   DATA
===================
0x0000   NUM_SLOPES
0x0001   NUM_HOPS

0x0002   TIME (S) SLOPE 1
0x0004   TEMP (C) SLOPE 1
0x0006   TIME (S) SLOPE 2
0x0008   TEMP (C) SLOPE 2
0x000A   TIME (S) SLOPE 3
0x000C   TEMP (C) SLOPE 3
0x000E   TIME (S) SLOPE 4
0x0010   TEMP (C) SLOPE 4
0x0012   TIME (S) SLOPE 5
0x0014   TEMP (C) SLOPE 5
0x0016   TIME (S) SLOPE 6
0x0018   TEMP (C) SLOPE 6
0x001A   TIME (S) SLOPE 7
0x001C   TEMP (C) SLOPE 7
0x001E   TIME (S) SLOPE 8
0x0020   TEMP (C) SLOPE 8
0x0022   TIME (S) SLOPE 9
0x0024   TEMP (C) SLOPE 9
0x0026   TIME (S) SLOPE 10
0x0028   TEMP (C) SLOPE 10

0x002A   TIME (S) HOP 1
0x002C   NUMBER   HOP 1
0x002E   TIME (S) HOP 2
0x0030   NUMBER   HOP 2
0x0032   TIME (S) HOP 3
0x0034   NUMBER   HOP 3
0x0036   TIME (S) HOP 4
0x0038   NUMBER   HOP 4
0x003A   TIME (S) HOP 5
0x003C   NUMBER   HOP 5
0x003E   TIME (S) HOP 6
0x0040   NUMBER   HOP 6
0x0042   TIME (S) HOP 7
0x0044   NUMBER   HOP 7
0x0046   TIME (S) HOP 8
0x0048   NUMBER   HOP 8
0x004A   TIME (S) HOP 9
0x004C   NUMBER   HOP 9
0x004E   TIME (S) HOP 10
0x0050   NUMBER   HOP 10

0x0052   BOIL TIME (S)
0x0054   BOIL TEMP (C)

0X0056   PADDING (0)

0X0058   CRC32
*/
void persist_init(void)
{
	uint32_t crc32;
	uint32_t crc32_saved;

	crc32 = HAL_CRC_Calculate(&hcrc, (uint32_t *)PERSIST_START, PERSIST_SIZE >> 2);
	crc32_saved = PERSIST_CRC32_GET();

	if((crc32 != crc32_saved) || (HAL_GPIO_ReadPin(GPIOA,BB_KEY_RIGHT) == GPIO_PIN_RESET))
	{
		uint8_t num_slopes = 3;
		uint8_t num_hops = 3;
#if BB_TEMP_UNIT_IS_CELSIUS == 1
		ctrl_slope_t slopes[3] = { {40*60, 62}, {20*60, 68}, {10*60, 76} };
		ctrl_hop_t hops[3] = { {55*60, 0}, {15*60, 1}, {5*60, 2} };
		ctrl_slope_t boil = {60*60, 96};
#else
		ctrl_slope_t slopes[3] = { {40*60, 144}, {20*60, 155}, {10*60, 167} };
		ctrl_hop_t hops[3] = { {55*60, 0}, {15*60, 1}, {5*60, 2} };
		ctrl_slope_t boil = {60*60, 205};
#endif
		persist_save(num_slopes,slopes,num_hops,hops,boil);
	}
}

void persist_save(uint8_t num_slopes, ctrl_slope_t slopes[], uint8_t num_hops, ctrl_hop_t hops[], ctrl_slope_t boil)
{
	uint8_t n;
	uint32_t crc32;
	uint32_t error;
	FLASH_EraseInitTypeDef page = { FLASH_TYPEERASE_PAGES, PERSIST_START, 1 };

	if((num_slopes > CTRL_MAX_SLOPES) || (num_hops > CTRL_MAX_HOPS))
		return;

	HAL_FLASH_Unlock();
	HAL_FLASHEx_Erase(&page,&error);

	HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,PERSIST_START + 0x0000, (num_slopes << 8 | num_hops));

	for(n = 0 ; n < num_slopes ; n++)
	{
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,PERSIST_START + 0x0002 + n*4,slopes[n].time_s);
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,PERSIST_START + 0x0004 + n*4,slopes[n].temp);
	}

	for(n = 0 ; n < num_hops ; n++)
	{
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,PERSIST_START + 0x002A + n*4,hops[n].time_s);
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,PERSIST_START + 0x002C + n*4,hops[n].number);
	}

	HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,PERSIST_START + 0x0052,boil.time_s);
	HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,PERSIST_START + 0x0054,boil.temp);

	crc32 = HAL_CRC_Calculate(&hcrc, (uint32_t *)PERSIST_START, PERSIST_SIZE >> 2);
	HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,PERSIST_CRC32_ADDR,crc32);

	HAL_FLASH_Lock();
}

void persist_load(uint8_t *num_slopes, ctrl_slope_t slopes[], uint8_t *num_hops, ctrl_hop_t hops[], ctrl_slope_t *boil)
{
	uint8_t n;
	uint16_t hdr = PERSIST_HDR_GET();
	*num_slopes = hdr >> 8;
	*num_hops = hdr & 0xFF;

	for(n = 0 ; n < *num_slopes ; n++)
	{
		slopes[n].time_s = *((uint16_t *)(PERSIST_START + 0x0002 + n*4));
		slopes[n].temp   = *((uint16_t *)(PERSIST_START + 0x0004 + n*4));
	}

	for(n = 0 ; n < *num_hops ; n++)
	{
		hops[n].time_s = *((uint16_t *)(PERSIST_START + 0x002A + n*4));
		hops[n].number = *((uint16_t *)(PERSIST_START + 0x002C + n*4));
	}

	boil->time_s = *((uint16_t *)(PERSIST_START + 0x0052));
	boil->temp   = *((uint16_t *)(PERSIST_START + 0x0054));
}

