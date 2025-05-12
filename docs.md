# Static Test Platform

## Overview

This Arduino-based test stand is designed for precise measurement and logging of model rocket motor performance. It integrates several key components:
* HX711 Load Cell Amplifier for force measurement
* SD Card logging
* Serial interface for control and data output

## Hardware Requirements

### Components
* Arduino Board (e.g., Arduino Uno)
* HX711 Load Cell Amplifier
* Load Cell (appropriate for rocket motor testing)
* SD Card Module
* Calibration Weight

### Pin Configuration
* HX711 Data Pin: Digital Pin 8
* HX711 Clock Pin: Digital Pin 9
* SD Card Chip Select: Digital Pin 4

## Features

### 1. Calibration
The system offers multiple calibration methods:
* Automatic calibration procedure
* Manual calibration factor adjustment
* Stored calibration value in EEPROM

### 2. Data Logging
* Real-time serial output
* SD Card logging (CSV format)
* Logs time, weight, and force measurements
* Automatic file naming (TEST0.CSV, TEST1.CSV, etc.)

### 3. Measurement Modes
* Standby Mode
* Calibration Mode
* Measurement Mode

### 4. Serial Interface Commands

| Command | Description |
|---------|-------------|
| `t`     | Tare (zero) the load cell |
| `r`     | Run calibration procedure |
| `c`     | Manually change calibration factor |
| `s`     | Start measurement/test |
| `x`     | Stop measurement/test |
| `p`     | Toggle serial plotter mode |
| `d`     | Test SD card functionality |
| `m`     | Show menu |

## Calibration Procedure

### Automatic Calibration
1. Remove all weight from the load cell
2. Send 't' to tare the scale
3. Place a known calibration weight
4. Enter the weight's mass in grams
5. Choose to save the new calibration value to EEPROM

### Manual Calibration
1. View current calibration factor
2. Enter a new calibration value
3. Optionally save to EEPROM

## Measurement Process

### Data Recorded
* Time (seconds)
* Weight (grams)
* Force (Newtons)

## Serial Plotter Mode
* Toggleable output format for easy graphing
* Provides simplified data representation

## SD Card Logging
* Automatic file creation
* CSV format with headers
* Periodic flushing to prevent data loss
* SD card functionality testing

## Troubleshooting

### Common Issues
* Ensure proper wiring of HX711 load cell
* Verify SD card formatting and connectivity
* Check calibration factor accuracy
* Ensure stable power supply

### Error Messages
* "Timeout, check MCU>HX711 wiring" - Indicates connection problem
* "SD card initialization failed" - SD card not detected or faulty

## Safety Considerations
* Always follow proper safety protocols when testing rocket motors
* Ensure proper isolation and protection during tests
* Use appropriate personal protective equipment (PPE)
* Conduct tests in a controlled, safe environment

## Recommended Workflow
1. Power on the test stand
2. Tare the load cell
3. Calibrate if necessary
4. Prepare rocket motor
5. Start measurement
6. Conduct test
7. Stop measurement
8. Review logged data

## Potential Improvements
* Add more advanced data processing
* Implement wireless data transmission
* Add temperature and environmental logging

## Code Dependencies
* HX711_ADC library
* SD library
* EEPROM library

## Limitations
* Limited by load cell capacity
* Dependent on SD card performance
* Arduino's memory constraints

## Maintenance
* Periodically check load cell calibration
* Clean and inspect mechanical components
* Ensure SD card is functioning correctly

## License
[Add appropriate open-source license information]

## Contributing
[Add guidelines for contributing to the project]
