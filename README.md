# Smart Irrigation Controller

ESP32-based smart irrigation controller with automatic soil-moisture control, GSM communication, Wi-Fi API, environmental monitoring, and safety protection.

## Project Status
- [x] KiCad schematic
- [x] ERC verification
- [x] Prototype firmware
- [ ] PCB design
- [ ] PCB fabrication
- [ ] Final hardware testing

## Main Features

- Automatic irrigation based on soil moisture
- Manual pump control
- ESP32 control system
- Soil-moisture sensing
- DHT11 temperature and humidity monitoring
- SIM800L GSM/SMS communication
- Wi-Fi connectivity and HTTP API
- LCD user interface
- Push-button menu system
- EEPROM configuration storage
- Pump runtime protection
- Sensor fault detection
- GSM alert system

## Hardware

The current hardware design includes:

- ESP32 38-pin module
- LM2596 voltage regulator
- Mini360 voltage regulator
- SIM800L
- DHT11
- Soil-moisture sensor
- Relay driver
- 2N2222 transistor
- LCD with I2C interface
- Push buttons
- Buzzer
- 12 V power input

## Repository Structure

```text
firmware/      ESP32 firmware
hardware/      KiCad schematic and custom libraries
prototype/     Prototype photos and videos
docs/          Additional documentation

Schematic

The current KiCad schematic represents the electrical architecture of the prototype.

PCB design will be developed in a later revision.

Prototype

Photos and videos of the prototype will be added here.(soon)

Future Work :

PCB layout
PCB fabrication
Hardware validation
Final enclosure
Improved power protection
Extended communication features