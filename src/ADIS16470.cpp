////////////////////////////////////////////////////////////////////////////////////////////////////////
//  November 2017
//  Author: Juan Jose Chong <juan.chong@analog.com>
////////////////////////////////////////////////////////////////////////////////////////////////////////
//  ADIS16470.cpp
////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
//  This library provides all the functions necessary to interface the ADIS16470 IMU with a 
//  PJRC 32-Bit Teensy 3.2 Development Board. Functions for SPI configuration, reads and writes,
//  and scaling are included. This library may be used for the entire ADIS1646X family of devices 
//  with some modification.
//
//  Permission is hereby granted, free of charge, to any person obtaining
//  a copy of this software and associated documentation files (the
//  "Software"), to deal in the Software without restriction, including
//  without limitation the rights to use, copy, modify, merge, publish,
//  distribute, sublicense, and/or sell copies of the Software, and to
//  permit persons to whom the Software is furnished to do so, subject to
//  the following conditions:
//
//  The above copyright notice and this permission notice shall be
//  included in all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
//  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
//  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
//  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
//  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
//  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
//  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ADIS16470.h"

////////////////////////////////////////////////////////////////////////////
// Constructor with configurable CS, DR, and RST
////////////////////////////////////////////////////////////////////////////
// CS - Chip select pin
// DR - DR output pin for data ready
// RST - Hardware reset pin
////////////////////////////////////////////////////////////////////////////
ADIS16470::ADIS16470(int CS, int DR, int RST) {
  _CS = CS;
  _DR = DR;
  _RST = RST;
  // Initialize SPI
  SPI.begin();
  // Set default pin states
  pinMode(_CS, OUTPUT); // Set CS pin to be an output
  pinMode(_DR, INPUT); // Set DR pin to be an input
  pinMode(_RST, OUTPUT); // Set RST pin to be an output
  digitalWrite(_CS, HIGH); // Initialize CS pin to be high
  digitalWrite(_RST, HIGH); // Initialize RST pin to be high
}

////////////////////////////////////////////////////////////////////////////
// Destructor
////////////////////////////////////////////////////////////////////////////
ADIS16470::~ADIS16470() {
}

////////////////////////////////////////////////////////////////////////////
// Performs a hardware reset by setting _RST pin low for delay (in ms).
// Returns 1 when complete.
////////////////////////////////////////////////////////////////////////////
int ADIS16470::resetDUT(uint8_t ms) {
  digitalWrite(_RST, LOW);
  delay(ms);
  digitalWrite(_RST, HIGH);
  delay(ms);
  return(1);
}

////////////////////////////////////////////////////////////////////////////
// Selects the ADIS16470 for read/write operations.
// Sets SPI bit order, clock divider, and data mode.
// Also sets chip select to LOW.
// This function is useful when there are multiple SPI devices
// using different settings.
// Returns 1 when complete.
////////////////////////////////////////////////////////////////////////////
int ADIS16470::select() {
  SPISettings IMUSettings(1000000, MSBFIRST, SPI_MODE3);
  SPI.beginTransaction(IMUSettings);
  digitalWrite(_CS, LOW); // Set CS low to enable device
  return (1);
}

////////////////////////////////////////////////////////////////////////////
// Deselects the ADIS16470 for read/write operations.
// Frees up the SPi bus for other devices.
// Also sets chip select to HIGH.
// Returns 1 when complete.
////////////////////////////////////////////////////////////////////////////
int ADIS16470::deselect() {
  SPI.endTransaction();
  digitalWrite(_CS, HIGH); // Set CS high to disable device
  return (1);
}

////////////////////////////////////////////////////////////////////////////////////////////
// Reads two bytes (one word) in two sequential registers over SPI
// Returns an (int) signed 16 bit 2's complement number
////////////////////////////////////////////////////////////////////////////////////////////
// regAddr - address of register to be read
////////////////////////////////////////////////////////////////////////////////////////////
int16_t ADIS16470::regRead(uint8_t regAddr) {
//Read registers using SPI
  
  // Write register address to be read
  select();              // select the device
  SPI.transfer(regAddr); // Write address over SPI bus
  SPI.transfer(0x00); // Write 0x00 to the SPI bus fill the 16 bit transaction requirement
  deselect();            // deselect the device

  delayMicroseconds(_stall); // Delay to not violate read rate 

  // Read data from requested register
  select();              // select the device
  uint8_t _msbData = SPI.transfer(0x00); // Send (0x00) and place upper byte into variable
  uint8_t _lsbData = SPI.transfer(0x00); // Send (0x00) and place lower byte into variable
  deselect();            // deselect the device

  delayMicroseconds(_stall); // Delay to not violate read rate 
  
  int16_t _dataOut = (_msbData << 8) | (_lsbData & 0xFF); // Concatenate upper and lower bytes
  // Shift MSB data left by 8 bits, mask LSB data with 0xFF, and OR both bits.

  return(_dataOut);
}

////////////////////////////////////////////////////////////////////////////
// Writes one byte of data to the specified register over SPI.
// Returns 1 when complete.
////////////////////////////////////////////////////////////////////////////
// regAddr - address of register to be written
// regData - data to be written to the register
////////////////////////////////////////////////////////////////////////////
int ADIS16470::regWrite(uint8_t regAddr, int16_t regData) {

  // Write register address and data
  uint16_t addr = (((regAddr & 0x7F) | 0x80) << 8); // Toggle sign bit, and check that the address is 8 bits
  uint16_t lowWord = (addr | (regData & 0xFF)); // OR Register address (A) with data(D) (AADD)
  uint16_t highWord = ((addr | 0x100) | ((regData >> 8) & 0xFF)); // OR Register address with data and increment address

  // Split words into chars
  uint8_t highBytehighWord = (highWord >> 8);
  uint8_t lowBytehighWord = (highWord & 0xFF);
  uint8_t highBytelowWord = (lowWord >> 8);
  uint8_t lowBytelowWord = (lowWord & 0xFF);

  // Write highWord to SPI bus
  select();              // select the device
  SPI.transfer(highBytelowWord); // Write high byte from low word to SPI bus
  SPI.transfer(lowBytelowWord); // Write low byte from low word to SPI bus
  deselect();            // deselect the device

  delayMicroseconds(_stall);; // Delay to not violate read rate 

  // Write lowWord to SPI bus
  select();              // select the device
  SPI.transfer(highBytehighWord); // Write high byte from high word to SPI bus
  SPI.transfer(lowBytehighWord); // Write low byte from high word to SPI bus
  deselect();            // deselect the device

  delayMicroseconds(_stall);; // Delay to not violate read rate 

  return(1);
}

////////////////////////////////////////////////////////////////////////////
// Intiates a burst read from the sensor.
// Returns a pointer to an array of sensor data. 
////////////////////////////////////////////////////////////////////////////
// No inputs required.
////////////////////////////////////////////////////////////////////////////
uint8_t *ADIS16470::byteBurst(void) {

  static uint8_t burstdata[20];

  // Trigger Burst Read
  select(); // select the device
  SPI.transfer(0x68);
  SPI.transfer(0x00);

  // Read Burst Data
  burstdata[0] = SPI.transfer(0x00); //DIAG_STAT
  burstdata[1] = SPI.transfer(0x00);
  burstdata[2] = SPI.transfer(0x00); //XGYRO_OUT
  burstdata[3] = SPI.transfer(0x00);
  burstdata[4] = SPI.transfer(0x00); //YGYRO_OUT
  burstdata[5] = SPI.transfer(0x00);
  burstdata[6] = SPI.transfer(0x00); //ZGYRO_OUT
  burstdata[7] = SPI.transfer(0x00);
  burstdata[8] = SPI.transfer(0x00); //XACCEL_OUT
  burstdata[9] = SPI.transfer(0x00);
  burstdata[10] = SPI.transfer(0x00); //YACCEL_OUT
  burstdata[11] = SPI.transfer(0x00);
  burstdata[12] = SPI.transfer(0x00); //ZACCEL_OUT
  burstdata[13] = SPI.transfer(0x00);
  burstdata[14] = SPI.transfer(0x00); //TEMP_OUT
  burstdata[15] = SPI.transfer(0x00);
  burstdata[16] = SPI.transfer(0x00); //TIME_STMP
  burstdata[17] = SPI.transfer(0x00);
  burstdata[18] = SPI.transfer(0x00); //CHECKSUM
  burstdata[19] = SPI.transfer(0x00);
  deselect(); // deselect the device

  return burstdata;

}

////////////////////////////////////////////////////////////////////////////
// Intiates a burst read from the sensor.
// Returns a pointer to an array of sensor data. 
////////////////////////////////////////////////////////////////////////////
// No inputs required.
////////////////////////////////////////////////////////////////////////////
uint16_t *ADIS16470::wordBurst(void) {

  static uint16_t burstwords[10];

  // Trigger Burst Read
  select(); // select the device
  SPI.transfer(0x68);
  SPI.transfer(0x00);

  // Read Burst Data
  burstwords[0] = ((SPI.transfer(0x00) << 8) | (SPI.transfer(0x00) & 0xFF)); //DIAG_STAT
  burstwords[1] = ((SPI.transfer(0x00) << 8) | (SPI.transfer(0x00) & 0xFF)); //XGYRO
  burstwords[2] = ((SPI.transfer(0x00) << 8) | (SPI.transfer(0x00) & 0xFF)); //YGYRO
  burstwords[3] = ((SPI.transfer(0x00) << 8) | (SPI.transfer(0x00) & 0xFF)); //ZGYRO
  burstwords[4] = ((SPI.transfer(0x00) << 8) | (SPI.transfer(0x00) & 0xFF)); //XACCEL
  burstwords[5] = ((SPI.transfer(0x00) << 8) | (SPI.transfer(0x00) & 0xFF)); //YACCEL
  burstwords[6] = ((SPI.transfer(0x00) << 8) | (SPI.transfer(0x00) & 0xFF)); //ZACCEL
  burstwords[7] = ((SPI.transfer(0x00) << 8) | (SPI.transfer(0x00) & 0xFF)); //TEMP_OUT
  burstwords[8] = ((SPI.transfer(0x00) << 8) | (SPI.transfer(0x00) & 0xFF)); //TIME_STMP
  burstwords[9] = ((SPI.transfer(0x00) << 8) | (SPI.transfer(0x00) & 0xFF)); //CHECKSUM

  deselect();  // deselect the device

  return burstwords;

}

////////////////////////////////////////////////////////////////////////////
// Calculates checksum based on burst data.
// Returns the calculated checksum.
////////////////////////////////////////////////////////////////////////////
// *burstArray - array of burst data
// return - (int16_t) signed calculated checksum
////////////////////////////////////////////////////////////////////////////
int16_t ADIS16470::checksum(uint16_t * burstArray) {
  int16_t s = 0;
  for (int i = 0; i < 9; i++) // Checksum value is not part of the sum!!
  {
      s += (burstArray[i] & 0xFF); // Count lower byte
      s += ((burstArray[i] >> 8) & 0xFF); // Count upper byte
  }

  return s;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Converts accelerometer data output from the regRead() function
// Returns (float) signed/scaled accelerometer in g's
/////////////////////////////////////////////////////////////////////////////////////////
// sensorData - data output from regRead()
/////////////////////////////////////////////////////////////////////////////////////////
float ADIS16470::accelScale(int16_t sensorData)
{
  float finalData = sensorData * 0.00125; // Multiply by accel sensitivity (0.00125g/LSB)
  return finalData;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Converts gyro data output from the regRead() function 
// Returns (float) signed/scaled gyro in degrees/sec
/////////////////////////////////////////////////////////////////////////////////////////////
// sensorData - data output from regRead()
/////////////////////////////////////////////////////////////////////////////////////////
float ADIS16470::gyroScale(int16_t sensorData)
{
  float finalData = sensorData * 0.1; // Multiply by gyro sensitivity (0.1 deg/LSB)
  return finalData;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Converts temperature data output from the regRead() function 
// Returns (float) signed/scaled temperature in degrees Celcius
/////////////////////////////////////////////////////////////////////////////////////////////
// sensorData - data output from regRead()
/////////////////////////////////////////////////////////////////////////////////////////
float ADIS16470::tempScale(int16_t sensorData)
{
  float finalData = (sensorData * 0.1); // Multiply by temperature scale (0.1 deg C/LSB)
  return finalData;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Converts integrated angle data output from the regRead() function 
// Returns (float) signed/scaled delta angle in degrees
/////////////////////////////////////////////////////////////////////////////////////////////
// sensorData - data output from regRead()
/////////////////////////////////////////////////////////////////////////////////////////
float ADIS16470::deltaAngleScale(int16_t sensorData)
{
  float finalData = sensorData * 0.061; // Multiply by delta angle scale (0.061 degrees/LSB)
  return finalData;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Converts integrated velocity data output from the regRead() function 
// Returns (float) signed/scaled delta velocity in m/sec
/////////////////////////////////////////////////////////////////////////////////////////////
// sensorData - data output from regRead()
/////////////////////////////////////////////////////////////////////////////////////////
float ADIS16470::deltaVelocityScale(int16_t sensorData)
{
  float finalData = sensorData * 0.01221; // Multiply by velocity scale (0.01221 m/sec/LSB)
  return finalData;
}
