/*
  BrewBuddy by Marcelo Barros de Almeida is licensed under a
  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.

  For commercial usage, please contact the author via marcelobarrosalmeida@gmail.com
*/

#ifndef CTRL_H_
#define CTRL_H_

#define CTRL_MAX_SLOPES 10
#define CTRL_MAX_HOPS   10
#if BB_TEMP_UNIT_IS_CELSIUS == 1
	#define CTRL_MAX_TEMP   105
	#define CTRL_MIN_TEMP   0
#else
	#define CTRL_MAX_TEMP   221
	#define CTRL_MIN_TEMP   32
#endif
#define CTRL_MAX_TIME_S (60*200)

typedef struct ctrl_slope_s
{
	uint16_t time_s;
	uint8_t temp;
} ctrl_slope_t;

typedef struct ctrl_hop_s
{
	uint16_t time_s;
	uint8_t number;
} ctrl_hop_t;

void ctrl_init(void);
void ctrl_sm_run_1s(void);
void ctrl_boil_get(ctrl_slope_t *boil);
void ctrl_boil_set(ctrl_slope_t boil);
void ctrl_slope_get(uint8_t n, ctrl_slope_t *slope);
void ctrl_slope_set(uint8_t n, ctrl_slope_t *slope);
void ctrl_sm_start(uint8_t num_slopes, uint8_t num_hops, bool is_boil);
void ctrl_curr_exec_hop_data_get(uint8_t *num_hops, uint16_t *next_hop_time, uint8_t *hop_list, uint8_t *hop_list_size);
void ctrl_curr_exec_data_get(uint8_t *num_slopes, uint8_t *cur_slope, uint16_t *time_s, ctrl_slope_t *slope_info, bool *heating, bool *paused);
bool ctrl_sm_finished(void);
void ctrl_sm_pause_set(bool paused);
bool ctrl_sm_pause_get(void);
void ctrl_sm_stop(void);
void ctrl_sm_forward(void);
void ctrl_sm_backward(void);
uint8_t ctrl_sm_get_cur_slope(void);
void ctrl_hop_get(uint8_t n, ctrl_hop_t *hop);
void ctrl_hop_set(uint8_t n, ctrl_hop_t *hop);
bool ctrl_sm_waiting_hop(void);
void ctrl_hope_addition_done(void);

#endif /* CTRL_H_ */
