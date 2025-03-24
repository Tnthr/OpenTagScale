# OpenTagScale

A weight scale designed to work for spools of 3D printer filament with an OpenTag RFID tag attached to them. The data is used to update a local Spoolman server with weight remaining. The idea is that this whole process would be automatic and could be used in conjunction with the OpenSpool project to implement a fully automated, RFID-based, filament management system.

## References

- OpenSpool - https://github.com/spuder/openspool
- OpenTag - https://github.com/Bambu-Research-Group/RFID-Tag-Guide/blob/main/OpenTag.md/
- Spoolman - https://github.com/Donkie/Spoolman


## Hardware

The whole project is still in the alpha phase and will certainly change as it is developed further. Current hardware is depicted below with notes about future hardware plans.

### Current:
- ESP8266 Microcontroller
- 5 kg load cell with HX711
- RC522 RFID reader
- RGB LED

### Future:
OpenSpool is using the ESP32 and PN532 exclusively and that seems to be reliable and readily available hardware. I will probably move to the same hardware in the future and in the interest of increased functionality I will attempt to retain compatibility with the currently used hardware as well.
- ESP32 Microcontroller
- PN532 RFID reader

## 3D printed case
Since it's for a 3D printer, it only makes sense to 3D print the case that it fits in.
