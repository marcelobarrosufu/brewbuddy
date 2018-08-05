
# Description

BrewBuddy is a compact device for helping homebrewers in their exhausting task of converting malts, hops and yeasts into happiness.

It can be used as a simple thermometer, like any other analog thermometer found in the market but aggregating time control via stopwatch. As you know, time and temperature walk together when brewing a nice beer.

Although BrewBuddy is not a controller, you can program your recipe using a step mash strategy. BrewBuddy will blink a led when an step is finished, indicating that you can heat the mosture again.

When boiling, it is very easy to program your hop additions, waiting for a BrewBuddy indication. Smartphones or additional devices are not required anymore!

BrewBuddy is powered by a regular CR2032 battery, running for 20h! Menus and configurations are simple and intuitive, you will learn all function in minutes.

You can program up to 10 steps/hop additions (time) and the last setup is automatically saved in non volatile memory.

Some videos:

 - https://youtu.be/tlFAVe8KUfs
 - https://youtu.be/ZO_FSPlZCxY
 - https://youtu.be/FMSUGOILIN4
 - https://youtu.be/00Js_LpAEr8
 - https://youtu.be/yrWA2lQHpJ8

# Author

BrewBuddy was created from scracth by me, Marcelo Barros. I am an electrical engineer, professor and home brewer. After some bad experiences with uncalibrated analog thermometers I decided to create my own thermometer, agregating additional functionalities for mashing and boiling.

# Help Required

If you are a 3D expert and wanna help, please contact me. In the next week my new PCBs will arrive and I can ship  a complete and working board for those that want to develop or improve the current case.

# Compiling

 - Create a new workspace using System Worbenck for STM32 (SW4STM32). For instance,  <path>/brewbuddy
 - Go to brewbuddy directory and clone this repository
 - Starts SW4STM32 and import the project:   File->Import->General->Existing Projects into Workspace
 - In the dialog, please set:
    - Select root directory:  select your workspace directory, <path>/brewbuddy
    - Select the brew buddy project
    - Do not check the option "copy projects into workspace"
 - Before compiling, check optimization as "Optimize for Size (-Os)", under Properties->C/C++ Build->Tool Settings->MCU GCC Compiler->Optimization

# Firmware Notes

BrewBuddy firmware was built with low power requirements in mind, generating several actions:

 - The controller clock was reduced to 1MHz. Internal 8MHz clock (HSI) is used and PLL is disabled. 
 - I2C clock is only 100kHz (standard mode).
 - The main loop (see `main.c`) is only a _WFI (wake for interrupt) call, all code runs in interrupt mode:
   - Systick (1ms) timer is used to coordenate the periodic events (leds, display updates, ADC samplig and calibration). When some action is required, for instance a display update, it creates an event and sets SVD exception as pending. SVD is configured as a low priority exception, running only when other high priority interrupts are idle. This way we can ensure a better temporization for BrewBuddy.
  - Led modes were implemented as the amount of systick ticks for ON and OFF states. Using this strategy, the led processing is very light, taking few CPU time and energy.
  - Floating point is not used (less power and less flash requirements).
  
Some firmware notes:

  - As the battery voltage level is always falling I decided to calibrate the ADC periodically. DMA was not used to sampling the ADC values due the high cost in terms of flash usage. STM32 HAL takes some kBs of flash for DMA and a baremetal usage of DMA is required if we want to keep the flash usage low as possible. I need more time for this implementation. Or I need to use a microcontroller with bigger flash.
  - `bb_scr.c` is the main file for OLED screens. OLED screens are a machine state, in fact. Each screen is represented by a function (state) with the prototype `bb_scr_list_t bb_screen_function(bool first_time, bb_key_event_t *key)`, where:
    - `first_time`: it is used to indicate when we have a new screen to be presented or the same screen. Static text may be drawn in display using this flag, decreasing the amount of elements that need to be updated at each screen refresh.
    - `key`: if a key was pressed the information is here.
    - Remember: screens are implemented as a machine state (function based). So, you must return the next state when leaving the screen function. The list of states is given by the enumeration `bb_scr_list_t`. To include a new state, you must edit the macro `XMACRO_SCR_LIST`, adding a new state ID and a new function name, like `X(BB_SCR_NEW_OP_ID, bb_scr_new_op_boil)`. After, create a function for `bb_scr_new_op_boil` and implement the screen and your state changes based on the screen ID and keys pressed.
  - Step control is implemented in `ctrl.c`. Again, it is a machine state called at each 1s (see `ctrl_sm_run_1s()`). Given a proper initialization, steps can be followed by the states implemented in `ctrl.c`. 
  - All LMT86 handling was implemented in `lmt86.c`. Given the current temperature value (in ADC steps) and the amount of steps for a 2.5V reference, the real temperature (in degrees) is calculate (see `lmt86_conv_temp_1d()`). As we do not use floating point, conversion is based on a mV x degrees table (provided by datasheet). A simple interpolation is performed when the current value is not in the table. The temperature value is multiplied by ten for a minimum precision in integer operations.
  - If you want to change fonts, check the file `ssd1306_fonts.c`. We are using a small 7x5 font due to flash limits but it is possible to create bigger fonts and better presentation. The SSD1306 driver is minimal as well, a stripped/modified version of original driver provided by stm32f4-discovery.net. 
  - In `persist.c` you can find the default value for persistence and the routines to save / restore values from flash. A CRC is provided to check data integrity. The last flash sector (1kB) is used for data storage.
  
  At this moment, we do not have flash for anything else, even compiling for release and using size optimization:

| Flash | RAM (data+bss) |
| --- | --- |
| 15092 of 15360 bytes (98.3%) | 1200 of 4096 bytes (29.3%) |

# Adding localization support 

Some steps to add support for a new language:

  - Create a copy of `bb_locale_ENUS.h` and modify the suffix using a combination of [ISO-639 Language Codes and ISO ISO-3166 Country Codes](https://docs.oracle.com/cd/E13214_01/wli/docs92/xref/xqisocodes.html). For instance, you can use `bb_locale_ESCL.h` for Spanish and Chile.
  - Change the define `BB_DEFAULT_LOCALE` inside `bb_locale.h` (use your localization file name).
  - Translate the file contents. 
  - In special, for units, change `BB_TEMP_UNIT_IS_CELSIUS` and `BB_TEMP_UNIT_LABEL` for your corresponding temperature unit (0 and 'F' for Fahrenheit and 1 and 'C' for Celsius).

# CAD Files

Please, install KiCAD for opening schematics and PCB.

# TODO

 - When followings steps, make possible to go to next step without waiting the heating phase (just pressing RIGHT key)
 - Calibration menu
 - Setup menu, for temperatue and language
 - Change the microcontroller for more flash space (required for additional menus). STM32F030K6 or STM32F031F6 are possible options. The second one is more expensive but pin compatible.
 - Read internal temperature (for protection or calibration).
 - Low battery indication at startup.

# Licensing

BrewBuddy by Marcelo Barros de Almeida is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
If you want to create a commercial device based on BrewBuddy, please contact me for other licenses and prices.
