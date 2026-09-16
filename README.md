# Smart Irrigation Controller(Work in progress)

ESP32-based smart irrigation controller with automatic soil-moisture control, GSM communication, Wi-Fi API, environmental monitoring, and safety protection.

> **License:** Proprietary — All Rights Reserved. Contributions are welcome via Pull Requests. See [LICENSE](LICENSE) and [CONTRIBUTING.md](CONTRIBUTING.md) for details.

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
- General-purpose electrolytic capacitors(1000uF)
- Low-ESR capacitors(1000uF)
- Ceramic capacitors(100nF)

## Repository Structure



├── README.md
├── LICENSE
├── firmware/
├── hardware/
│   ├── schematic/
│   └── documentation/
├── docs/
│   ├── hardware-design.md
│   ├── firmware-architecture.md
│   ├── troubleshooting.md
│   ├── testing.md
│   └── communication.md
└── prototype/


Schematic

The current KiCad schematic represents the electrical architecture of the prototype.

PCB design will be developed in a later revision.


##Future Work :

PCB layout
PCB fabrication
Hardware validation
Final enclosure
Improved power protection
Extended communication features



##Overview

This project is an ESP32-based smart irrigation controller designed to monitor soil moisture and control a water pump according to configurable moisture thresholds. The system supports both automatic and manual operation through a local LCD interface, while also providing remote control and status monitoring through Wi-Fi and GSM/SMS communication.
The controller uses a soil-moisture sensor as the primary input for automatic irrigation, with a DHT sensor providing environmental temperature and humidity data. A relay-driven pump provides the irrigation output, while configurable calibration values and start/stop thresholds allow the system to be adapted to different soil and sensor characteristics.
The hardware was developed and validated initially as a functional prototype. The current repository documents the validated schematic and firmware; PCB design and further hardware refinement are planned as subsequent development stages.



##Features

•	Automatic irrigation based on configurable soil-moisture start and stop thresholds.
•	Manual pump control through the local LCD interface.
•	Soil-moisture calibration using configurable dry and wet reference values.
•	Temperature and humidity monitoring using a DHT sensor.
•	LCD user interface with dashboard, operating-mode selection, manual pump control, and configuration menus.
•	Wi-Fi connectivity for remote status monitoring and authenticated pump/control commands through an HTTP API.
•	GSM/SMS connectivity through the SIM800L module for remote commands and status communication.
•	Relay-based pump control with software handling of the relay's active logic.
•	Pump runtime protection to prevent the pump from operating continuously beyond the configured safety limit.
•	Sensor fault detection with pump shutdown when a critical soil-moisture sensor fault is detected.
•	Persistent configuration for operating mode, moisture thresholds, and calibration parameters.
•	Audible feedback through a buzzer for user-interface actions and system events.



##System Architecture

The system is organized around an ESP32 microcontroller that acts as the central control and communication unit. It acquires sensor data, manages the irrigation state, drives the pump relay, updates the local LCD interface, and handles remote commands received through Wi-Fi and GSM.


Functional Architecture


                         ┌─────────────────────┐
                         │       ESP32         │
                         │                     │
                         │  Control Logic      │
                         │  State Management   │
                         │  UI Management      │
                         │  Communications     │
                         └──────────┬──────────┘
                                    │
          ┌─────────────────────────┼─────────────────────────┐
          │                         │                         │
          ▼                         ▼                         ▼
   ┌─────────────┐           ┌─────────────┐           ┌─────────────┐
   │ Soil Sensor │           │ DHT Sensor  │           │ LCD + I²C   │
   │  Moisture   │           │ Temp/Humid. │           │    UI       │
   └─────────────┘           └─────────────┘           └─────────────┘
          │
          │ moisture data
          ▼
   ┌─────────────────────────────────────────────────────────────┐
   │                    Irrigation Control                       │
   │                                                             │
   │   AUTO:   START/STOP thresholds determine pump state        │
   │   MANUAL: User directly controls pump state                 │
   └──────────────────────────────┬──────────────────────────────┘
                                  │
                                  ▼
                           ┌─────────────┐
                           │ Relay Driver│
                           │  2N2222     │
                           └──────┬──────┘
                                  │
                                  ▼
                           ┌─────────────┐
                           │    Relay    │
                           └──────┬──────┘
                                  │
                                  ▼
                              ┌───────┐
                              │ Pump  │
                              └───────┘


        ┌─────────────────┐                 ┌─────────────────┐
        │      Wi-Fi      │                 │     SIM800L     │
        │  HTTP API       │                 │    GSM / SMS    │
        └────────┬────────┘                 └────────┬────────┘
                 │                                   │
                 └────────────────┬──────────────────┘
                                  ▼
                               ESP32


The separate regulator paths isolate the high-current GSM load from the main logic supply while maintaining a common electrical reference between the subsystems. The common ground is required for reliable signal communication between the ESP32 and externally powered modules such as the SIM800L.


## Hardware Design

### Main Controller
ESP32 acts as the central controller, handling sensor acquisition,
irrigation control, user interface, and communication.

### Power Supply
The system uses a 12 V input with separate regulated supply paths
for the ESP32/logic and SIM800L. The separate branches reduce the
effect of GSM transient current demand on the main logic supply,
while maintaining a common ground reference.

### Sensors
- Capacitive soil-moisture sensor connected to an ESP32 ADC input.
- DHT sensor for temperature and relative humidity.

### Pump Control
The pump is switched through a relay driven by a 2N2222 transistor,
rather than directly from an ESP32 GPIO.

### User Interface
A 16×2 I²C LCD and physical buttons provide local monitoring,
configuration, and manual pump control.

### Communication
- Wi-Fi through the ESP32's integrated interface and HTTP API.
- GSM/SMS through the SIM800L UART interface.

### Power Integrity
Local decoupling and bulk capacitance are used to support supply
stability and transient load behavior.


##Design Philosophy

The hardware was designed around several principles:
1.	Keep high-current loads away from direct microcontroller GPIO drive.
2.	Provide appropriate regulation for subsystems with different power requirements.
3.	Maintain a common signal reference between communicating subsystems.
4.	Use calibration rather than assuming a universal raw sensor-to-percentage relationship.
5.	Keep the schematic readable through functional grouping and appropriate net labeling.
6.	Treat power integrity and transient behavior as part of the system design rather than as an afterthought.


##Firmware Architecture

The firmware is organized around a non-blocking control loop in which sensor acquisition, user-interface handling, irrigation control, communication, and safety checks are processed as independent tasks.
The ESP32 acts as the central software controller and maintains the current system state, including the operating mode, sensor measurements, pump state, configuration parameters, and communication status.

Main Control Flow

                    ┌──────────────────┐
                    │   System Setup   │
                    │                  │
                    │ GPIO / LCD /     │
                    │ Sensors / Wi-Fi  │
                    │ GSM initialization│
                    └────────┬─────────┘
                             │
                             ▼
                    ┌──────────────────┐
                    │    Main Loop     │
                    └────────┬─────────┘
                             │
          ┌──────────────────┼───────────────────┐
          │                  │                   │
          ▼                  ▼                   ▼
   Sensor Processing    User Interface      Communications
          │                  │                   │
          ▼                  ▼                   ▼
   Sensor / Fault       LCD + Buttons       Wi-Fi + GSM
      State                  │                   │
          │                  └────────┬──────────┘
          │                           │
          └──────────────┬────────────┘
                         ▼
                 Irrigation Control
                         │
              ┌──────────┴──────────┐
              │                     │
             AUTO                 MANUAL
              │                     │
        START / STOP            User command
        thresholds                  │
              │                     │
              └──────────┬──────────┘
                         ▼
                    Pump Control
                         │
                         ▼
                       Relay


##Operating Modes

The firmware maintains two primary irrigation modes.

AUTO Mode:
In automatic mode, the pump state is determined from the calibrated soil-moisture measurement and the configured START and STOP thresholds.
The use of separate start and stop thresholds provides hysteresis. This prevents the pump from rapidly switching states when the measured moisture is close to a single threshold.
Conceptually:
Moisture < START  → Pump ON
Moisture ≥ STOP   → Pump OFF
The exact boundary behavior is implemented by the firmware.
MANUAL Mode:
In manual mode, the automatic moisture-control decision is disabled and the user directly selects the pump state through the LCD interface.
The manual interface provides explicit PUMP ON and PUMP OFF commands.
This separates user-directed operation from the automatic irrigation algorithm.


## Control Logic

The soil-moisture sensor is sampled through the ESP32 ADC and
converted to a normalized moisture percentage using configurable
DRY and WET calibration references.

All pump-control paths converge on the same control mechanism:

- Automatic moisture control
- LCD manual control
- Wi-Fi commands
- GSM/SMS commands

A configurable pump-runtime timeout provides an additional safeguard
against indefinite pump operation.

##LCD User Interface

The LCD interface is implemented as a collection of screens/states rather than as one continuously redrawn menu.
The main interface consists of:
Main Menu
├── AUTO
├── MANUAL
│   ├── PUMP ON
│   └── PUMP OFF
└── SETTINGS
    ├── START
    ├── STOP
    ├── DRY
    └── WET
The settings interface allows the user to modify the automatic irrigation thresholds and sensor calibration parameters.
The display logic also explicitly manages the contents of individual LCD fields so that remnants of previous, longer strings are not left visible after navigation.

## Communication Interfaces

### Wi-Fi

The ESP32 exposes an HTTP API for status monitoring and authenticated
control commands.

GET /status

/control?key=<API_KEY>&cmd=<COMMAND>

Supported commands include:

- AUTO
- MANUAL
- PUMP_ON
- PUMP_OFF
- RESET_PUMP

### GSM / SMS

The SIM800L provides an independent remote-control path through SMS.
Received commands are routed through the same command-processing layer
used by the local and Wi-Fi interfaces.

##Configuration and Persistence

User-configurable parameters are stored separately from transient runtime state.
Configuration includes the values required for irrigation thresholds and sensor calibration. 

## Safety and Fault Handling

The firmware includes software-level safeguards intended to prevent
common sensor and control failures from causing uncontrolled pump
operation.

Implemented protections include:

- Critical soil-moisture sensor fault handling
- Maximum continuous pump runtime
- Pump-state validation
- Hysteresis between START and STOP thresholds
- Consistent command routing across local and remote interfaces

The general failure-handling principle is to prevent unreliable
conditions from resulting in indefinite irrigation.

## Troubleshooting & Engineering Validation

Prototype debugging was performed using a measurement-based fault
isolation approach rather than modifying firmware based only on
observed symptoms.

The development process included:

- Schematic inspection
- Continuity and voltage measurements
- Serial diagnostics
- Subsystem isolation
- Controlled functional tests
- KiCad ERC validation

Representative issues included:

- SIM800L UART failure caused by missing common ground between
  independently regulated subsystems.
- Relay/pump logic inversion caused by the relay's active-low interface.
- LCD residual characters caused by rewriting shorter strings over
  longer previous content.
- DHT communication failures investigated independently from other
  subsystems.
- KiCad ERC errors resolved by correcting actual connectivity and
  electrical-type issues rather than broadly suppressing checks.


##Testing & Validation

The prototype was tested incrementally, with individual subsystems validated before being tested as an integrated system. Functional tests were performed on the sensor interfaces, user interface, actuator control, Wi-Fi API, and GSM communication.
Functional Test Matrix
Subsystem	Test	Expected Result	Result
ESP32	Power-up and initialization	Controller starts normally	✅
Soil sensor	Change sensor conditions	Moisture reading changes accordingly	✅
DHT sensor	Read temperature and humidity	Valid environmental readings are obtained	✅
LCD	Dashboard display	Current system state is displayed correctly	✅
LCD	Menu navigation	Menus can be navigated using the buttons	✅
LCD	Manual pump control	User can turn the pump ON/OFF	✅
LCD	Configuration	START/STOP and calibration values can be modified	✅
Relay	Pump ON command	Relay switches to the active state	✅
Pump	Manual ON/OFF	Physical pump follows the selected state	✅
AUTO mode	Low moisture condition	Pump activates according to START threshold	✅
AUTO mode	Adequate moisture condition	Pump stops according to STOP threshold	✅
Pump safety	Extended continuous operation	Safety timeout prevents indefinite operation	✅
Wi-Fi	Network connection	ESP32 connects and obtains an IP address	✅
HTTP API	/status	Valid JSON system status is returned	✅
HTTP API	Unauthorized control request	Request is rejected	✅
HTTP API	Authenticated MANUAL command	Command is accepted	✅
HTTP API	Authenticated PUMP_ON	Pump activates	✅
HTTP API	Authenticated PUMP_OFF	Pump stops	✅
SIM800L	AT communication	Modem responds to initialization commands	✅
GSM/SMS	SMS reception	Incoming command is detected and parsed	✅
GSM/SMS	STATUS	Status response is returned by SMS	✅
GSM/SMS	Pump control	SMS command changes pump state	✅
Schematic	KiCad ERC	No unresolved ERC errors remain	✅


## Build and Setup

### Hardware

Before powering the prototype, verify:

- Input supply voltage
- Regulated supply voltages
- Common ground connections
- ESP32 GPIO assignments
- Sensor connections
- Relay and pump connections
- SIM800L power and UART connections

Refer to the KiCad schematic and pin-assignment documentation for
the complete electrical connections.

### Firmware

The firmware is developed for the ESP32 using the Arduino framework.

Before compiling, configure the project-specific parameters:

- Wi-Fi credentials
- Wi-Fi API key
- GSM/SIM configuration
- Hardware pin assignments
- Irrigation defaults

Real credentials must never be committed to the public repository.

### Initial Configuration

1. Verify sensor readings.
2. Perform DRY/WET calibration.
3. Configure START/STOP thresholds.
4. Verify manual pump operation.
5. Test AUTO mode.
6. Test Wi-Fi/API control.
7. Test GSM/SMS functionality.
8. Verify pump safety behavior.

##Security

The public repository must not contain real:
•	Wi-Fi passwords;
•	API keys;
•	SIM credentials;
•	telephone numbers;
•	other authentication secrets.
Example placeholders should be used instead:
WIFI_SSID     = "YOUR_WIFI_SSID"
WIFI_PASSWORD = "YOUR_WIFI_PASSWORD"
WIFI_API_KEY  = "YOUR_API_KEY"
The API key should be treated as a credential even though the project is intended as a prototype.

##User Guide

For detailed operating instructions, see
[User Guide](docs/user-guide.md).

##Security Considerations

The Wi-Fi control endpoint is protected by an API key.
The API key must be treated as a credential and must not be committed to the public repository.

## Future Work

### PCB Development

Replace prototype wiring with a dedicated PCB featuring controlled
routing, dedicated connectors, improved power distribution, and
better mechanical integration.

### Power Integrity

Further validate supply stability during worst-case GSM transmission
bursts and simultaneous pump operation.

### Electrical Protection

Add production-oriented protection such as:

- Overvoltage protection
- Reverse-polarity protection
- Fusing
- Transient suppression
- Improved actuator isolation
- Hardware-level pump shutdown

### Environmental & Mechanical Design

Develop a suitable enclosure with appropriate ingress protection,
connector protection, cable strain relief, thermal management,
mounting, and serviceability.

### Sensor Calibration

Improve the calibration workflow and evaluate long-term effects of
sensor aging, soil composition, environmental conditions, and
installation depth.

### Communication Security

Replace the prototype-oriented API-key security model with stronger
authentication, encrypted transport, credential management, and
network isolation where required.

### Reliability Testing

Perform long-duration testing, repeated switching, power-cycle tests,
sensor-disconnection tests, communication-loss tests, and
brownout/transient-condition testing.

### Firmware

Potential improvements include OTA updates, enhanced diagnostics,
event logging, fault recovery, and expanded configuration management.

##Current Scope

The current project should therefore be considered a functional embedded-systems prototype, rather than a finished commercial irrigation product.
The current revision demonstrates the integration of:
Sensors
   ↓
ESP32 control system
   ↓
Local user interface
   ↓
Automatic / manual control
   ↓
Relay / pump actuator

          +
Wi-Fi remote control
          +
GSM / SMS communication
          +

Software safety mechanisms

PCB development, production-level protection, environmental packaging, extensive reliability testing, and production-grade communication security constitute subsequent engineering stages.


