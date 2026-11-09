# T-OS Premium — ESP32-2432S028R

Experimental firmware for classic ESP32-D0WD, 4 MB flash, 2.8" 320x240 ILI9341 LCD and XPT2046 resistive touch.

## Status
Source + GitHub Actions cloud compilation. **Not yet hardware tested.** Display/touch pins are based on common ESP32-2432S028R boards. Do not use the HUD as a safety-critical driving aid.

## Features
- Six colorful touch menu tiles: clock, weather, HUD, assistant placeholder, connection, settings
- Vietnam time via NTP
- Weather from Open-Meteo for Hanoi when Wi-Fi is configured
- HUD demo over serial: `HUD,65,60,350,RIGHT,CAMERA 800M`
- No native Vietmap Live / GoFa / DatMap / WazeMod integration yet
- No AI assistant backend yet

## Build in GitHub
Go to **Actions → Build T-OS firmware → Run workflow**. After success download the artifact `TOS-Premium-ESP32-4MB`. It contains a **merged BIN** at address **0x0** and a standalone app BIN at 0x10000. Use **merged BIN at 0x0** in ESPFlash Android, 115200 baud. Never flash ZIP/source as a BIN.

The GitHub build downloads the Arduino ESP32 core and Arduino libraries. The build must show a green success check before flashing. If the workflow fails, do not flash any files from that run.

## Wi-Fi
Edit `TOS_Premium/config.h` in GitHub with your network name/password *only if the repository is private*. This repo is public, so **do not commit real Wi-Fi passwords**. Default firmware works offline for menu/HUD; weather/NTP require Wi-Fi. A later release will provide secure on-device Wi-Fi setup.

## Notes
Board screen and touch revisions vary. If LCD is white or touch inaccurate, verify hardware revision. Touch calibration constants are in config.h. Backlight is GPIO21 on this board diagram. ESP32 classic 4 MB, flash mode DIO, 40 MHz.

Firmware is for testing while stationary.