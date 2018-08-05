/*
  BrewBuddy by Marcelo Barros de Almeida is licensed under a
  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.

  For commercial usage, please contact the author via marcelobarrosalmeida@gmail.com
*/

#include <bb_locale.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "stm32f0xx_hal.h"
#include "ssd1306_fonts.h"
#include "ssd1306.h"
#include "bb.h"
#include "bb_scr.h"
#include "main.h"
#include "ctrl.h"
#include "lmt86.h"
#include "persist.h"

enum {
	BB_EVENT_KEY_IDX = 0,
	BB_EVENT_SYS_IDX = 1,
	BB_EVENT_CONTINUOUS_KEY_IDX = 2,
	BB_MAX_EVENTS = 3,
};

typedef enum
{
	BB_ADC_TEMP = ADC_CHANNEL_5,
	BB_ADC_2V5 = ADC_CHANNEL_6
} bb_adc_channels_t;

#define TEMP_UPDATE_SCR_MS 600
#define TEMP_UPDATE_TMP_MS 50
#define LED_PROCESS_MS     50

extern ADC_HandleTypeDef hadc;
extern IWDG_HandleTypeDef hiwdg;

void bb_handle_events(bb_event_t *key);

bb_event_t bb_events[BB_MAX_EVENTS];
volatile uint32_t bb_temp_raw;
volatile uint32_t bb_continous_press_ms;
volatile bb_led_state_t bb_led_state = BB_LED_OFF;
volatile uint8_t bb_led_cnt = 0;
volatile bb_adc_channels_t bb_channel = BB_ADC_2V5;
volatile uint16_t bb_2v5_ref = 3100;
volatile bool bb_simulation = false;
volatile int32_t bb_temp_simulation = 0;
volatile int32_t bb_temp_simulation_setpoint = 0;
uint16_t bb_simulation_speed_factor = 1;

bool bb_temp_get_simulation(void)
{
	return bb_simulation;
}

void bb_temp_set_simulation(bool state)
{
	bb_simulation = state;
}

void bb_temp_set_temp_simulation(int32_t temp)
{
	bb_temp_simulation = temp;
}

void bb_temp_set_temp_simulation_setpoint(int32_t temp)
{
	bb_temp_simulation_setpoint = temp;
}

int32_t bb_temp_get1d(void)
{
	if(bb_temp_get_simulation())
	{
		static uint8_t inc = 0;
		if(bb_temp_simulation < bb_temp_simulation_setpoint)
		{
			if(++inc >= 3)
			{
				bb_temp_simulation += 1;
				inc = 0;
			}
		}
		return bb_temp_simulation;
	}
	else
	{
		uint32_t vref = (2500*4095)/bb_2v5_ref;
		return lmt86_conv_temp_1d(bb_temp_raw, 4095, vref);
	}
}

static void bb_update_trigger(void)
{
	SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
}

static void bb_led_set(bool state)
{
	GPIO_PinState v = state ? GPIO_PIN_SET : GPIO_PIN_RESET;

	HAL_GPIO_WritePin(GPIOF, GPIO_PIN_0, v);
}

void bb_set_led_state(bb_led_state_t new_state)
{
	if(new_state > BB_LED_BLINK_FAST)
		return;

	if(new_state != bb_led_state)
	{
		bb_led_state = new_state;
		bb_led_cnt = 0;
	}
}

void bb_led_process(void)
{
	struct bb_led_states_s
	{
		uint8_t ticks_on_50ms;
		uint8_t ticks_off_50ms;
	};

	struct bb_led_states_s led_states[] =
	{
		{0,1},
		{1,0},
		{400/LED_PROCESS_MS,(400+500)/LED_PROCESS_MS},
		{200/LED_PROCESS_MS,(200+300)/LED_PROCESS_MS},
		{100/LED_PROCESS_MS,(100+200)/LED_PROCESS_MS},
	};

	bb_led_set(bb_led_cnt < led_states[bb_led_state].ticks_on_50ms);

	if(++bb_led_cnt > led_states[bb_led_state].ticks_off_50ms)
		bb_led_cnt = 0;
}

uint32_t HAL_Elapsed_Time(uint32_t tmr_old_ms, uint32_t tmr_new_ms)
{
    uint32_t elapsed_ms;

    if(tmr_new_ms < tmr_old_ms)
        elapsed_ms = UINT32_MAX - tmr_old_ms + tmr_new_ms + 1;
    else
        elapsed_ms = tmr_new_ms - tmr_old_ms;

    return elapsed_ms;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	bool fall;
	uint32_t time_ms;

	if(GPIO_Pin < BB_KEY_RIGHT || GPIO_Pin > BB_KEY_UP)
		return;

	// only process one key per time
	if(bb_events[BB_EVENT_KEY_IDX].event.key.id != BB_KEY_EMPTY && bb_events[BB_EVENT_KEY_IDX].event.key.id != GPIO_Pin)
		return;

	time_ms = HAL_GetTick();
	fall = HAL_GPIO_ReadPin(GPIOA,GPIO_Pin) == GPIO_PIN_RESET;
	bb_events[BB_EVENT_KEY_IDX].event.key.elapsed_time_ms = HAL_Elapsed_Time(bb_events[BB_EVENT_KEY_IDX].event.key.fall_time_ms,time_ms);

	// ignore two successive pressings (debouncing)
	if(fall)
	{
		if(bb_events[BB_EVENT_KEY_IDX].event.key.elapsed_time_ms < BB_KEY_DEBOUNCE_TIME_MS)
			return;

		if(GPIO_Pin == BB_KEY_UP || GPIO_Pin == BB_KEY_DOWN)
			bb_continous_press_ms = BB_KEY_CONTINUOUS_PRESS_TIME_MS;
		else
			bb_continous_press_ms = 0;

		bb_events[BB_EVENT_KEY_IDX].event.key.fall_time_ms = time_ms;
		bb_events[BB_EVENT_KEY_IDX].event.key.id = GPIO_Pin;
	}
	else
	{
		bb_continous_press_ms = 0;
		bb_events[BB_EVENT_KEY_IDX].type = BB_EVENT_KEY;
		bb_events[BB_EVENT_KEY_IDX].event.key.long_press = bb_events[BB_EVENT_KEY_IDX].event.key.elapsed_time_ms > BB_KEY_LONG_PRESS_TIME_MS;
		bb_update_trigger();
	}
}

void HAL_SYSTICK_Callback(void)
{
	static uint16_t temp_update_scr_ms = 0;
	static uint16_t temp_update_tmp_ms = 0;
	static uint32_t time_update_1s = 0;
	static uint32_t time_proc_led_ms = 0;

	if(++time_proc_led_ms >= LED_PROCESS_MS)
	{
		bb_led_process();
		time_proc_led_ms = 0;
	}

	if(++time_update_1s >= (1000/bb_simulation_speed_factor))
	{
		bb_scr_tick_1s();
		time_update_1s = 0;
	}

	if(++temp_update_scr_ms >= TEMP_UPDATE_SCR_MS)
	{
		temp_update_scr_ms = 0;

		if(bb_events[BB_EVENT_SYS_IDX].type == BB_EVENT_EMPTY)
			bb_events[BB_EVENT_SYS_IDX].event.sys.id = BB_SYS_UPD_SCR;

		bb_events[BB_EVENT_SYS_IDX].type = BB_EVENT_SYS;
		bb_update_trigger();
	}

	if(++temp_update_tmp_ms >= TEMP_UPDATE_TMP_MS)
	{
		temp_update_tmp_ms = 0;

		if(bb_events[BB_EVENT_SYS_IDX].type == BB_EVENT_EMPTY)
			bb_events[BB_EVENT_SYS_IDX].event.sys.id = BB_SYS_UPD_TEMP;

		bb_events[BB_EVENT_SYS_IDX].type = BB_EVENT_SYS;
		bb_update_trigger();
	}

	if(bb_continous_press_ms > 0)
	{
		if(bb_continous_press_ms > 1)
		{
			bb_continous_press_ms--;
		}
		else
		{
			bb_continous_press_ms = 50;
			bb_events[BB_EVENT_CONTINUOUS_KEY_IDX] = bb_events[BB_EVENT_KEY_IDX];
			bb_events[BB_EVENT_CONTINUOUS_KEY_IDX].type = BB_EVENT_KEY;
			bb_events[BB_EVENT_CONTINUOUS_KEY_IDX].event.key.long_press = false;
			bb_events[BB_EVENT_CONTINUOUS_KEY_IDX].event.key.continuous_press = true;
			bb_update_trigger();
		}
	}
}

static void bb_configure_channel(bb_adc_channels_t channel)
{
	ADC_ChannelConfTypeDef sConfig;

	// remove all
	hadc.Instance->CHSELR = 0;

	// configure
	sConfig.Channel = channel;
	sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
	HAL_ADC_ConfigChannel(&hadc, &sConfig);

	bb_channel = channel;
}

inline void bb_pendsv_update(void)
{
	for(uint8_t n = 0 ; n < BB_MAX_EVENTS ; n++)
		bb_handle_events(&bb_events[n]);
}

static void bb_update_temperature(void)
{
	static uint16_t cal = 0;
#if 0
	if(HAL_ADC_PollForConversion(&hadc, 50) == HAL_OK)
	{
		bb_temp_raw = HAL_ADC_GetValue(&hadc);
	}
#else
	if(++cal > (30000/TEMP_UPDATE_TMP_MS))
	{
		cal = 0;
		HAL_ADC_Stop(&hadc);
		HAL_ADCEx_Calibration_Start(&hadc);
	}
	HAL_ADC_Start_IT(&hadc);
#endif

}

#define NUM_ADC_SAMPLES 50

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* h)
{
	static uint8_t first_samples = 0;
	static uint16_t num_samples = 0;
	static uint32_t tmp_sum = 0;

	if(bb_channel == BB_ADC_2V5)
	{
		bb_2v5_ref = HAL_ADC_GetValue(h);
		bb_configure_channel(BB_ADC_TEMP);
	}
	else if(bb_channel == BB_ADC_TEMP)
	{
		if(first_samples < 5)
		{
			// the temperature sensor shows low values for first samples so we will start
			// the moving average filter only after five samples or it will take several
			// seconds to converge
			bb_temp_raw = HAL_ADC_GetValue(h) & 0xFFFE; // discarding LSB bit (only noise)
			tmp_sum = 0;
			first_samples++;
		}
		else
		{
			// discarding LSB bit (only noise)
			tmp_sum += HAL_ADC_GetValue(h) & 0xFFFE;

			if(++num_samples >= NUM_ADC_SAMPLES)
			{
	#if 0
				bb_temp_raw = tmp_sum/NUM_ADC_SAMPLES;
	#else
				tmp_sum = tmp_sum/NUM_ADC_SAMPLES;
				bb_temp_raw = (bb_temp_raw*75 + tmp_sum*25)/100;
	#endif
				num_samples = 0;
				tmp_sum = 0;
				bb_configure_channel(BB_ADC_2V5);
			}
		}
	}

	HAL_ADC_Stop_IT(&hadc);
}

//void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
//{
//	hadc = hadc + 0;
//}

void bb_handle_events(bb_event_t *event)
{
	if(event->type == BB_EVENT_KEY)
	{
		bb_key_event_t *key = &(event->event.key);

		HAL_NVIC_DisableIRQ(EXTI0_1_IRQn);
		HAL_NVIC_DisableIRQ(EXTI2_3_IRQn);

		if(bb_scr_update(key))
		{
			// force an immediate screen update when current screen ID changes
			if(bb_events[BB_EVENT_SYS_IDX].type == BB_EVENT_EMPTY)
			{
				bb_events[BB_EVENT_SYS_IDX].type = BB_EVENT_SYS;
				bb_events[BB_EVENT_SYS_IDX].event.sys.id = BB_SYS_UPD_SCR;
				// when looping over items on main menu we will have a watchdog as
				// we are continuously generating events and processing
				HAL_IWDG_Refresh(&hiwdg);
				bb_update_trigger();
			}
		}
		else
		{
			// we need to clear to indicate it was handled
			event->type = BB_EVENT_EMPTY;
		}

		HAL_NVIC_EnableIRQ(EXTI2_3_IRQn);
		HAL_NVIC_EnableIRQ(EXTI0_1_IRQn);
	}
	else if(event->type == BB_EVENT_SYS)
	{
		bb_sys_event_t *sys = &(event->event.sys);

		if(sys->id == BB_SYS_UPD_TEMP)
		{
			bb_update_temperature();
		}
		else if(sys->id == BB_SYS_UPD_SCR)
		{
			bb_key_event_t key = { .id = BB_KEY_EMPTY };
			bb_scr_update(&key);
		}

		// we need to clear to indicate it was handled
		event->type = BB_EVENT_EMPTY;
	}
}


bool bb_init(void)
{
	memset(&bb_events[0],0,sizeof(bb_events));
	bb_temp_raw = 0;
	bb_continous_press_ms = 0;
	bb_simulation_speed_factor = 1;

	if(HAL_GPIO_ReadPin(GPIOA,BB_KEY_LEFT) == GPIO_PIN_RESET)
	{
		bb_temp_set_simulation(true);
		bb_temp_set_temp_simulation(250);
		bb_temp_set_temp_simulation_setpoint(250);
		bb_simulation_speed_factor = 60;
	}

	bb_configure_channel(BB_ADC_2V5);
	HAL_ADCEx_Calibration_Start(&hadc);
	HAL_ADC_Start_IT(&hadc);

	bb_set_led_state(BB_LED_OFF);
	persist_init();
    SSD1306_Init();
    SDD1306_Clear_Screen();
    bb_scr_init();
    ctrl_init();

    bb_events[BB_EVENT_KEY_IDX].type = BB_EVENT_KEY;
    bb_update_trigger();

	return true;
}

