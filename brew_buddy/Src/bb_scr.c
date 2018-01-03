/*
  BrewBuddy by Marcelo Barros de Almeida is licensed under a
  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.

  For commercial usage, please contact the author via marcelobarrosalmeida@gmail.com
*/

#include <bb_locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "stm32f0xx_hal.h"
#include "ssd1306_fonts.h"
#include "ssd1306.h"
#include "bb.h"
#include "bb_scr.h"
#include "main.h"
#include "ctrl.h"

bb_time_t bb_free_timer;
bool bb_free_timer_up_counter = true;
uint8_t bb_scr_slope_idx = 0;
uint8_t bb_scr_hop_idx = 0;
ctrl_slope_t bb_scr_slope;
ctrl_slope_t bb_scr_boil;
uint8_t timer_1s = 0;
ctrl_hop_t bb_scr_hop;

#define XMACRO_SCR_LIST \
	X(BB_SCR_SPLASH_ID,           bb_scr_splash) \
	X(BB_SCR_OP_TEMP_ID,          bb_scr_op_temp) \
	X(BB_SCR_TEMP_ID,             bb_scr_temp) \
    X(BB_SCR_OP_AUTO_MASH_ID,     bb_scr_op_auto_mash) \
    X(BB_SCR_AUTO_SLOP_TEMP_ID,   bb_scr_auto_slop_temp) \
    X(BB_SCR_AUTO_SLOP_DURA_ID,   bb_scr_auto_slop_duration) \
    X(BB_SCR_AUTO_MASH_ID,        bb_scr_auto_mash) \
	X(BB_SCR_AUTO_MASH_DONE_ID,   bb_scr_auto_done_mash) \
	X(BB_SCR_OP_AUTO_BOIL_ID,     bb_scr_op_auto_boil) \
	X(BB_SCR_AUTO_BOIL_TEMP_ID,   bb_scr_auto_boil_temp) \
	X(BB_SCR_AUTO_BOIL_DURA_ID,   bb_scr_auto_boil_duration) \
	X(BB_SCR_AUTO_BOIL_HOP_ID,    bb_scr_auto_boil_hop) \
	X(BB_SCR_AUTO_BOIL_HOP_UPD_ID,bb_scr_auto_boil_upd_hop) \
	X(BB_SCR_AUTO_BOIL_ID,        bb_scr_auto_boil) \
	X(BB_SCR_AUTO_BOIL_DONE_ID,   bb_scr_auto_boil_done)

typedef enum bb_scr_list_e
{
#define X(SCR_ENUM_NAME, SCR_FUNC_NAME) SCR_ENUM_NAME,
		XMACRO_SCR_LIST
#undef X
} bb_scr_list_t;

typedef bb_scr_list_t (*bb_scr_func_t)(bool first_time, bb_key_event_t *key);

#define X(SCR_ENUM_NAME, SCR_FUNC_NAME) \
		bb_scr_list_t SCR_FUNC_NAME(bool first_time, bb_key_event_t *key);
	XMACRO_SCR_LIST
#undef X

bb_scr_func_t scr_funcs[] =
{
#define X(SCR_ENUM_NAME, SCR_FUNC_NAME) SCR_FUNC_NAME,
		XMACRO_SCR_LIST
#undef X
};

void bb_free_timer_get(bb_time_t *tm)
{
	*tm = bb_free_timer;
}

void bb_free_timer_reset(void)
{
	bb_free_timer.hour = bb_free_timer.minute = 0;
}

void bb_free_timer_add(void)
{
	if(bb_free_timer.minute >= 59)
	{
		bb_free_timer.minute = 0;
		bb_free_timer.hour++;
	}
	else
		bb_free_timer.minute++;
}

void bb_free_timer_sub(void)
{
	if(bb_free_timer.minute > 0)
	{
		bb_free_timer.minute--;
	}
	else if(bb_free_timer.hour > 0)
	{
		bb_free_timer.hour--;
		bb_free_timer.minute = 59;
	}
}

void bb_free_timer_update(void)
{
	if(bb_free_timer_up_counter)
		bb_free_timer_add();
	else
		bb_free_timer_sub();
}

void bb_free_timer_set_up_counter(bool dir)
{
	bb_free_timer_up_counter = dir;
	timer_1s = 0;
}

bool bb_free_timer_is_counting_up(void)
{
	return bb_free_timer_up_counter;
}

// do not put any HAL delay dependence here
void bb_scr_tick_1s(void)
{
	if(++timer_1s >= 60)
	{
		bb_free_timer_update();
		timer_1s = 0;
	}

	ctrl_sm_run_1s();
}

static uint8_t * bb_scr_insert_uint(uint8_t *buffer, uint32_t value, uint8_t n_digits, bool spaces)
{
    uint8_t* pbuf = buffer;
    uint8_t *pend;
    uint32_t shifter;
    bool filling = false;
    uint8_t digits[10] = {'0','1','2','3','4','5','6','7','8','9'};

    // count the number of digits
    shifter = value;
    do
    {
        ++pbuf;
        shifter = shifter / 10;
    } while(shifter);

    if(n_digits > 0)
    {
    	int8_t n = n_digits - (pbuf - buffer);
    	while(n-- > 0)
    		pbuf++;
    }

    pend = pbuf;

    // now fill the digits
    do
    {
    	if(filling && spaces)
    		*--pbuf = ' ';
    	else
    		*--pbuf = digits[value % 10];
        value = value / 10;

        filling = value == 0;

    } while(pbuf != buffer);

    *pend = '\0';

    return pend;
}

static uint8_t * bb_scr_insert_int(uint8_t *buffer, int32_t value, uint8_t n_digits, bool spaces)
{
    uint8_t* pbuf = buffer;

    // add signal, invert value and call the unsigned version
    if(value < 0)
    {
        *pbuf++ = '-';
        value *= -1;
    }

    return bb_scr_insert_uint(pbuf, (uint32_t) value, n_digits, spaces);
}

static uint8_t * bb_scr_insert_str(uint8_t *buffer, uint8_t *str)
{
	uint8_t *pbuf = buffer;
	uint8_t *pstr = str;
	uint16_t n = strlen((char *)str);

	while(n-- > 0)
		*pbuf++ = *pstr++;

	*pbuf = '\0';
	return pbuf;
}

static uint8_t * bb_scr_format_float1d(uint8_t *buf, int32_t f, uint8_t n_digits, bool spaces)
{
	uint8_t *pbuf;
	int32_t ip, fp;

	ip = f / 10;
	fp = f - ip * 10;

	pbuf = bb_scr_insert_int(buf,ip,n_digits,spaces);
	fp =  fp < 0 ? -fp : fp;
	*pbuf++ = '.';
	pbuf = bb_scr_insert_uint(pbuf,fp,0,false);

	return pbuf;
}

static char bb_scr_translate_hop(uint8_t number)
{
	return (number + 'A');
}

static void bb_scr_write_temp(int16_t temp, uint8_t line)
{
	uint8_t buf[23];
	uint8_t *pbuf = buf;

	SDD1306_Clear_Line(2);
	pbuf = bb_scr_format_float1d(buf,temp,0,false);
	*pbuf++ = BB_TEMP_UNIT_LABEL[0];
	*pbuf = '\0';
	SSD1306_Write_Centered_String(buf,2);
}

static void bb_scr_write_play_pause(bool pause, bool erase)
{
	SSD1306_Goto(120,0);

	if(erase)
	{
		uint8_t chr[] = { 32, 0 };
		SSD1306_Write_String(chr);

	}
	else if(pause)
	{
		uint8_t chr[] = { 129, 0 };
		SSD1306_Write_String(chr);
	}
	else
	{
		uint8_t chr[] = { 128, 0 };
		SSD1306_Write_String(chr);
	}

}
bb_scr_list_t bb_scr_temp(bool first_time, bb_key_event_t *key)
{
	bb_scr_list_t scr = BB_SCR_TEMP_ID;

	bb_time_t tm;
	bb_time_t last_tm = { UINT8_MAX, UINT8_MAX };
	uint8_t buf[23];
	uint8_t *pbuf = buf;
	int32_t temp;
	static int32_t last_temp = INT16_MIN;

	if(first_time)
	{
		bb_set_led_state(BB_LED_OFF);
		bb_free_timer_set_up_counter(true);
		SSD1306_Write_Centered_String((uint8_t*)BB_TEMP_LABEL,0);
		SSD1306_Write_Centered_String((uint8_t*)BB_TIME_LABEL,4);
	}

	if(key->id == BB_KEY_LEFT)
	{
		bb_set_led_state(BB_LED_OFF);
		scr = BB_SCR_OP_TEMP_ID;
	}
	else if(key->id == BB_KEY_RIGHT)
	{
		bb_set_led_state(BB_LED_OFF);
		bb_free_timer_set_up_counter(true);
		bb_free_timer_reset();
	}
	else if(key->id == BB_KEY_UP)
	{
		bb_free_timer_set_up_counter(false);
		bb_free_timer_add();
	}
	else if(key->id == BB_KEY_DOWN)
	{
		bb_free_timer_set_up_counter(false);
		bb_free_timer_sub();
	}

	temp = bb_temp_get1d();
	if(last_temp != temp || first_time)
	{
		bb_scr_write_temp(temp,2);
		last_temp = temp;
	}

	bb_free_timer_get(&tm);
	if(!bb_free_timer_is_counting_up() && tm.hour == 0 && tm.minute == 0)
	{
		bb_set_led_state(BB_LED_BLINK_NORMAL);
	}

	if(last_tm.hour != tm.hour || last_tm.minute != tm.minute || first_time)
	{
		pbuf = bb_scr_insert_uint(buf,tm.hour,2,false);
		*pbuf++ = ':';
		pbuf = bb_scr_insert_uint(pbuf,tm.minute,2,false);
		SSD1306_Write_Centered_String((uint8_t*)buf,6);
		last_tm = tm;
	}

	return scr;
}

bb_scr_list_t bb_scr_op_temp(bool first_time, bb_key_event_t *key)
{
	bb_scr_list_t scr = BB_SCR_OP_TEMP_ID;

	if(first_time)
		SSD1306_Write_Centered_String((uint8_t*)BB_OP_TEMP_LABEL,3);

	if(key->id == BB_KEY_UP)
		scr = BB_SCR_OP_AUTO_BOIL_ID;
	else if(key->id == BB_KEY_DOWN)
		scr = BB_SCR_OP_AUTO_MASH_ID;
	else if(key->id == BB_KEY_RIGHT)
		scr = BB_SCR_TEMP_ID;

	return scr;
}

bb_scr_list_t bb_scr_op_auto_mash(bool first_time, bb_key_event_t *key)
{
	bb_scr_list_t scr = BB_SCR_OP_AUTO_MASH_ID;

	if(first_time)
		SSD1306_Write_Centered_String((uint8_t*)BB_OP_AUTO_MASH_LABEL,3);

	if(key->id == BB_KEY_UP)
		scr = BB_SCR_OP_TEMP_ID;
	else if(key->id == BB_KEY_DOWN)
		scr = BB_SCR_OP_AUTO_BOIL_ID;
	else if(key->id == BB_KEY_RIGHT)
	{
		scr = BB_SCR_AUTO_SLOP_TEMP_ID;
		bb_scr_slope_idx = 0;
	}

	return scr;
}

bb_scr_list_t bb_scr_op_auto_boil(bool first_time, bb_key_event_t *key)
{
	bb_scr_list_t scr = BB_SCR_OP_AUTO_BOIL_ID;

	if(first_time)
		SSD1306_Write_Centered_String((uint8_t*)BB_OP_AUTO_BOIL_LABEL,3);

	if(key->id == BB_KEY_UP)
		scr = BB_SCR_OP_AUTO_MASH_ID;
	else if(key->id == BB_KEY_DOWN)
		scr = BB_SCR_OP_TEMP_ID;
	else if(key->id == BB_KEY_RIGHT)
	{
		bb_scr_slope_idx = 0;
		bb_scr_hop_idx = 0;
		scr = BB_SCR_AUTO_BOIL_TEMP_ID;
	}

	return scr;
}

bb_scr_list_t bb_scr_auto_slop_temp(bool first_time, bb_key_event_t *key)
{
	bb_scr_list_t scr = BB_SCR_AUTO_SLOP_TEMP_ID;
	uint8_t buf[23];
	uint8_t *pbuf;

	if(first_time)
	{
		ctrl_slope_get(bb_scr_slope_idx,&bb_scr_slope);
		SSD1306_Write_Centered_String((uint8_t*)BB_OP_AUTO_MASH_TITLE_LABEL,0);
		bb_scr_insert_uint(buf,bb_scr_slope_idx+1,0,false);
		SSD1306_Write_Centered_String(buf,2);
		SSD1306_Write_Centered_String((uint8_t*)BB_TEMP_LABEL,4);
	}

	if(key->id == BB_KEY_UP)
	{
		if(bb_scr_slope.temp < CTRL_MAX_TEMP)
			bb_scr_slope.temp++;
	}
	else if(key->id == BB_KEY_DOWN)
	{
		if(bb_scr_slope.temp > CTRL_MIN_TEMP)
			bb_scr_slope.temp--;
	}
	else if(key->id == BB_KEY_RIGHT)
	{
		ctrl_slope_set(bb_scr_slope_idx,&bb_scr_slope);
		scr = BB_SCR_AUTO_SLOP_DURA_ID;
	}
	else if(key->id == BB_KEY_LEFT)
	{
		ctrl_slope_set(bb_scr_slope_idx,&bb_scr_slope);
		if(bb_scr_slope_idx == 0)
			scr = BB_SCR_OP_AUTO_MASH_ID;
		else
		{
			bb_scr_slope_idx--;
			scr = BB_SCR_AUTO_SLOP_DURA_ID;
		}
	}

	pbuf = bb_scr_insert_uint(buf,bb_scr_slope.temp,0,false);
	*pbuf++ = BB_TEMP_UNIT_LABEL[0];
	*pbuf = '\0';
	SDD1306_Clear_Line(6);
	SSD1306_Write_Centered_String(buf,6);

	return scr;
}

bb_scr_list_t bb_scr_auto_slop_duration(bool first_time, bb_key_event_t *key)
{
	bb_scr_list_t scr = BB_SCR_AUTO_SLOP_DURA_ID;
	uint8_t buf[23];
	uint8_t *pbuf;

	if(first_time)
	{
		ctrl_slope_get(bb_scr_slope_idx,&bb_scr_slope);
		SSD1306_Write_Centered_String((uint8_t*)BB_OP_AUTO_MASH_TITLE_LABEL,0);
		bb_scr_insert_uint(buf,bb_scr_slope_idx+1,0,false);
		SSD1306_Write_Centered_String(buf,2);
		SSD1306_Write_Centered_String((uint8_t*)BB_DURATION_LABEL,4);
	}

	if(key->id == BB_KEY_UP)
	{
		if(bb_scr_slope.time_s < CTRL_MAX_TIME_S)
			bb_scr_slope.time_s += 60;
	}
	else if(key->id == BB_KEY_DOWN)
	{
		if(bb_scr_slope.time_s > 60)
			bb_scr_slope.time_s -= 60;
		else
			bb_scr_slope.time_s = 0;
	}
	else if(key->id == BB_KEY_RIGHT && !key->long_press)
	{
		if((bb_scr_slope_idx + 1) < CTRL_MAX_SLOPES)
		{
			ctrl_slope_set(bb_scr_slope_idx,&bb_scr_slope);
			bb_scr_slope_idx++;
			scr = BB_SCR_AUTO_SLOP_TEMP_ID;
		}
	}
	else if(key->id == BB_KEY_RIGHT && key->long_press)
	{
		ctrl_slope_set(bb_scr_slope_idx,&bb_scr_slope);
		ctrl_sm_start(bb_scr_slope_idx + 1, 0, false);
		scr = BB_SCR_AUTO_MASH_ID;
	}
	else if(key->id == BB_KEY_LEFT)
	{
		ctrl_slope_set(bb_scr_slope_idx,&bb_scr_slope);
		scr = BB_SCR_AUTO_SLOP_TEMP_ID;
	}

	pbuf = bb_scr_insert_uint(buf,bb_scr_slope.time_s/60,0,false);
	*pbuf++ = BB_DURATION_UNITY[0];
	*pbuf = '\0';
	SDD1306_Clear_Line(6);
	SSD1306_Write_Centered_String(buf,6);

	return scr;
}

bb_scr_list_t bb_scr_auto_mash(bool first_time, bb_key_event_t *key)
{
	bb_scr_list_t scr = BB_SCR_AUTO_MASH_ID;
	uint8_t buf[23];
	uint8_t *pbuf, *pbuf2;
	int32_t temp;
	uint8_t cur_slope;
	uint8_t num_slopes;
	uint16_t time_s;
	bool heating, paused;
	uint8_t n;
	ctrl_slope_t slope_info;
	static bool toggle;
	static int32_t last_temp = INT16_MIN;

	if(first_time)
	{
		toggle = true;
		bb_set_led_state(BB_LED_OFF);
		SSD1306_Write_Centered_String((uint8_t*)BB_OP_AUTO_MASH_LABEL,0);
		SSD1306_Write_Centered_String((uint8_t*)BB_AUTO_MASH_TITLE_LABEL,4);
		if(bb_temp_get_simulation())
			bb_temp_set_temp_simulation(250);
	}

	temp = bb_temp_get1d();

	if(last_temp != temp || first_time)
	{
		bb_scr_write_temp(temp,2);
		last_temp = temp;
	}

	if(key->id == BB_KEY_LEFT)
	{
		bb_set_led_state(BB_LED_OFF);
		if(ctrl_sm_get_cur_slope() == 0)
		{
			ctrl_sm_stop();
			scr = BB_SCR_OP_AUTO_MASH_ID;
		}
		else
			ctrl_sm_backward();
	}
	else if(key->id == BB_KEY_RIGHT)
	{
		if(ctrl_sm_pause_get())
		{
			ctrl_sm_pause_set(false);
		}
		else
		{
			bb_set_led_state(BB_LED_OFF);
			ctrl_sm_forward();
		}
	}
	else if(key->id == BB_KEY_UP || key->id == BB_KEY_DOWN)
	{
		ctrl_sm_pause_set(!ctrl_sm_pause_get());
	}

	if(ctrl_sm_finished())
	{
		scr = BB_SCR_AUTO_MASH_DONE_ID;
	}
	else
	{
		ctrl_curr_exec_data_get(&num_slopes,&cur_slope, &time_s, &slope_info, &heating, &paused);

		if(bb_temp_get_simulation() && !ctrl_sm_pause_get())
			bb_temp_set_temp_simulation_setpoint(slope_info.temp*10);

		if(paused && time_s == 0 && heating) // start or new slope
			bb_set_led_state(BB_LED_BLINK_NORMAL);
		else
			bb_set_led_state(BB_LED_OFF);

		pbuf = bb_scr_insert_uint(buf,cur_slope+1,2,true);
		*pbuf++ = '/';
		pbuf = bb_scr_insert_uint(pbuf,num_slopes,0,false);
		n = 7 - (pbuf - buf);
		while(n-- > 0)
			*pbuf++ = ' ';

		pbuf = bb_scr_insert_uint(pbuf,slope_info.temp,3,true);
		*pbuf++ = BB_TEMP_UNIT_LABEL[0];
		*pbuf++ = ' ';
		*pbuf++ = ' ';
		pbuf = bb_scr_insert_uint(pbuf,time_s/60,3,true);
		if(heating)
		{
			*pbuf++ = BB_DURATION_UNITY[0];
			pbuf = bb_scr_insert_str(pbuf,(uint8_t*)BB_AUTO_HEATING_LABEL);
		}
		else
		{
			*pbuf++ = '/';
			pbuf2 = pbuf;
			pbuf = bb_scr_insert_uint(pbuf,slope_info.time_s/60,0,false);
			*pbuf++ = BB_DURATION_UNITY[0];
			n = 4  - (pbuf - pbuf2);
			while(n-- > 0)
				*pbuf++ = ' ';
		}
		*pbuf = '\0';
		SSD1306_Goto(0,6);
		SSD1306_Write_String(buf);

		bb_scr_write_play_pause(ctrl_sm_pause_get(),toggle);
		toggle = !toggle;
	}

	return scr;
}

bb_scr_list_t bb_scr_auto_done_mash(bool first_time, bb_key_event_t *key)
{
	bb_scr_list_t scr = BB_SCR_AUTO_MASH_DONE_ID;
	int32_t temp;
	static int32_t last_temp = INT16_MIN;

	if(first_time)
	{
		bb_set_led_state(BB_LED_BLINK_FAST);
		SSD1306_Write_Centered_String((uint8_t*)BB_TEMP_LABEL,0);
		SSD1306_Write_Centered_String((uint8_t*)BB_AUTO_MESH_END_LABEL,4);
	}

	temp = bb_temp_get1d();

	if(last_temp != temp || first_time)
	{
		bb_scr_write_temp(temp,2);
		last_temp = temp;
	}


	if(key->id == BB_KEY_LEFT)
	{
		bb_set_led_state(BB_LED_OFF);
		scr = BB_SCR_OP_AUTO_MASH_ID;
	}
	return scr;
}

bb_scr_list_t bb_scr_auto_boil_temp(bool first_time, bb_key_event_t *key)
{
	bb_scr_list_t scr = BB_SCR_AUTO_BOIL_TEMP_ID;
	uint8_t buf[23];
	uint8_t *pbuf;

	if(first_time)
	{
		ctrl_boil_get(&bb_scr_boil);
		SSD1306_Write_Centered_String((uint8_t*)BB_OP_AUTO_BOIL_TITLE_LABEL,0);
		SSD1306_Write_Centered_String((uint8_t*)BB_TEMP_LABEL,4);
	}

	if(key->id == BB_KEY_UP)
	{
		if(bb_scr_boil.temp < CTRL_MAX_TEMP)
			bb_scr_boil.temp++;
	}
	else if(key->id == BB_KEY_DOWN)
	{
		if(bb_scr_boil.temp > CTRL_MIN_TEMP)
			bb_scr_boil.temp--;
	}
	else if(key->id == BB_KEY_RIGHT)
	{
		ctrl_boil_set(bb_scr_boil);
		scr = BB_SCR_AUTO_BOIL_DURA_ID;
	}
	else if(key->id == BB_KEY_LEFT)
	{
		ctrl_boil_set(bb_scr_boil);
		scr = BB_SCR_OP_AUTO_BOIL_ID;
	}

	pbuf = bb_scr_insert_uint(buf,bb_scr_boil.temp,0,false);
	*pbuf++ = BB_TEMP_UNIT_LABEL[0];
	*pbuf = '\0';
	SDD1306_Clear_Line(6);
	SSD1306_Write_Centered_String(buf,6);

	return scr;
}

bb_scr_list_t bb_scr_auto_boil_duration(bool first_time, bb_key_event_t *key)
{
	bb_scr_list_t scr = BB_SCR_AUTO_BOIL_DURA_ID;
	uint8_t buf[23];
	uint8_t *pbuf;

	if(first_time)
	{
		ctrl_boil_get(&bb_scr_boil);
		SSD1306_Write_Centered_String((uint8_t*)BB_OP_AUTO_BOIL_TITLE_LABEL,0);
		SSD1306_Write_Centered_String((uint8_t*)BB_DURATION_LABEL,4);
	}

	if(key->id == BB_KEY_UP)
	{
		if(bb_scr_boil.time_s < CTRL_MAX_TIME_S)
			bb_scr_boil.time_s += 60;
	}
	else if(key->id == BB_KEY_DOWN)
	{
		if(bb_scr_boil.time_s > 60)
			bb_scr_boil.time_s -= 60;
		else
			bb_scr_boil.time_s = 0;
	}
	else if(key->id == BB_KEY_RIGHT && !key->long_press)
	{
		ctrl_boil_set(bb_scr_boil);
		bb_scr_hop_idx = 0;
		scr = BB_SCR_AUTO_BOIL_HOP_ID;
	}
	else if(key->id == BB_KEY_RIGHT && key->long_press)
	{
		ctrl_boil_set(bb_scr_boil);
		bb_scr_hop_idx = 0;
		ctrl_sm_start(1, 0, true);
		scr = BB_SCR_AUTO_BOIL_ID;
	}
	else if(key->id == BB_KEY_LEFT)
	{
		ctrl_boil_set(bb_scr_boil);
		scr = BB_SCR_AUTO_BOIL_TEMP_ID;
	}

	pbuf = bb_scr_insert_uint(buf,bb_scr_boil.time_s/60,0,false);
	*pbuf++ = BB_DURATION_UNITY[0];
	*pbuf = '\0';
	SDD1306_Clear_Line(6);
	SSD1306_Write_Centered_String(buf,6);

	return scr;
}

bb_scr_list_t bb_scr_auto_boil_hop(bool first_time, bb_key_event_t *key)
{
	bb_scr_list_t scr = BB_SCR_AUTO_BOIL_HOP_ID;
	uint8_t buf[23];
	uint8_t *pbuf;

	if(first_time)
	{
		ctrl_hop_get(bb_scr_hop_idx,&bb_scr_hop);
		SSD1306_Write_Centered_String((uint8_t*)BB_OP_AUTO_BOIL_HOP_TITLE_LABEL,0);
		buf[0] = bb_scr_translate_hop(bb_scr_hop_idx);
		buf[1] = '\0';
		SSD1306_Write_Centered_String(buf,2);
		SSD1306_Write_Centered_String((uint8_t*)BB_DURATION_LABEL,4);
	}

	if(bb_scr_hop.time_s > bb_scr_boil.time_s)
		bb_scr_hop.time_s = bb_scr_boil.time_s;

	if(key->id == BB_KEY_UP)
	{
		if(bb_scr_hop.time_s < bb_scr_boil.time_s)
			bb_scr_hop.time_s += 60;
	}
	else if(key->id == BB_KEY_DOWN)
	{
		if(bb_scr_hop.time_s > 60)
			bb_scr_hop.time_s -= 60;
		else
			bb_scr_hop.time_s = 0;
	}
	else if(key->id == BB_KEY_RIGHT && !key->long_press)
	{
		if((bb_scr_hop_idx + 1) < CTRL_MAX_HOPS)
		{
			bb_scr_hop.number = bb_scr_hop_idx;
			ctrl_hop_set(bb_scr_hop_idx,&bb_scr_hop);
			bb_scr_hop_idx++;
			scr = BB_SCR_AUTO_BOIL_HOP_UPD_ID; // only to force an screen update
		}
	}
	else if(key->id == BB_KEY_RIGHT && key->long_press)
	{
		bb_scr_hop.number = bb_scr_hop_idx;
		ctrl_hop_set(bb_scr_hop_idx,&bb_scr_hop);
		ctrl_sm_start(1,bb_scr_hop_idx + 1, true);
		scr = BB_SCR_AUTO_BOIL_ID;
	}
	else if(key->id == BB_KEY_LEFT)
	{
		bb_scr_hop.number = bb_scr_hop_idx;
		ctrl_hop_set(bb_scr_hop_idx,&bb_scr_hop);
		if(bb_scr_hop_idx == 0)
			scr = BB_SCR_AUTO_BOIL_TEMP_ID;
		else
		{
			bb_scr_hop_idx--;
			scr = BB_SCR_AUTO_BOIL_HOP_UPD_ID; // only to force an screen update
		}
	}

	pbuf = bb_scr_insert_uint(buf,bb_scr_hop.time_s/60,0,false);
	*pbuf++ = BB_DURATION_UNITY[0];
	*pbuf = '\0';
	SDD1306_Clear_Line(6);
	SSD1306_Write_Centered_String(buf,6);

	return scr;
}

bb_scr_list_t bb_scr_auto_boil(bool first_time, bb_key_event_t *key)
{
	bb_scr_list_t scr = BB_SCR_AUTO_BOIL_ID;
	uint8_t buf[23];
	uint8_t *pbuf, *pbuf2;
	int32_t temp;
	uint16_t time_s, next_hop_time_s;
	bool heating, paused;
	uint8_t n, hop_list_size, num_hops, num_slopes, cur_slope;
	ctrl_slope_t slope_info;
	static bool toggle;
	static int32_t last_temp = INT16_MIN;
	uint8_t hop_list[CTRL_MAX_HOPS];

	ctrl_curr_exec_hop_data_get(&num_hops, &next_hop_time_s, hop_list, &hop_list_size);
	ctrl_curr_exec_data_get(&num_slopes,&cur_slope, &time_s, &slope_info, &heating, &paused);

	if(first_time)
	{
		toggle = true;
		bb_set_led_state(BB_LED_OFF);
		SSD1306_Write_Centered_String((uint8_t*)BB_OP_AUTO_BOIL_LABEL,0);
		SSD1306_Write_Centered_String((uint8_t*)BB_AUTO_BOILH_TITLE_LABEL,4);
		if(bb_temp_get_simulation())
		{
			bb_temp_set_temp_simulation(250);
			bb_temp_set_temp_simulation_setpoint(slope_info.temp*10);
		}
	}

	temp = bb_temp_get1d();

	if(last_temp != temp || first_time)
	{
		bb_scr_write_temp(temp,2);
		last_temp = temp;
	}

	if(key->id == BB_KEY_LEFT)
	{
		bb_set_led_state(BB_LED_OFF);
		ctrl_sm_stop();
		scr = BB_SCR_OP_AUTO_BOIL_ID;
	}
	else if(key->id == BB_KEY_UP || key->id == BB_KEY_DOWN)
	{
		if(ctrl_sm_waiting_hop())
		{
			bb_set_led_state(BB_LED_OFF);
			ctrl_hope_addition_done();
			ctrl_sm_pause_set(false);
		}
		else
			ctrl_sm_pause_set(!ctrl_sm_pause_get());
	}
	else if(key->id == BB_KEY_RIGHT)
	{
		bb_set_led_state(BB_LED_OFF);
		if(ctrl_sm_waiting_hop())
			ctrl_hope_addition_done();
	}

	if(ctrl_sm_finished())
	{
		scr = BB_SCR_AUTO_BOIL_DONE_ID;
	}
	else
	{
		if(heating)
		{
			pbuf = bb_scr_insert_uint(buf,time_s/60,3,true);
			*pbuf++ = BB_DURATION_UNITY[0];
			pbuf = bb_scr_insert_str(pbuf,(uint8_t*)BB_AUTO_HEATING_LABEL);
		}
		else
		{
			pbuf = bb_scr_insert_uint(buf,(slope_info.time_s - time_s + 59)/60,3,true);
			*pbuf++ = '/';
			pbuf2 = pbuf;
			pbuf = bb_scr_insert_uint(pbuf,slope_info.time_s/60,0,false);
			*pbuf++ = BB_DURATION_UNITY[0];
			n = 4  - (pbuf - pbuf2);
			while(n-- > 0)
				*pbuf++ = ' ';
		}

		if(ctrl_sm_waiting_hop())
		{
			bb_set_led_state(BB_LED_BLINK_NORMAL);
			SSD1306_Write_Centered_String((uint8_t*)BB_AUTO_BOILHA_TITLE_LABEL,4);

			n = 21 - (8 + hop_list_size);
			while(n-- > 0)
				*pbuf++ = ' ';

			for(n = 0 ; n < hop_list_size ; n++)
				*pbuf++ = bb_scr_translate_hop(hop_list[n]);

			bb_scr_write_play_pause(true,toggle);
		}
		else
		{
			bb_set_led_state(BB_LED_OFF);
			SSD1306_Write_Centered_String((uint8_t*)BB_AUTO_BOILH_TITLE_LABEL,4);

			n = 9;
			while(n-- > 0)
				*pbuf++ = ' ';

			if(hop_list_size)
			{
				int32_t p = (next_hop_time_s - time_s + 59)/60;
				if(p > 0)
				{
					pbuf = bb_scr_insert_uint(pbuf,p,3,true);
					*pbuf++ = BB_DURATION_UNITY[0];
				}
				else
				{
					*pbuf++ = ' ';
					*pbuf++ = ' ';
					*pbuf++ = ' ';
					*pbuf++ = ' ';
				}
			}
			else
			{
				*pbuf++ = ' ';
				*pbuf++ = ' ';
				*pbuf++ = '-';
				*pbuf++ = '-';
			}

			bb_scr_write_play_pause(ctrl_sm_pause_get(),toggle);
		}

		*pbuf = '\0';
		SSD1306_Goto(0,6);
		SSD1306_Write_String(buf);

		toggle = !toggle;
	}
	return scr;
}

bb_scr_list_t bb_scr_auto_boil_upd_hop(bool first_time, bb_key_event_t *key)
{
	bb_scr_list_t scr = BB_SCR_AUTO_BOIL_HOP_ID;

	return scr;
}

bb_scr_list_t bb_scr_auto_boil_done(bool first_time, bb_key_event_t *key)
{
	bb_scr_list_t scr = BB_SCR_AUTO_BOIL_DONE_ID;
	int32_t temp;
	static int32_t last_temp = INT16_MIN;

	if(first_time)
	{
		bb_set_led_state(BB_LED_BLINK_FAST);
		SSD1306_Write_Centered_String((uint8_t*)BB_TEMP_LABEL,0);
		SSD1306_Write_Centered_String((uint8_t*)BB_AUTO_BOIL_END_LABEL,4);
	}

	temp = bb_temp_get1d();

	if(last_temp != temp || first_time)
	{
		bb_scr_write_temp(temp,2);
		last_temp = temp;
	}


	if(key->id == BB_KEY_LEFT)
	{
		bb_set_led_state(BB_LED_OFF);
		scr = BB_SCR_OP_AUTO_BOIL_ID;
	}
	return scr;
}

bb_scr_list_t bb_scr_splash(bool first_time, bb_key_event_t *key)
{
	bb_scr_list_t scr = BB_SCR_SPLASH_ID;

	if(first_time)
	{
		SSD1306_Write_Centered_String((uint8_t*)BB_SPLASH1,1);
		SSD1306_Write_Centered_String((uint8_t*)BB_SPLASH2,3);
		SSD1306_Write_Centered_String((uint8_t*)BB_VERSION,6);
		if(bb_temp_get_simulation())
			SSD1306_Write_Centered_String((uint8_t*)BB_SIMUL,4);
	}
	else
		scr = BB_SCR_TEMP_ID;

	HAL_Delay(500);

	return scr;
}

bool bb_scr_update(bb_key_event_t *key)
{
	static bb_scr_list_t bb_scr = BB_SCR_SPLASH_ID;
	static bb_scr_list_t bb_last_scr = BB_SCR_OP_AUTO_MASH_ID; // force update when initializing

	bool first_time = bb_last_scr == bb_scr ? false : true;
	bb_last_scr = bb_scr;

	if(first_time)
		SDD1306_Clear_Screen();

	bb_scr = scr_funcs[bb_scr](first_time,key);

	// we need to clear the key to indicate it was handled
	key->id = BB_KEY_EMPTY;

	return !(bb_scr == bb_last_scr);
}

void bb_scr_init(void)
{
	timer_1s = 0;
	bb_free_timer_reset();
}


