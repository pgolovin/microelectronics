# 3D Printer Firmware
Homemade 3D Printer firmware based on STM chip with limited ability for customization

## Components
* 3D Printer Firmware. 
* STM32* HAL based high level library of microelectronics components that can be used separatly outside of the project.
* Set of supporting programs
   * Set of tests for components library
   * PrinterEmulator  - Emulator of printer firmware
   * MaterialEditor   - Material data compiler, can be used to create built-in material overrides. (not used)
   * CommandsCompiler - Standalone compiler of G-Code commands to check correctness of compilation or create binary buffer that can be used by printer firmware. (not used)
## 3D Printer Firmware
### Construction:
The printer contains different components that smoothly working together to provide stable and accurate printing. Printer operates without external connection to any PC/Laptop. All commands and models provided through SDCard. The configuration and interaction with user is done using ILI9341 display with touch. UI library supports different set of components that used in forming printer UI.  

Printer supports dual SDCard configuration: 
* External storage, user can download g-code models into it and upload it into internal storage
* Internal storage, currently used as an internal configuration storage to save printer settings. Also it is a primary source of commands for the printing. If printing is started external storage can be removed, because all commands will be get from the internal one.    

_In future WiFi module will be added to allow downloading model data through LAN directly into internal storage of the printer._
### Supported functionality:
* Set of supported G-Code commands
   * G0-G1 - move and print
   * G28   - move to home point
   * G60   - save current head position to the internal storage
   * G92   - set current coordinate without moving head
   * G99   - save current printer state (non standard command)
 * Set of supported G-Code command extensions (M commands)
   * M104  - heat nozzle
   * M106  - enable nozzle cooler
   * M107  - disable cooler
   * M109  - wait nozzle to heat
   * M140  - heat table
   * M190  - wait table to heat
   * M24   - resume printing (disabled) 
 * Printer supports smooth motion control, that accelerates and slows down head speed, to prevent head jittering
