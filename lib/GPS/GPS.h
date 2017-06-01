/*
  Stanford Student Space Initiative
  Balloons | VALBAL | June 2017
  Davy Ragland | dragland@stanford.edu
  Aria Tedjarati | satedjarati@stanford.edu

  File: GPS.h
  --------------------------
  Interface to Ublox NEO-M8Q GPS module.
*/

#ifndef GPS_H
#define GPS_H

#include <TinyGPS++.h>
#include <EEPROM.h>

class GPS {
public:
/**********************************  SETUP  ***********************************/
  GPS(uint8_t GPS_EnablePinNum, uint16_t GPS_BaudVal, uint8_t EEPROMAddressVal, uint16_t GPS_LockTime, uint16_t GPS_QuitTime) :
    GPS_ENABLE_PIN(GPS_EnablePinNum),
    GPS_BAUD(GPS_BaudVal),
    EEPROMAddress(EEPROMAddressVal),
    GPS_LOCK_TIME(GPS_LockTime),
    GPS_TIMEOUT_TIME(GPS_QuitTime) {
  }
  bool     init(bool shouldStartup);
/********************************  FUNCTIONS  *********************************/
  bool     restart();
  void     hotstart();
  void     shutdown();
  float    getLatitude();
  float    getLongitude();
  float    getAltitude();
  float    getSpeed();
  float    getCourse();
  uint8_t  getSats();
  void     smartDelay(uint32_t ms);
private:
/*********************************  HELPERS  **********************************/
  bool     setFlightMode(uint16_t GPS_LOCK_TIME);
  void     sendUBX(uint8_t* MSG, uint8_t len);
  bool     getUBX_ACK(uint8_t* MSG);
/*********************************  OBJECTS  **********************************/
  uint8_t  GPS_ENABLE_PIN;
  uint16_t GPS_BAUD;
  uint8_t  EEPROMAddress;
  uint16_t GPS_LOCK_TIME;
  uint16_t GPS_TIMEOUT_TIME;
  TinyGPSPlus tinygps;
};

#endif
