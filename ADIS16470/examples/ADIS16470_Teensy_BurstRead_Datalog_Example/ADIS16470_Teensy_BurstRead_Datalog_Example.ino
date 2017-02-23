////////////////////////////////////////////////////////////////////////////////////////////////////////
//  February 2017
//  Author: Juan Jose Chong <juan.chong@analog.com>
////////////////////////////////////////////////////////////////////////////////////////////////////////
//  ADIS16470_Teensy_BurstRead_Example.ino
////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
//  This Arduino project interfaces with an ADIS16470 using SPI and the 
//  accompanying C++ libraries, reads IMU data in LSBs, scales the data, and 
//  outputs measurements to a serial debug terminal (PuTTY) via the onboard 
//  USB serial port.
//
//  This library shows the user how to capture data synchronously and display it to a serial port.
//  Due to the processing speed and bandwidth limitations of the Teensy, serial data must be
//  printed at a slower rate than what is captured from the sensor. In most applications, 
//  code would be inserted into the ISR in order to perform sensor integration, filtering, etc.  
//
//  This project has been tested on a PJRC 32-Bit Teensy 3.2 Development Board, 
//  but should be compatible with any other embedded platform with some modification.
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
//  Pinout for a Teensy 3.2 Development Board
//  RST = D6
//  SCK = D13/SCK
//  CS = D10/CS
//  DOUT(MISO) = D12/MISO
//  DIN(MOSI) = D11/MOSI
//  DR = D2
//
////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <ADIS16470.h>
#include <SPI.h>

// Initialize Variables
// Temporary Data Array
int16_t *burstData;

// Checksum variable
int16_t burstChecksum = 0;

// Scaled data array
float scaledData[7];

// Ping-pong variables
float datapacket1[11]; 
float datapacket2[11]; 
bool pingpong = true; 
bool freezepingpong = false; 

// Delay counter variable 
int printCounter = 0; 

// Serial packet variables
String serialpacket = "";
String separator = ',';

// Change printing from Serial1 (pins 0,1) to Serial (USB)
#define HWSERIAL Serial

// Call ADIS16470 Class
ADIS16470 IMU(10,2,6); // Chip Select, Data Ready, Reset Pin Assignments

void setup()
{
    Serial.begin(115200); // Initialize serial output via USB
    IMU.configSPI(); // Configure SPI communication
    delay(500); // Give the part time to start up
    IMU.regWrite(MSC_CTRL, 0xC1);  // Enable Data Ready, set polarity
    IMU.regWrite(FILT_CTRL, 0x04); // Set digital filter
    IMU.regWrite(DEC_RATE, 0), // Disable decimation
    attachInterrupt(2, grabData, RISING); // Attach interrupt to pin 2. Trigger on the rising edge
}

// Function used to read register values when an ISR is triggered using the IMU's DataReady output
void grabData()
{
    burstData = IMU.burstRead(); // Read data and insert into array
    burstChecksum = IMU.checksum(burstData); // Calculate checksum based on data array
    scaleData(); // Scale sensor output data

    // PingPong between arrays such that data is not overwritten when writing  
    //  to the serial port while an ISR is generated. Disabling the ISR is not  
    //  acceptable because data would potentially be lost when ISRs weren't serviced. 
    // Note that this is important when performing calculations in the embedded processor.
 
    // Ping array 
    if(pingpong == true) { 
        datapacket1[0] = (float)(*(burstData + 0)); //DIAG_STAT
        datapacket1[1] = scaledData[0]; //XGYRO
        datapacket1[2] = scaledData[1]; //YGYRO
        datapacket1[3] = scaledData[2]; //ZGYRO
        datapacket1[4] = scaledData[3]; //XACCL
        datapacket1[5] = scaledData[4]; //YACCL
        datapacket1[6] = scaledData[5]; //ZACCL
        datapacket1[7] = scaledData[6]; //TEMP
        datapacket1[8] = (float)(*(burstData + 8)); //TIME_STMP
        datapacket1[9] = (float)(*(burstData + 9)); //CHECKSUM
        if (burstChecksum == *(burstData + 9)) 
            datapacket1[10] = 1; // 1 = Valid Checksum, 0 = Invalid Checksum
        else
            datapacket1[10] = 0;
    }


    // Pong array 
    if(pingpong == false) { 
        datapacket2[0] = (float)(*(burstData + 0)); //DIAG_STAT
        datapacket2[1] = scaledData[0]; //XGYRO
        datapacket2[2] = scaledData[1]; //YGYRO
        datapacket2[3] = scaledData[2]; //ZGYRO
        datapacket2[4] = scaledData[3]; //XACCL
        datapacket2[5] = scaledData[4]; //YACCL
        datapacket2[6] = scaledData[5]; //ZACCL
        datapacket2[7] = scaledData[6]; //TEMP
        datapacket2[8] = (float)(*(burstData + 8)); //TIME_STMP
        datapacket2[9] = (float)(*(burstData + 9)); //CHECKSUM
        if (burstChecksum == *(burstData + 9)) 
            datapacket2[10] = 1; // 1 = Valid Checksum, 0 = Invalid Checksum
        else
            datapacket2[10] = 0;
    }
    // Continue ping-pong-ing until frozen 
    if(freezepingpong == false) { 
        pingpong = !pingpong; 
    } 
}

// Function used to scale all acquired data (scaling functions are included in ADIS16470.cpp)
void scaleData()
{
    scaledData[0] = IMU.gyroScale(*(burstData + 1)); //Scale X Gyro
    scaledData[1] = IMU.gyroScale(*(burstData + 2)); //Scale Y Gyro
    scaledData[2] = IMU.gyroScale(*(burstData + 3)); //Scale Z Gyro
    scaledData[3] = IMU.accelScale(*(burstData + 4)); //Scale X Accel
    scaledData[4] = IMU.accelScale(*(burstData + 5)); //Scale Y Accel
    scaledData[5] = IMU.accelScale(*(burstData + 6)); //Scale Z Accel
    scaledData[6] = IMU.tempScale(*(burstData + 7)); //Scale Temp Sensor
}

// Main loop
void loop() {
  printCounter ++;
  if (printCounter >= 10000) // Delay for writing data to the serial port
  {
    // Build string data packet for frame from either ping or pong. Note that
    //  whichever array is currently being read to print to the serial port, 
    //  it will never be overwritten by servicing ISRs.
    freezepingpong = true;

    // Ping active
    if(pingpong == true) {
      for(int x = 0; x < 11; x++) {
        serialpacket += datapacket1[x];
        serialpacket += separator;
      }
    }
    // Pong active
    if(pingpong == false) {
      for(int x = 0; x < 11; x++) {
        serialpacket += datapacket2[x];
        serialpacket += separator;
      }
    }
  
    // Print data packet to serial port
    HWSERIAL.println(serialpacket);

    // Clear data packet
    serialpacket = "";

    // Unfreeze pingpong
    freezepingpong = false;
    
    // Reset print counter
    printCounter = 0;
  }
}
