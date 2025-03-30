# OpenTagScale

- [OpenTagScale](#opentagscale)
  - [Introduction](#introduction)
  - [References](#references)
  - [Hardware](#hardware)
    - [Current](#current)
    - [Future](#future)
  - [Wiring Schematic](#wiring-schematic)
  - [Spool](#spool)
  - [3D printed case](#3d-printed-case)

## Introduction

A weight scale designed to work for spools of 3D printer filament with an OpenTag RFID tag attached to them. The data is used to update a local Spoolman server with weight remaining. The idea is that this whole process would be automatic and could be used in conjunction with the OpenSpool project to implement a fully automated, RFID-based, filament management system. 

The system relies on having an NFC tag on the spool. These tags are cheap and readily available, with plenty of storage for all the data needed. The OpenTag and OpenSpool projects talk about their implementation of these tags and this project piggybacks on that by adding one data field (spoolman_id) to the tag to enable looking up the spool in your local Spoolman instance. So far this project only reads that tags that were written previously by another device like a smartphone. This is very cumbersome and one of the next planned improvements is to add the ability to write tags as well.

So far there is no interface except the RGB LED that blinks or lights in various colors to indicate statuses. Future plans are to add a web interface for control of writing tags, configuration, and other. The current setup simply has a web interface setup to be used for the initial connection to a wireless network.

## References

- OpenSpool - https://github.com/spuder/openspool
- OpenTag - https://github.com/Bambu-Research-Group/RFID-Tag-Guide/blob/main/OpenTag.md/
- Spoolman - https://github.com/Donkie/Spoolman

## Hardware

The whole project is still in the alpha phase and will certainly change as it is developed further. Current hardware is depicted below with notes about future hardware plans.

### Current
- ESP8266 Microcontroller
- 5 kg load cell with HX711
- RC522 RFID reader
- RGB LED
- NTAG 215 or NTAG 216

### Future
OpenSpool is using the ESP32 and PN532 exclusively and that seems to be reliable and readily available hardware. I will probably move to the same hardware in the future and in the interest of increased functionality I will attempt to retain compatibility with the currently used hardware as well.
- ESP32 Microcontroller
- PN532 RFID reader

## Wiring Schematic
Work in progress.

## Spool
To make the NFC tags easy to adhere I designed a spool based on the Bambu Lab spool. The [original model](https://cad.onshape.com/documents/36757e334ae10b1e7195433d/w/2f680e493286b2c54e8c8fa2/e/c2bb7262ae2036b0372fdf81) can be seen on onshape. The [finished model](https://makerworld.com/en/models/847682-bambu-spool-w-recessed-filament-holder-v2) is on Maker World. If you like it, I wouldn't mind a boost. 

## 3D printed case
Since it's for a 3D printer, it only makes sense to 3D print the case that it fits in. The [original model](https://cad.onshape.com/documents/46f2bb21fd1245b4dd9cc548/w/38287b3c24f1259fb91325b8/e/d62aecde3ff828b749eea8a5) is designed in onshape and linked here. Feel free to copy the model and change it or let me know if you have good ideas for improvements that I can implement.

