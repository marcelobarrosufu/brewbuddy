/*
  BrewBuddy by Marcelo Barros de Almeida is licensed under a
  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.

  For commercial usage, please contact the author via marcelobarrosalmeida@gmail.com
*/

#ifndef BB_H_
#define BB_H_

#define BB_KEY_DEBOUNCE_TIME_MS         50
#define BB_KEY_LONG_PRESS_TIME_MS       1000
#define BB_KEY_CONTINUOUS_PRESS_TIME_MS 2000

typedef enum bb_key_id_e
{
	BB_KEY_EMPTY = 0x00,
	BB_KEY_UP    = 0x08, //GPIO_PIN_0,
	BB_KEY_DOWN  = 0x04, //GPIO_PIN_1,
	BB_KEY_LEFT  = 0x02, //GPIO_PIN_2,
	BB_KEY_RIGHT = 0x01, //GPIO_PIN_3,
} bb_key_id_t;

typedef struct bb_key_event_s
{
	bb_key_id_t id;
	bool long_press;
	bool continuous_press;
	uint32_t fall_time_ms;
	uint32_t elapsed_time_ms;
} bb_key_event_t;

typedef enum bb_sys_id_e
{
	BB_SYS_EMPTY = 0,
	BB_SYS_UPD_TEMP = 1,
	BB_SYS_UPD_SCR = 2,
} bb_sys_id_t;

typedef struct bb_sys_event_s
{
	bb_sys_id_t id;
} bb_sys_event_t;

typedef enum bb_event_type_e
{
	BB_EVENT_EMPTY = 0,
	BB_EVENT_KEY = 1,
	BB_EVENT_SYS = 2,
} bb_event_type_t;

typedef enum bb_led_state_s
{
	BB_LED_OFF = 0,
	BB_LED_ON,
	BB_LED_BLINK_SLOW,
	BB_LED_BLINK_NORMAL,
	BB_LED_BLINK_FAST,
} bb_led_state_t;

typedef struct bb_event_s
{
	union
	{
		bb_sys_event_t sys;
		bb_key_event_t key;
	} event;
	bb_event_type_t type;
} bb_event_t;

typedef struct bb_time_s
{
	uint8_t hour;
	uint8_t minute;
} bb_time_t;

bool bb_init(void);
void bb_pendsv_update(void);
int32_t bb_temp_get1d(void);
void bb_free_timer_get(bb_time_t *tm);
void bb_free_timer_reset(void);
int32_t bb_temp_get1d(void);
void bb_set_led_state(bb_led_state_t state);

bool bb_temp_get_simulation(void);
void bb_temp_set_simulation(bool state);
void bb_temp_set_temp_simulation(int32_t temp);
void bb_temp_set_temp_simulation_setpoint(int32_t temp);

#endif /* BB_H_ */
