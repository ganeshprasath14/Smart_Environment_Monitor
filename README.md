# Smart Environment Monitor using STM32F401RE

This project is a simple embedded environment monitoring and alert system developed using the STM32F401RE microcontroller.

The system detects motion using a PIR sensor and reads the surrounding light level using an LDR sensor. When motion is detected in a dark environment, the LED is turned ON and the buzzer gives an alert. The LDR value and motion status are displayed on an SSD1306 OLED using I2C.

The firmware is written in Embedded C using direct STM32 register programming.

## Features

- PIR based motion detection
- LDR sensor reading using ADC
- SSD1306 OLED display
- I2C communication
- LED control based on motion and light level
- Buzzer alert during motion in dark conditions
- Direct STM32 register-level programming

## Hardware Used

- STM32F401RE Nucleo board
- PIR motion sensor
- LDR sensor module
- 0.96 inch SSD1306 OLED display
- Active buzzer
- Breadboard
- Jumper wires

## Pin Connections

### PIR Sensor

OUT -> PA0  
VCC -> 5V  
GND -> GND

### LDR Module

Analog Output -> PA1  
VCC -> 3.3V  
GND -> GND

### OLED Display

SCL -> PB8  
SDA -> PB9  
VCC -> 3.3V  
GND -> GND

### Other Connections

Onboard LED LD2 -> PA5  
Buzzer -> PA6

## Working

The PIR sensor is used to detect motion.

The LDR analog output is connected to PA1 and the ADC is used to read the sensor value.

The measured LDR value is compared with a threshold value of 2000.

When motion is detected in a dark environment:

- The onboard LED turns ON.
- The buzzer gives an alert.
- The OLED displays the LDR value and motion status.

When there is no motion or the environment is bright, the LED and buzzer remain OFF.

## OLED Display

The project uses an SSD1306 128x64 OLED display with I2C communication.

PB8 is used as SCL and PB9 is used as SDA.

The OLED I2C address used in the project is:

0x3C

## Peripherals Used

GPIO

Used for the PIR input, LED output and buzzer output.

ADC1

Used to read the analog output of the LDR through PA1.

I2C1

Used to communicate with the SSD1306 OLED through PB8 and PB9.

Timers

Used for timing the buzzer and motion-related operations.

## Project Structure

Smart_Environment_Monitor

Core
  Inc
  Src
    main.c
  Startup

Drivers

Debug

Smart_Environment_Monitor.ioc

README.md

## How to Run

1. Open the project in STM32CubeIDE.
2. Connect the STM32F401RE board using ST-LINK.
3. Connect the PIR, LDR, OLED and buzzer according to the pin connections.
4. Build the project.
5. Flash the program to the STM32F401RE.
6. The OLED will display the sensor information.
7. Move in front of the PIR sensor and change the light level to test the system.

## What I Learned

This project helped me gain practical experience with STM32F4 microcontrollers and embedded C programming.

I worked with GPIO, ADC, I2C, timers, interrupts, bit manipulation and OLED interfacing.

It also helped me understand how multiple sensors and peripherals can be integrated into a single embedded system.

## Future Improvements

- Add temperature and humidity sensors.
- Add wireless connectivity for remote monitoring.

## Author

Ganesh Prasath

M.Tech Embedded Systems
