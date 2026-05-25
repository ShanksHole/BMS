/*
* Description: This code is used in esp32c3 which is contacting with BQ76920 
* Author: Vrushank Hole
* Created : 25/5/2026
* Modified : 25/5/2026
* Version: 1
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

//==================================================
// SMART 4S BMS
// ESP32-C3 + BQ76920 + 16x2 LCD
//==================================================

//--------------------------------------------------
// I2C CONFIGURATION
//--------------------------------------------------

#define SDA_PIN 8
#define SCL_PIN 9

#define BQ76920_ADDR 0x08

//--------------------------------------------------
// LCD CONFIGURATION
//--------------------------------------------------

LiquidCrystal_I2C lcd(0x27, 16, 2);

//--------------------------------------------------
// BQ76920 REGISTERS
//--------------------------------------------------

#define CELLBAL1      0x01

#define SYS_CTRL1     0x04
#define SYS_CTRL2     0x05

#define VC1_HI_BYTE   0x0C
#define VC2_HI_BYTE   0x0E
#define VC3_HI_BYTE   0x10
#define VC5_HI_BYTE   0x14

#define TS1_HI_BYTE   0x2C

#define CC_HI_BYTE    0x32

//--------------------------------------------------
// BATTERY VARIABLES
//--------------------------------------------------

float cellVoltage[4];

float packVoltage = 0;
float packCurrent = 0;

float temperature = 0;

float soh = 100;

//--------------------------------------------------
// BATTERY CAPACITY
//--------------------------------------------------

float ratedCapacity = 2500.0;
float measuredCapacity = 2400.0;

//--------------------------------------------------
// SHUNT RESISTOR
//--------------------------------------------------

const float SHUNT_RESISTOR = 0.005;

//--------------------------------------------------
// LCD SCREEN VARIABLES
//--------------------------------------------------

unsigned long previousLCDMillis = 0;
const long lcdInterval = 3000;
int screenIndex = 0;

//--------------------------------------------------
// FAULT VARIABLES
//--------------------------------------------------

String faultMessage = "SYSTEM NORMAL";
String mosfetStatus = "ALL ON";

bool balancingCell[4] =
{
  false,
  false,
  false,
  false
};

//--------------------------------------------------
// BUILD FAULT BITMASK
//   Matches the Android app's FaultEvent bitmask:
//   Bit 0 = OV, Bit 1 = OC, Bit 2 = OT, Bit 3 = UV
//--------------------------------------------------

int buildFaultBitmask()
{
  int faults = 0;

  for (int i = 0; i < 4; i++)
  {
    if (cellVoltage[i] > 4.2f) faults |= 0x01; // Overvoltage
    if (cellVoltage[i] < 3.0f) faults |= 0x08; // Undervoltage
  }

  if (abs(packCurrent) > 20)  faults |= 0x02;  // Overcurrent
  if (temperature > 60)       faults |= 0x04;  // Overtemperature

  return faults;
}



//==================================================
// WRITE REGISTER
//==================================================

void writeRegister(uint8_t reg, uint8_t data)
{
  Wire.beginTransmission(BQ76920_ADDR);

  Wire.write(reg);

  Wire.write(data);

  Wire.endTransmission();
}

//==================================================
// READ REGISTER
//==================================================

uint8_t readRegister(uint8_t reg)
{
  Wire.beginTransmission(BQ76920_ADDR);

  Wire.write(reg);

  Wire.endTransmission(false);

  Wire.requestFrom(BQ76920_ADDR, 1);

  if (Wire.available())
  {
    return Wire.read();
  }

  return 0;
}

//==================================================
// READ 16-BIT REGISTER
//==================================================

uint16_t readCellRaw(uint8_t reg)
{
  uint8_t hi = readRegister(reg);

  uint8_t lo = readRegister(reg + 1);

  return (((uint16_t)(hi & 0x3F)) << 8) | lo;
}

//==================================================
// INITIALIZE BQ76920
//==================================================

void initBQ76920()
{
  // Enable ADC

  writeRegister(SYS_CTRL1, 0x19);

  // Enable Coulomb Counter

  writeRegister(SYS_CTRL2, 0x40);

  Serial.println("BQ76920 Initialized");
}

//==================================================
// READ CELL VOLTAGES
//==================================================

void readCellVoltages()
{
   uint16_t raw1 = readCellRaw(VC1_HI_BYTE);
   uint16_t raw2 = readCellRaw(VC2_HI_BYTE);
   uint16_t raw3 = readCellRaw(VC3_HI_BYTE);
   uint16_t raw5 = readCellRaw(VC5_HI_BYTE);

   cellVoltage[0] = raw1 * 0.001 / 2.60;
   cellVoltage[1] = raw2 * 0.001 / 2.60;
   cellVoltage[2] = raw3 * 0.001 / 2.60;
   cellVoltage[3] = raw5 * 0.001 / 2.60;
   
   packVoltage =
   cellVoltage[0] +
   cellVoltage[1] +
   cellVoltage[2] +
   cellVoltage[3];
}
//==================================================
// READ CURRENT
//==================================================

void readCurrent()
{
  int16_t rawCurrent = readCellRaw(CC_HI_BYTE);

  packCurrent =
      (rawCurrent * 8.44e-6) /
      SHUNT_RESISTOR;
}

//==================================================
// READ TEMPERATURE
//==================================================

void readTemperature()
{
  uint16_t rawTemp = readCellRaw(TS1_HI_BYTE);

  //--------------------------------
  // INVALID SENSOR CHECK
  //--------------------------------

  if(rawTemp == 0)
  {
    temperature = 25.0;

    return;
  }

  //--------------------------------
  // SIMPLE STABLE CONVERSION
  //--------------------------------

  float voltage = rawTemp * 0.000382;

  temperature =
      25.0 +
      ((1.20 - voltage) * 25.0);

  //--------------------------------
  // LIMITS
  //--------------------------------

  if(temperature < 0)
  {
    temperature = 0;
  }

  if(temperature > 100)
  {
    temperature = 100;
  }
}

//==================================================
// CALCULATE SOH
//==================================================

void calculateSOH()
{
  soh =
      (measuredCapacity / ratedCapacity)
      * 100.0;
}

//==================================================
// ENABLE CELL BALANCING
//==================================================

void enableBalancing(int cell)
{
  uint8_t balanceReg = readRegister(CELLBAL1);

  balanceReg |= (1 << cell);

  writeRegister(CELLBAL1, balanceReg);

  balancingCell[cell] = true;
}

//==================================================
// DISABLE CELL BALANCING
//==================================================

void disableBalancing(int cell)
{
  uint8_t balanceReg = readRegister(CELLBAL1);

  balanceReg &= ~(1 << cell);

  writeRegister(CELLBAL1, balanceReg);

  balancingCell[cell] = false;
}

//==================================================
// DISABLE CHARGING MOSFET
//==================================================

void disableCharging()
{
  uint8_t reg = readRegister(SYS_CTRL2);

  reg &= ~(1 << 0);

  writeRegister(SYS_CTRL2, reg);

  mosfetStatus = "CHG OFF";
}

//==================================================
// DISABLE DISCHARGING MOSFET
//==================================================

void disableDischarging()
{
  uint8_t reg = readRegister(SYS_CTRL2);

  reg &= ~(1 << 1);

  writeRegister(SYS_CTRL2, reg);

  mosfetStatus = "DSG OFF";
}

//==================================================
// ENABLE BOTH MOSFETS
//==================================================

void enableMOSFETs()
{
  uint8_t reg = readRegister(SYS_CTRL2);

  reg |= (1 << 0);

  reg |= (1 << 1);

  writeRegister(SYS_CTRL2, reg);

  mosfetStatus = "ALL ON";
}

//==================================================
// PROTECTION CHECKS
//==================================================

void protectionCheck()
{
  faultMessage = "SYSTEM NORMAL";

  enableMOSFETs();

  //--------------------------------
  // CELL PROTECTION
  //--------------------------------

  for(int i = 0; i < 4; i++)
  {
    //--------------------------------
    // OVERVOLTAGE
    //--------------------------------

    if(cellVoltage[i] > 4.2)
    {
      disableCharging();

      enableBalancing(i);

      faultMessage =
        "OV CELL " + String(i + 1);

      Serial.println("================================");

      Serial.print("OVERVOLTAGE ON CELL ");

      Serial.println(i + 1);

      Serial.println("CHG MOSFET DISABLED");

      Serial.print("BALANCING ACTIVE CELL ");

      Serial.println(i + 1);

      Serial.println("================================");
    }

    //--------------------------------
    // UNDERVOLTAGE
    //--------------------------------

    else if(cellVoltage[i] < 3.0)
    {
      disableDischarging();

      faultMessage =
        "UV CELL " + String(i + 1);

      Serial.println("================================");

      Serial.print("UNDERVOLTAGE ON CELL ");

      Serial.println(i + 1);

      Serial.println("DSG MOSFET DISABLED");

      Serial.println("================================");
    }

    //--------------------------------
    // NORMAL CELL
    //--------------------------------

    else
    {
      disableBalancing(i);
    }
  }

  //--------------------------------
  // OVERCURRENT
  //--------------------------------

  if(abs(packCurrent) > 20)
  {
    disableDischarging();

    faultMessage = "OVERCURRENT";

    Serial.println("================================");

    Serial.println("OVERCURRENT DETECTED");

    Serial.println("DSG MOSFET DISABLED");

    Serial.println("================================");
  }

  //--------------------------------
  // OVERTEMPERATURE
  //--------------------------------

  if(temperature > 60)
  {
    disableCharging();

    disableDischarging();

    faultMessage = "OVERTEMP";

    Serial.println("================================");

    Serial.println("OVERTEMP DETECTED");

    Serial.println("ALL MOSFETS DISABLED");

    Serial.println("================================");
  }
}

//==================================================
// SERIAL MONITOR OUTPUT
//==================================================

void printData()
{
  Serial.println("========== SMART BMS ==========");

  for(int i = 0; i < 4; i++)
  {
    Serial.print("Cell ");

    Serial.print(i + 1);

    Serial.print(": ");

    Serial.print(cellVoltage[i], 3);

    Serial.print(" V");

    if(balancingCell[i])
    {
      Serial.print(" | BALANCING ACTIVE");
    }

    Serial.println();
  }

  Serial.println("-------------------------------");

  Serial.print("Pack Voltage: ");

  Serial.print(packVoltage, 2);

  Serial.println(" V");

  Serial.print("Current: ");

  Serial.print(packCurrent, 2);

  Serial.println(" A");

  Serial.print("Temperature: ");

  Serial.print(temperature, 1);

  Serial.println(" C");

  Serial.print("Battery SoH: ");

  Serial.print(soh, 1);

  Serial.println("%");

  Serial.print("Fault: ");

  Serial.println(faultMessage);

  Serial.print("MOSFET STATUS: ");

  Serial.println(mosfetStatus);

  Serial.println("===============================");
}

//==================================================
// LCD DISPLAY SYSTEM
//==================================================

void displayLCD()
{
  unsigned long currentMillis = millis();

  //--------------------------------
  // SWITCH SCREEN EVERY 3 SECONDS
  //--------------------------------

  if(currentMillis - previousLCDMillis >= lcdInterval)
  {
    previousLCDMillis = currentMillis;

    screenIndex++;

    if(screenIndex > 4)
    {
      screenIndex = 0;
    }

    lcd.clear();
  }

  //--------------------------------
  // SCREEN 1
  //--------------------------------

  if(screenIndex == 0)
  {
    lcd.setCursor(0,0);

    lcd.print("V:");

    lcd.print(packVoltage,1);

    lcd.print(" I:");

    lcd.print(packCurrent,1);

    lcd.setCursor(0,1);

    lcd.print("Temp:");

    lcd.print(temperature,1);

    lcd.print("C");
  }

  //--------------------------------
  // SCREEN 2
  //--------------------------------

  else if(screenIndex == 1)
  {
    lcd.setCursor(0,0);

    lcd.print("C1:");

    lcd.print(cellVoltage[0],2);

    lcd.print(" ");

    lcd.print("C2:");

    lcd.print(cellVoltage[1],2);

    lcd.setCursor(0,1);

    lcd.print("C3:");

    lcd.print(cellVoltage[2],2);

    lcd.print(" ");

    lcd.print("C4:");

    lcd.print(cellVoltage[3],2);
  }

  //--------------------------------
  // SCREEN 3
  //--------------------------------

  else if(screenIndex == 2)
  {
    lcd.setCursor(0,0);

    lcd.print("Battery SoH");

    lcd.setCursor(0,1);

    lcd.print(soh,1);

    lcd.print("%");
  }

  //--------------------------------
  // SCREEN 4
  //--------------------------------

  else if(screenIndex == 3)
  {
    lcd.setCursor(0,0);

    lcd.print("FAULT:");

    lcd.setCursor(0,1);

    lcd.print(faultMessage);
  }

  //--------------------------------
  // SCREEN 5
  //--------------------------------

  else if(screenIndex == 4)
  {
    lcd.setCursor(0,0);

    lcd.print("MOSFET:");

    lcd.setCursor(0,1);

    lcd.print(mosfetStatus);
  }
}

//==================================================
// SETUP
//==================================================

void setup()
{
  Serial.begin(115200);

  //--------------------------------
  // START I2C
  //--------------------------------

  Wire.begin(SDA_PIN, SCL_PIN);

  //--------------------------------
  // LCD INITIALIZATION
  //--------------------------------

  lcd.init();

  lcd.backlight();

  lcd.setCursor(0,0);

  lcd.print("SMART BMS");

  lcd.setCursor(0,1);

  lcd.print("Initializing");

  delay(2000);

  //--------------------------------
  // INITIALIZE BQ76920
  //--------------------------------

  initBQ76920();

  // ▼▼▼ MQTT ADDITIONS ▼▼▼
  //--------------------------------
  // CONNECT WIFI + MQTT
  //--------------------------------

  connectWiFi();

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  reconnectMQTT();
  // ▲▲▲ MQTT ADDITIONS ▲▲▲
}

//==================================================
// MAIN LOOP
//==================================================

void loop()
{
  //--------------------------------
  // READ BATTERY DATA
  //--------------------------------

  readCellVoltages();

  readCurrent();

  readTemperature();

  calculateSOH();

  //--------------------------------
  // RUN PROTECTION LOGIC
  //--------------------------------

  protectionCheck();

  //--------------------------------
  // SERIAL OUTPUT
  //--------------------------------

  printData();

  //--------------------------------
  // LCD OUTPUT
  //--------------------------------

  displayLCD();

  // ▼▼▼ MQTT ADDITIONS ▼▼▼
  //--------------------------------
  // MQTT: keep connection alive +
  // publish every mqttInterval ms
  //--------------------------------

  mqttClient.loop();     // must be called every loop iteration

  reconnectMQTT();       // no-op if already connected

  unsigned long now = millis();

  if (now - previousMqttMillis >= mqttInterval)
  {
    previousMqttMillis = now;
    publishBmsData();
  }
  // ▲▲▲ MQTT ADDITIONS ▲▲▲

  delay(1000);
}
