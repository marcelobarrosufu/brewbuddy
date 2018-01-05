
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
 - The main loop is only a _WFI (wake for interrupt) call, all code runs in interrupt mode:
   - Systick (1ms) timer is used to coordenate the periodic events (leds, display updates, AD samplig and calibration). When some action is required, for instance a display update, it creates an event and sets SVD exception as pending. SVD is configured as a low priority exception, running only when other high priority interrupts are idle. This way we can ensure a better temporization for BrewBuddy.
  - Led modes were implemented as the amount of systick ticks for ON and OFF states. Using this strategy, the led processing is very light, taking few CPU time and energy.
  
Some firmware notes:

  - As the battery voltage level is always falling I decided to calibrate the AD periodically. DMA was not used to sampling the AD values due the high cost in terms of flash usage. STM32 HAL takes some kBs of flash for DMA and a baremetal usage of DMA is required if we want to keep the flash usage low as possible. I need more time for this implementation. Or I need to use a microcontroller with bigger flash.

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
