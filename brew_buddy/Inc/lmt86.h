/*
  BrewBuddy by Marcelo Barros de Almeida is licensed under a
  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.

  For commercial usage, please contact the author via marcelobarrosalmeida@gmail.com
*/

#ifndef LMT86_H_
#define LMT86_H_

int32_t lmt86_conv_temp_1d(uint32_t adc_val, uint32_t adc_steps, uint32_t vref_mv);
int32_t lmt86_locale_conv_temp(int32_t c1d);

#endif /* LMT86_H_ */
