# EVERBLU_ESP32_arduino
# everblu-meters - Water usage data for Home Assistant
Fetch water/gas usage data from Cyble EverBlu meters using RADIAN protocol on 433Mhz. Intergrated with Home Assistant via MQTT. 
## Hardware
![ESP32 with CC1101](wt32-eth01_pinsconf.jpg)
The project runs on ESP32 with an RF transreciver (CC1101). 

### Connections ESP32 to CC1101:

- See `cc1101.ccp` for SPI pins mapping.
- See `everblu_meters.h` for GDOx pins mapping.


Pins wiring for [wt21-eth01 board link](https://files.seeedstudio.com/products/102991455/WT32-ETH01_datasheet_V1.1-%20en.pdf),  [CC1101 8-pin DATASHEETS ](https://www.m2mmarket.com.tr/Data/EditorFiles/E07-M1101D-SMA_Datasheet_EN_v1.0_1.pdf) :

|CC1101|Wt32-eth01|
|--|--|
|VCC|`3V3`|
|GOD0 (GDO0) |`GPIO 14`|
|CSN (SPI chip select) |`GPIO 0`|
|SCK (SPI clock)|`GPIO 12`|
|MOSI (SPI MOSI) |`GPIO 32|`
|GOD1 (SPI MISO)|`GPIO 4`|
|GOD2 (GDO2) |not inst|
|GND (ground)|`GND`|

##LED and Switch pins
|||
|little switch |`GPIO 5`|
|LED1 |`GPIO 15`|
|LED2 |`GPIO 2`|

### Where YearMeter and SerialNumber ?
[Where Year and SN](meter_label.png)