/*
  BrewBuddy by Marcelo Barros de Almeida is licensed under a
  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.

  For commercial usage, please contact the author via marcelobarrosalmeida@gmail.com
*/

#ifndef PERSIST_C_
#define PERSIST_C_

void persist_init(void);
void persist_save(uint8_t num_slopes, ctrl_slope_t slopes[], uint8_t num_hops, ctrl_hop_t hops[], ctrl_slope_t boil);
void persist_load(uint8_t *num_slopes, ctrl_slope_t slopes[], uint8_t *num_hops, ctrl_hop_t hops[], ctrl_slope_t *boil);

#endif /* PERSIST_C_ */
