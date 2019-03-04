/*!
 *
 *  @file Adafruit_STMPE610.cpp
 *
 *  @mainpage Adafruit STMPE610 Resistive Touch Screen Controller
 *
 *  @section intro_sec Introduction
 *
 *  This is a library for the Adafruit STMPE610 Resistive
 *  touch screen controller breakout
 *  ----> http://www.adafruit.com/products/1571
 *
 *  Check out the links above for our tutorials and wiring diagrams
 *  These breakouts use SPI or I2C to communicate
 *
 *  Adafruit invests time and resources providing this open source code,
 *  please support Adafruit and open-source hardware by purchasing
 *  products from Adafruit!
 *
 *  @section author Author
 *
 *  Written by Limor Fried/Ladyada for Adafruit Industries.
 *
 *  @section license License
 *
 *  MIT license, all text above must be included in any redistribution
 */

#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include <Wire.h>
#include <SPI.h>

#include "Adafruit_STMPE610.h"

#if defined(SPI_HAS_TRANSACTION)
// SPI transaction support allows managing SPI settings and prevents
// conflicts between libraries, with a hardware-neutral interface
static SPISettings mySPISettings;
#elif defined(__AVR__)
static uint8_t SPCRbackup;
static uint8_t mySPCR;
#endif

/*!
 *  @brief  Instantiates a new STMPE610 class
 *  @param  cspin
 *          CS pin
 *  @param  mosipin
 *          MOSI pin
 *  @param  misopin
 *          MISO pin
 *  @param  clkpin
 *          CLK pin
 */
Adafruit_STMPE610::Adafruit_STMPE610(uint8_t cspin, uint8_t mosipin,
                                     uint8_t misopin, uint8_t clkpin) {
  _CS = cspin;
  _MOSI = mosipin;
  _MISO = misopin;
  _CLK = clkpin;
}

/*!
 *  @brief  Instantiates a new STMPE610 class
 *  @param  cspin
 *          CS pin
 */
Adafruit_STMPE610::Adafruit_STMPE610(uint8_t cspin) {
  _CS = cspin;
  _MOSI = _MISO = _CLK = -1;
}

/*!
 *  @brief  Instantiates a new STMPE610 using provided wire
 *  @param  *theWire
 */
Adafruit_STMPE610::Adafruit_STMPE610(TheWire *theWire) {
  _CS = _MISO = _MOSI = _CLK = -1;
  _wire = theWire;
}

/*!
 *  @brief  Instantiates a new STMPE610 using provided SPI
 *  @param  *theSPI
 */
Adafruit_STMPE610::Adafruit_STMPE610(uint8_t cspin, uint8_t clkpin,
                                     SPIClass *theSPI) {
  _CS = cspin;
  _CLK = clkpin;
  _MOSI = _MISO = -1;
  _spi = theSPI;
}

/*!
 *  @brief  Instantiates a new STMPE610 class
 */
Adafruit_STMPE610::Adafruit_STMPE610() {
  // use i2c
  _CS = -1;
}

/*!
 *  @brief  Setups the HW
 *  @param  i2caddr
 *          I2C address (defaults to STMPE_ADDR)
 *  @return True if process is successful
 */
boolean Adafruit_STMPE610::begin(uint8_t i2caddr) {
  if (_CS != -1 && _CLK == -1) {
    // hardware SPI
    pinMode(_CS, OUTPUT);
    digitalWrite(_CS, HIGH);

#if defined(SPI_HAS_TRANSACTION)
    _spi->begin();
    mySPISettings = SPISettings(1000000, MSBFIRST, SPI_MODE0);
#elif defined(__AVR__)
    SPCRbackup = SPCR;
    _spi->begin();
    _spi->setClockDivider(SPI_CLOCK_DIV16);
    _spi->setDataMode(SPI_MODE0);
    mySPCR = SPCR; // save our preferred state
    // Serial.print("mySPCR = 0x"); Serial.println(SPCR, HEX);
    SPCR = SPCRbackup; // then restore
#elif defined(__arm__)
    _spi->begin();
    _spi->setClockDivider(84);
    _spi->setDataMode(SPI_MODE0);
#endif
    m_spiMode = SPI_MODE0;
  } else if (_CS != -1) {
    // software SPI
    pinMode(_CLK, OUTPUT);
    pinMode(_CS, OUTPUT);
    pinMode(_MOSI, OUTPUT);
    pinMode(_MISO, INPUT);
  } else {
    _wire->begin();
    _i2caddr = i2caddr;
  }

  // try mode0
  if (getVersion() != 0x811) {
    if (_CS != -1 && _CLK == -1) {
      // Serial.println("try MODE1");
#if defined(SPI_HAS_TRANSACTION)
      mySPISettings = SPISettings(1000000, MSBFIRST, SPI_MODE1);
#elif defined(__AVR__)
      SPCRbackup = SPCR;
      _spi->begin();
      _spi->setDataMode(SPI_MODE1);
      _spi->setClockDivider(SPI_CLOCK_DIV16);
      mySPCR = SPCR; // save our new preferred state
      // Serial.print("mySPCR = 0x"); Serial.println(SPCR, HEX);
      SPCR = SPCRbackup; // then restore
#elif defined(__arm__)
      _spi->setClockDivider(84);
      _spi->setDataMode(SPI_MODE1);
#endif
      m_spiMode = SPI_MODE1;

      if (getVersion() != 0x811) {
        return false;
      }
    } else {
      return false;
    }
  }
  writeRegister8(STMPE_SYS_CTRL1, STMPE_SYS_CTRL1_RESET);
  delay(10);

  for (uint8_t i = 0; i < 65; i++) {
    readRegister8(i);
  }

  writeRegister8(STMPE_SYS_CTRL2, 0x0); // turn on clocks!
  writeRegister8(STMPE_TSC_CTRL,
                 STMPE_TSC_CTRL_XYZ | STMPE_TSC_CTRL_EN); // XYZ and enable!
  // Serial.println(readRegister8(STMPE_TSC_CTRL), HEX);
  writeRegister8(STMPE_INT_EN, STMPE_INT_EN_TOUCHDET);
  writeRegister8(STMPE_ADC_CTRL1, STMPE_ADC_CTRL1_10BIT |
                                      (0x6 << 4)); // 96 clocks per conversion
  writeRegister8(STMPE_ADC_CTRL2, STMPE_ADC_CTRL2_6_5MHZ);
  writeRegister8(STMPE_TSC_CFG, STMPE_TSC_CFG_4SAMPLE |
                                    STMPE_TSC_CFG_DELAY_1MS |
                                    STMPE_TSC_CFG_SETTLE_5MS);
  writeRegister8(STMPE_TSC_FRACTION_Z, 0x6);
  writeRegister8(STMPE_FIFO_TH, 1);
  writeRegister8(STMPE_FIFO_STA, STMPE_FIFO_STA_RESET);
  writeRegister8(STMPE_FIFO_STA, 0); // unreset
  writeRegister8(STMPE_TSC_I_DRIVE, STMPE_TSC_I_DRIVE_50MA);
  writeRegister8(STMPE_INT_STA, 0xFF); // reset all ints
  writeRegister8(STMPE_INT_CTRL,
                 STMPE_INT_CTRL_POL_HIGH | STMPE_INT_CTRL_ENABLE);

#if defined(__AVR__) && !defined(SPI_HAS_TRANSACTION)
  if (_CS != -1 && _CLK == -1)
    SPCR = SPCRbackup; // restore SPI state
#endif
  return true;
}

/*!
 *  @brief  Returns true if touched, false otherwise
 *  @return True if if touched, false otherwise
 */
boolean Adafruit_STMPE610::touched() {
  return (readRegister8(STMPE_TSC_CTRL) & 0x80);
}

/*!
 *  @brief  Returns true if touched, false otherwise
 *  @return True if if touched, false otherwise
 */
boolean Adafruit_STMPE610::bufferEmpty() {
  return (readRegister8(STMPE_FIFO_STA) & STMPE_FIFO_STA_EMPTY);
}

/*!
 *  @brief  Returns the FIFO buffer size
 *  @return The FIFO buffer size
 */
uint8_t Adafruit_STMPE610::bufferSize() {
  return readRegister8(STMPE_FIFO_SIZE);
}

/*!
 *  @brief  Returns the STMPE610 version number
 *  @return The STMPE610 version number
 */
uint16_t Adafruit_STMPE610::getVersion() {
  uint16_t v;
  // Serial.print("get version");
  v = readRegister8(0);
  v <<= 8;
  v |= readRegister8(1);
  // Serial.print("Version: 0x"); Serial.println(v, HEX);
  return v;
}

/*!
 *  @brief  Reads touchscreen data
 *  @param  *x
 *	    The x coordinate
 *  @param  *y
 *	    The y coordinate
 *  @param  *z
 *	    The z coordinate
 */
void Adafruit_STMPE610::readData(uint16_t *x, uint16_t *y, uint8_t *z) {
  uint8_t data[4];

  for (uint8_t i = 0; i < 4; i++) {
    data[i] = readRegister8(0xD7); // _spi->transfer(0x00);
    // Serial.print("0x"); Serial.print(data[i], HEX); Serial.print(" / ");
  }
  *x = data[0];
  *x <<= 4;
  *x |= (data[1] >> 4);
  *y = data[1] & 0x0F;
  *y <<= 8;
  *y |= data[2];
  *z = data[3];

  if (bufferEmpty())
    writeRegister8(STMPE_INT_STA, 0xFF); // reset all ints
}

/*!
 *  @brief  Returns point for touchscreen data
 *  @return The touch point using TS_Point
 */
TS_Point Adafruit_STMPE610::getPoint() {
  uint16_t x, y;
  uint8_t z;
  readData(&x, &y, &z);
  return TS_Point(x, y, z);
}

/*!
 *  @brief  Reads SPI data
 */
uint8_t Adafruit_STMPE610::spiIn() {
  if (_CLK == -1) {
#if defined(SPI_HAS_TRANSACTION)
    uint8_t d = _spi->transfer(0);
    return d;
#elif defined(__AVR__)
    SPCRbackup = SPCR;
    SPCR = mySPCR;
    uint8_t d = _spi->transfer(0);
    SPCR = SPCRbackup;
    return d;
#elif defined(__arm__)
    _spi->setClockDivider(84);
    _spi->setDataMode(m_spiMode);
    uint8_t d = _spi->transfer(0);
    return d;
#endif
  } else
    return shiftIn(_MISO, _CLK, MSBFIRST);
}

/*!
 *  @brief  Sends data through SPI
 *  @param  x
 *          Data to send (one byte)
 */
void Adafruit_STMPE610::spiOut(uint8_t x) {
  if (_CLK == -1) {
#if defined(SPI_HAS_TRANSACTION)
    _spi->transfer(x);
#elif defined(__AVR__)
    SPCRbackup = SPCR;
    SPCR = mySPCR;
    _spi->transfer(x);
    SPCR = SPCRbackup;
#elif defined(__arm__)
    _spi->setClockDivider(84);
    _spi->setDataMode(m_spiMode);
    _spi->transfer(x);
#endif
  } else
    shiftOut(_MOSI, _CLK, MSBFIRST, x);
}

/*!
 *  @brief  Reads data from specified register
 *  @param  reg
 *          The register
 *  @return Data in the register
 */
uint8_t Adafruit_STMPE610::readRegister8(uint8_t reg) {
  uint8_t x;
  if (_CS == -1) {
    // use i2c
    _wire->beginTransmission(_i2caddr);
    _wire->write((byte)reg);
    _wire->endTransmission();
    _wire->beginTransmission(_i2caddr);
    _wire->requestFrom(_i2caddr, (byte)1);
    x = _wire->read();
    _wire->endTransmission();

    // Serial.print("$"); Serial.print(reg, HEX);
    // Serial.print(": 0x"); Serial.println(x, HEX);
  } else {
#if defined(SPI_HAS_TRANSACTION)
    if (_CLK == -1)
      _spi->beginTransaction(mySPISettings);
#endif
    digitalWrite(_CS, LOW);
    spiOut(0x80 | reg);
    spiOut(0x00);
    x = spiIn();
    digitalWrite(_CS, HIGH);
#if defined(SPI_HAS_TRANSACTION)
    if (_CLK == -1)
      _spi->endTransaction();
#endif
  }

  return x;
}

/*!
 *  @brief  Reads data from specified register
 *  @param  reg
 *          The register
 *  @return Data in the register
 */
uint16_t Adafruit_STMPE610::readRegister16(uint8_t reg) {
  uint16_t x = 0;
  if (_CS == -1) {
    // use i2c
    _wire->beginTransmission(_i2caddr);
    _wire->write((byte)reg);
    _wire->endTransmission();
    _wire->requestFrom(_i2caddr, (byte)2);
    x = _wire->read();
    x <<= 8;
    x |= _wire->read();
    _wire->endTransmission();
  }
  if (_CLK == -1) {
    // hardware SPI
#if defined(SPI_HAS_TRANSACTION)
    if (_CLK == -1)
      _spi->beginTransaction(mySPISettings);
#endif
    digitalWrite(_CS, LOW);
    spiOut(0x80 | reg);
    spiOut(0x00);
    x = spiIn();
    x <<= 8;
    x |= spiIn();
    digitalWrite(_CS, HIGH);
#if defined(SPI_HAS_TRANSACTION)
    if (_CLK == -1)
      _spi->endTransaction();
#endif
  }

  // Serial.print("$"); Serial.print(reg, HEX);
  // Serial.print(": 0x"); Serial.println(x, HEX);
  return x;
}

/*!
 *  @brief  Writes data to specified register
 *  @param  reg
 *	    The register
 *  @param  val
 *          Value to write
 */
void Adafruit_STMPE610::writeRegister8(uint8_t reg, uint8_t val) {
  if (_CS == -1) {
    // use i2c
    _wire->beginTransmission(_i2caddr);
    _wire->write((byte)reg);
    _wire->write(val);
    _wire->endTransmission();
  } else {
#if defined(SPI_HAS_TRANSACTION)
    if (_CLK == -1)
      _spi->beginTransaction(mySPISettings);
#endif
    digitalWrite(_CS, LOW);
    spiOut(reg);
    spiOut(val);
    digitalWrite(_CS, HIGH);
#if defined(SPI_HAS_TRANSACTION)
    if (_CLK == -1)
      _spi->endTransaction();
#endif
  }
}

/*!
 *  @brief  TS_Point constructor
 */
TS_Point::TS_Point() { x = y = 0; }

/*!
 *  @brief  TS_Point constructor
 *  @param  x0
 *          Initial x
 *  @param  y0
 *          Initial y
 *  @param  z0
 *          Initial z
 */
TS_Point::TS_Point(int16_t x0, int16_t y0, int16_t z0) {
  x = x0;
  y = y0;
  z = z0;
}

/*!
 *  @brief  Equality operator for TS_Point
 *  @return True if points are equal
 */
bool TS_Point::operator==(TS_Point p1) {
  return ((p1.x == x) && (p1.y == y) && (p1.z == z));
}

/*!
 *  @brief  Non-equality operator for TS_Point
 *  @return True if points are not equal
 */
bool TS_Point::operator!=(TS_Point p1) {
  return ((p1.x != x) || (p1.y != y) || (p1.z != z));
}
