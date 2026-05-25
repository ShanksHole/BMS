# BMS
The Smart 4S Battery Management System (BMS) uses the ESP32-C3 and BQ76920 to monitor cell voltages, pack voltage, current, temperature, and SoH. It provides battery protection, MOSFET control, cell balancing, and real-time monitoring using a 16x2 LCD display.

# Smart 4S Battery Management System (BMS)

## Overview

This project is a Smart 4S Lithium-Ion Battery Management System designed using the ESP32-C3 and BQ76920. The system monitors and protects a 4-series lithium battery pack by continuously measuring cell voltages, pack voltage, current, temperature, and battery health parameters in real time.

The BQ76920 acts as the Analog Front End (AFE), responsible for precise battery sensing and protection features, while the ESP32-C3 handles data processing, display management, protection logic, and communication.

---

# Features

* Real-time monitoring of:

  * Individual cell voltages
  * Total pack voltage
  * Battery current
  * Temperature
  * Battery State of Health (SoH)

* Battery protection mechanisms:

  * Overvoltage protection
  * Undervoltage protection
  * Overcurrent protection
  * Overtemperature protection

* MOSFET control:

  * Charging MOSFET control
  * Discharging MOSFET control

* Passive cell balancing support

* 16x2 I2C LCD interface with automatic sliding screens

* Serial monitor debugging output

* ESP32-C3 based embedded architecture

---

# Hardware Used

| Component              | Description                 |
| ---------------------- | --------------------------- |
| ESP32-C3               | Main microcontroller        |
| BQ76920                | Battery monitoring AFE      |
| 16x2 I2C LCD           | Display module              |
| 4S Li-Ion Battery Pack | Battery source              |
| Shunt Resistor         | Current sensing             |
| MOSFETs                | Charge/discharge protection |
| NTC Thermistor         | Temperature sensing         |

---

# System Architecture

The battery pack is connected to the BQ76920 through cumulative voltage sensing lines (VC1–VC5). The BQ76920 continuously measures the battery parameters and communicates with the ESP32-C3 using the I2C protocol.

The ESP32-C3:

* Reads sensor data
* Calculates battery parameters
* Executes protection logic
* Controls MOSFETs
* Displays information on LCD

---

# LCD Display Screens

The LCD automatically switches screens every 3 seconds.

### Screen 1

* Pack Voltage
* Current
* Temperature

### Screen 2

* Cell 1 Voltage
* Cell 2 Voltage
* Cell 3 Voltage
* Cell 4 Voltage

### Screen 3

* Battery State of Health (SoH)

### Screen 4

* Fault Status

### Screen 5

* MOSFET Status

---

# Protection Logic

The system automatically disables charging or discharging when unsafe conditions are detected.

## Overvoltage Protection

If any cell voltage exceeds:

```text id="ejg5i0"
4.2V
```

Charging MOSFET is disabled and balancing activates.

## Undervoltage Protection

If any cell voltage falls below:

```text id="5v8gvh"
3.0V
```

Discharging MOSFET is disabled.

## Overcurrent Protection

If pack current exceeds:

```text id="8j4g2q"
20A
```

Discharging MOSFET is disabled.

## Overtemperature Protection

If temperature exceeds:

```text id="m4k8i7"
60°C
```

Both MOSFETs are disabled.

---

# Cell Configuration

This project uses a custom 4S configuration with the BQ76920:

* VC3 and VC4 are shorted
* VC5 is used as the final cell measurement point

Cell calculations:

```cpp id="d0f3xq"
C1 = VC1
C2 = VC2 - VC1
C3 = VC3 - VC2
C4 = VC5 - VC3
```

---

# Software Features

* I2C communication
* Register-level BQ76920 control
* ADC data processing
* Cell balancing control
* Fault detection system
* LCD user interface
* Serial debugging interface

---

# Applications

* Electric vehicles
* Portable battery packs
* Solar energy storage systems
* Robotics
* Drones
* UPS systems
* Embedded battery monitoring systems

---

# Future Improvements

* Bluetooth/WiFi monitoring
* Mobile application integration
* Cloud battery analytics
* Data logging
* SOC estimation algorithms
* Advanced thermal management
* Active balancing system

---

# Developed Using

* Arduino IDE
* ESP32 Board Package
* Embedded C++
* I2C Communication Protocol
