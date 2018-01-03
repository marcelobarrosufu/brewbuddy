/*
  BrewBuddy by Marcelo Barros de Almeida is licensed under a
  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.

  For commercial usage, please contact the author via marcelobarrosalmeida@gmail.com
*/

#include <stdint.h>
#include "lmt86.h"

typedef struct lmt86_table_row_s
{
	int8_t temp_c;
	uint16_t mv_1d;
} __attribute__((packed)) lmt86_table_row_t;

const lmt86_table_row_t lmt86_table[]  =
{
	// temp C, mV*10
	{ -5, 21540 },
	{ -4, 21430 },
	{ -3, 21320 },
	{ -2, 21220 },
	{ -1, 21110 },
	{ 0, 21000 },
	{ 1, 20890 },
	{ 2, 20790 },
	{ 3, 20680 },
	{ 4, 20570 },
	{ 5, 20470 },
	{ 6, 20360 },
	{ 7, 20250 },
	{ 8, 20140 },
	{ 9, 20040 },
	{ 10, 19930 },
	{ 11, 19820 },
	{ 12, 19710 },
	{ 13, 19610 },
	{ 14, 19500 },
	{ 15, 19390 },
	{ 16, 19280 },
	{ 17, 19180 },
	{ 18, 19070 },
	{ 19, 18960 },
	{ 20, 18850 },
	{ 21, 18740 },
	{ 22, 18640 },
	{ 23, 18530 },
	{ 24, 18420 },
	{ 25, 18310 },
	{ 26, 18200 },
	{ 27, 18100 },
	{ 28, 17990 },
	{ 29, 17880 },
	{ 30, 17770 },
	{ 31, 17660 },
	{ 32, 17560 },
	{ 33, 17450 },
	{ 34, 17340 },
	{ 35, 17230 },
	{ 36, 17120 },
	{ 37, 17010 },
	{ 38, 16900 },
	{ 39, 16790 },
	{ 40, 16680 },
	{ 41, 16570 },
	{ 42, 16460 },
	{ 43, 16350 },
	{ 44, 16240 },
	{ 45, 16130 },
	{ 46, 16020 },
	{ 47, 15910 },
	{ 48, 15800 },
	{ 49, 15690 },
	{ 50, 15580 },
	{ 51, 15470 },
	{ 52, 15360 },
	{ 53, 15250 },
	{ 54, 15140 },
	{ 55, 15030 },
	{ 56, 14920 },
	{ 57, 14810 },
	{ 58, 14700 },
	{ 59, 14590 },
	{ 60, 14480 },
	{ 61, 14360 },
	{ 62, 14250 },
	{ 63, 14140 },
	{ 64, 14030 },
	{ 65, 13910 },
	{ 66, 13800 },
	{ 67, 13690 },
	{ 68, 13580 },
	{ 69, 13460 },
	{ 70, 13350 },
	{ 71, 13240 },
	{ 72, 13130 },
	{ 73, 13010 },
	{ 74, 12900 },
	{ 75, 12790 },
	{ 76, 12680 },
	{ 77, 12570 },
	{ 78, 12450 },
	{ 79, 12340 },
	{ 80, 12230 },
	{ 81, 12120 },
	{ 82, 12010 },
	{ 83, 11890 },
	{ 84, 11780 },
	{ 85, 11670 },
	{ 86, 11550 },
	{ 87, 11440 },
	{ 88, 11330 },
	{ 89, 11220 },
	{ 90, 11100 },
	{ 91, 10990 },
	{ 92, 10880 },
	{ 93, 10760 },
	{ 94, 10650 },
	{ 95, 10540 },
	{ 96, 10420 },
	{ 97, 10310 },
	{ 98, 10200 },
	{ 99, 10080 },
	{ 100, 9970 },
	{ 101, 9860 },
	{ 102, 9740 },
	{ 103, 9630 },
	{ 104, 9510 },
	{ 105, 9400 },
	{ 106, 9290 },
	{ 107, 9170 },
	{ 108, 9060 },
	{ 109, 8950 },
	{ 110, 8830 },
};

const uint16_t lmt86_table_size = sizeof(lmt86_table)/sizeof(lmt86_table[0]);

int16_t lmt86_find_temp_idx(uint32_t mv_id)
{
	uint16_t mid = 0;
	int16_t upper = lmt86_table_size - 1;
	int16_t lower = 0;

	while(lower <= upper)
	{
		mid = lower + (upper - lower) / 2;

		if(lmt86_table[mid].mv_1d == mv_id)
		{
			break;
		}
		else if(mv_id < lmt86_table[mid].mv_1d)
		{
			lower = mid + 1;
		}
		else
		{
			upper = mid - 1;
		}
	}

	return mid;
}

int32_t lmt86_interpolate_1d(int16_t idx, int32_t mv_1d)
{
	if(((idx <= 0)                      && (mv_1d > lmt86_table[idx].mv_1d)) ||
	   ((idx >= (lmt86_table_size - 1)) && (mv_1d < lmt86_table[idx].mv_1d)))
	{
		return lmt86_table[idx].temp_c;
	}
	else
	{
		volatile int32_t t1, t2, v1, v2, r, dt, dv;

		if(mv_1d < lmt86_table[idx].mv_1d)
		{
			t2 = lmt86_table[idx+1].temp_c;
			t1 = lmt86_table[idx].temp_c;
			v2 = lmt86_table[idx+1].mv_1d;
			v1 = lmt86_table[idx].mv_1d;

		}
		else
		{
			t2 = lmt86_table[idx].temp_c;
			t1 = lmt86_table[idx-1].temp_c;
			v2 = lmt86_table[idx].mv_1d;
			v1 = lmt86_table[idx-1].mv_1d;
		}

		dt = t2 - t1;
		dv = v2 - v1;

		r = ((dt*mv_1d + t1*dv - dt*v1)*100) / dv;
		r = r / 10;

		return r;
	}
}

int32_t lmt86_locale_conv_temp(int32_t c1d)
{
#if BB_TEMP_UNIT_IS_CELSIUS == 1
	return c1d;
#else
	return ((c1d*18) + 3200)/10;
#endif
}

int32_t lmt86_conv_temp_1d(uint32_t adc_val, uint32_t adc_steps, uint32_t vref_mv)
{
	int16_t idx;
	volatile uint32_t mv_1d = ((adc_val*vref_mv*100)/adc_steps)/10;
	idx = lmt86_find_temp_idx(mv_1d);

	if(idx == -1)
		return lmt86_locale_conv_temp(lmt86_table[0].temp_c*10); // error, minimum scale value
	else if(lmt86_table[idx].mv_1d == mv_1d) // exact
		return lmt86_locale_conv_temp(lmt86_table[idx].temp_c*10);
	else
		return lmt86_locale_conv_temp(lmt86_interpolate_1d(idx,mv_1d));
}
