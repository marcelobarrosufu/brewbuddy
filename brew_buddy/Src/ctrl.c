/*
  BrewBuddy by Marcelo Barros de Almeida is licensed under a
  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.

  For commercial usage, please contact the author via marcelobarrosalmeida@gmail.com
*/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "bb.h"
#include "ctrl.h"
#include "persist.h"

#define CTRL_TEMP_SLACK 5

#define XMACRO_CTRL_LIST \
    X(CTRL_IDLE_STATE,     ctrl_idle_state) \
    X(CTRL_HEATING_STATE,  ctrl_heating_state) \
    X(CTRL_STEADY_STATE,   ctrl_mashing_state) \
	X(CTRL_WAIT_HOP_STATE, ctrl_wait_hop_state) \
    X(CTRL_FINISHED_STATE, ctrl_finished_state)

typedef enum ctrl_state_e
{
#define X(CTRL_ENUM_NAME, CTRL_FUNC_NAME) CTRL_ENUM_NAME,
	XMACRO_CTRL_LIST
#undef X
} ctrl_state_t;

typedef struct ctrl_slope_sm_s
{
	bool paused;
	bool is_boil;
	int32_t temp_avg; // expon. moving. average with n = 9
	uint8_t num_slopes;
	uint8_t current_slope;
	uint16_t time_s;
	uint8_t num_hops;
	uint8_t current_hop;
	uint8_t num_hops_additions;
	uint8_t next_hops_additions;
	ctrl_state_t state;
	ctrl_state_t last_state;
	ctrl_slope_t boil;
	ctrl_slope_t slopes[CTRL_MAX_SLOPES];
	ctrl_hop_t hops[CTRL_MAX_HOPS];
	ctrl_hop_t next_hops[CTRL_MAX_HOPS];
	uint16_t hops_time_s[CTRL_MAX_HOPS];
} ctrl_slope_sm_t;


typedef ctrl_state_t (*ctrl_slope_func_t)(ctrl_slope_sm_t *sm);

#define X(CTRL_ENUM_NAME, CTRL_FUNC_NAME) \
	ctrl_state_t CTRL_FUNC_NAME(ctrl_slope_sm_t *sm);
		XMACRO_CTRL_LIST
#undef X

ctrl_slope_func_t ctrl_slope_sm_funcs[] =
{
#define X(CTRL_ENUM_NAME, CTRL_FUNC_NAME) CTRL_FUNC_NAME,
		XMACRO_CTRL_LIST
#undef X
};

ctrl_slope_sm_t sm;

ctrl_state_t ctrl_idle_state(ctrl_slope_sm_t *sm)
{
	ctrl_state_t next_state = CTRL_IDLE_STATE;
	sm->time_s = 0;
	sm->paused = false;
	return next_state;
}

ctrl_state_t ctrl_heating_state(ctrl_slope_sm_t *sm)
{
	ctrl_state_t next_state = CTRL_HEATING_STATE;
	int32_t temp;
	int32_t slope_temp;

	if(sm->is_boil)
		slope_temp = sm->boil.temp;
	else
		slope_temp = sm->slopes[sm->current_slope].temp;

	sm->time_s++;

	temp = bb_temp_get1d();
	sm->temp_avg = (sm->temp_avg*8 + temp*2)/10;

	if(sm->temp_avg - (slope_temp*10 - CTRL_TEMP_SLACK) >= 0)
	{
		sm->time_s = 0;
		next_state = CTRL_STEADY_STATE;
	}

	return next_state;
}

ctrl_state_t ctrl_mashing_state(ctrl_slope_sm_t *sm)
{
	ctrl_state_t next_state = CTRL_STEADY_STATE;
	uint16_t time_s;

	if(sm->num_hops)
	{
		if((sm->next_hops_additions < sm->num_hops_additions) &&
		   (sm->time_s == sm->hops_time_s[sm->next_hops_additions]))
		{
			return CTRL_WAIT_HOP_STATE;
		}
	}

	sm->time_s++;

	if(sm->is_boil)
		time_s = sm->boil.time_s;
	else
		time_s = sm->slopes[sm->current_slope].time_s;

	if(sm->time_s >= time_s)
	{
		if((sm->current_slope + 1) >= sm->num_slopes)
		{
			next_state = CTRL_FINISHED_STATE;
		}
		else
		{
			sm->time_s = 0;
			sm->temp_avg = 0;
			sm->current_slope++;
			sm->paused = true;
			next_state = CTRL_HEATING_STATE;
		}
	}

	return next_state;
}

ctrl_state_t ctrl_wait_hop_state(ctrl_slope_sm_t *sm)
{
	ctrl_state_t next_state = CTRL_WAIT_HOP_STATE;

	return next_state;
}

ctrl_state_t ctrl_finished_state(ctrl_slope_sm_t *sm)
{
	ctrl_state_t next_state = CTRL_FINISHED_STATE;

	return next_state;
}

void ctrl_boil_get(ctrl_slope_t *boil)
{
	*boil = sm.boil;
}

void ctrl_boil_set(ctrl_slope_t boil)
{
	sm.boil = boil;
}

void ctrl_slope_get(uint8_t n, ctrl_slope_t *slope)
{
	if(sm.is_boil)
		*slope = sm.boil;
	else
		*slope = sm.slopes[n];
}

void ctrl_slope_set(uint8_t n, ctrl_slope_t *slope)
{
	sm.slopes[n].temp = slope->temp;
	sm.slopes[n].time_s = slope->time_s;
}

void ctrl_hop_get(uint8_t n, ctrl_hop_t *hop)
{
	hop->number = sm.hops[n].number;
	hop->time_s = sm.hops[n].time_s;
}

void ctrl_hop_set(uint8_t n, ctrl_hop_t *hop)
{
	sm.hops[n].number = hop->number;
	sm.hops[n].time_s = hop->time_s;
}

void ctrl_sm_run_1s(void)
{
	if(!sm.paused)
		sm.state = ctrl_slope_sm_funcs[sm.state](&sm);
}

void ctrl_sm_stop(void)
{
	sm.state = CTRL_IDLE_STATE;
}

void ctrl_sm_backward(void)
{
	sm.paused = false;

	if(sm.current_slope > 0)
	{
		sm.current_slope--;
		sm.state = CTRL_HEATING_STATE;
		sm.time_s = 0;
		sm.temp_avg = 0;
	}
}

void ctrl_sm_forward(void)
{
	sm.paused = false;

	if((sm.current_slope + 1) < sm.num_slopes)
	{
		sm.current_slope++;
		sm.state = CTRL_HEATING_STATE;
		sm.time_s = 0;
		sm.temp_avg = 0;
	}
	else
	{
		sm.state = CTRL_FINISHED_STATE;
	}
}

void ctrl_sm_pause_set(bool paused)
{
	sm.paused = paused;
}

bool ctrl_sm_pause_get(void)
{
	return sm.paused;
}

static void ctrl_hops_pre_processing(void)
{
	uint8_t n, m;

	// sorting, decreasing order
	for(n = 1 ; n < sm.num_hops ; n++)
	{
		for(m = (sm.num_hops - 1) ; m >= n ; m--)
		{
			if(sm.hops[m].time_s > sm.hops[m-1].time_s)
			{
				ctrl_hop_t tmp = sm.hops[m];
				sm.hops[m] = sm.hops[m-1];
				sm.hops[m-1] = tmp;
			}
		}
	}

	// generate hopping scheduling
	sm.num_hops_additions = 0;
	for(n = 0 ; n < sm.num_hops ; n++)
	{
		sm.hops_time_s[sm.num_hops_additions] = sm.boil.time_s - sm.hops[n].time_s;

		if(n == 0)
			sm.num_hops_additions++;
		else if(sm.hops_time_s[sm.num_hops_additions] != sm.hops_time_s[sm.num_hops_additions-1])
			sm.num_hops_additions++;
	}

	sm.next_hops_additions = 0;
}

void ctrl_sm_start(uint8_t num_slopes, uint8_t num_hops, bool is_boil)
{
	sm.state = CTRL_HEATING_STATE;

	sm.current_slope = 0;
	sm.current_hop = 0;
	sm.time_s = 0;
	sm.temp_avg = 0;
	sm.paused = false;
	sm.is_boil = is_boil;
	sm.num_slopes = num_slopes;
	sm.num_hops = num_hops;

	persist_save(sm.num_slopes,sm.slopes,sm.num_hops,sm.hops,sm.boil);

	if(sm.num_hops)
		ctrl_hops_pre_processing();
}

bool ctrl_sm_waiting_hop(void)
{
	return sm.state == CTRL_WAIT_HOP_STATE;
}

void ctrl_hope_addition_done(void)
{
	sm.time_s++;
	sm.next_hops_additions++;
	sm.state = CTRL_STEADY_STATE;
}

bool ctrl_sm_finished(void)
{
	return sm.state == CTRL_FINISHED_STATE;
}

void ctrl_curr_exec_hop_data_get(uint8_t *num_hops, uint16_t *next_hop_time_s, uint8_t *hop_list, uint8_t *hop_list_size)
{
	uint8_t m, k;

	*num_hops = sm.num_hops;

	if(sm.next_hops_additions >= sm.num_hops_additions)
	{
		*next_hop_time_s = 0;
		*hop_list_size = 0;
	}
	else
	{
		*next_hop_time_s = sm.hops_time_s[sm.next_hops_additions];
		if(hop_list)
		{
			for(m = 0, k = 0 ; m < sm.num_hops ; m++)
			{
				if(sm.hops_time_s[sm.next_hops_additions] == (sm.boil.time_s - sm.hops[m].time_s))
					hop_list[k++] = sm.hops[m].number;
			}
			*hop_list_size = k;
		}
	}
}

void ctrl_curr_exec_data_get(uint8_t *num_slopes, uint8_t *cur_slope, uint16_t *time_s, ctrl_slope_t *slope_info, bool *heating, bool *paused)
{
	*num_slopes = sm.num_slopes;
	*cur_slope = sm.current_slope;
	*time_s = sm.time_s;
	ctrl_slope_get(*cur_slope,slope_info);
	*heating = sm.state == CTRL_HEATING_STATE;
	*paused = sm.paused;
}

uint8_t ctrl_sm_get_cur_slope(void)
{
	return sm.current_slope;
}

void ctrl_init(void)
{
	memset(&sm,0,sizeof(ctrl_slope_sm_t));
	sm.state = CTRL_IDLE_STATE;

	persist_load(&sm.num_slopes, sm.slopes,&sm.num_hops,sm.hops,&sm.boil);
}
