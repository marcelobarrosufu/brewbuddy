
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

# Compiling

 - Create a new workspace using System Worbenck for STM32 (SW4STM32). For instance,  <path>/brewbuddy
 - Go to brewbuddy directory and clone this repository
 - Starts SW4STM32 and import the project:   File->Import->General->Existing Projects into Workspace
 - In the dialog, please set:
    - Select root directory:  select your workspace directory, <path>/brewbuddy
    - Select the brew buddy project
    - Do not check the option "copy projects into workspace"
 - Before compiling, check optimization as "Optimize for Size (-Os)", under Properties->C/C++ Build->Tool Settings->MCU GCC Compiler->Optimization

# CAD Files

Please, install KiCAD for opening schematics and PCB.

# Licensing

BrewBuddy by Marcelo Barros de Almeida is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
If you want to create a commercial device based on BrewBuddy, please contact me for other licenses and prices.

# TODO

 - When followings steps, make possible to go to next step without waiting the heating phase (just pressing RIGHT key)
 - Calibration menu
 - Setup menu, for temperatue and language
 - Change the microcontroller for more flash space (required for additional menus). STM32F030K6 or STM32F031F6 are possible options. The second one is more expensive but pin compatible.
 - Read internal temperature (for protection or calibration).
 - Low battery indication at startup.

# Author

BrewBuddy was created from scracth by me, Marcelo Barros. I am an electrical engineer, professor and home brewer. After some bad experiences with uncalibrated analog thermometers I decided to create my own thermometer, agregating additional functionalities for mashing and boiling.