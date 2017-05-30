/*
  Stanford Student Space Initiative
  Balloons | VALBAL | May 2017
  Davy Ragland | dragland@stanford.edu
  Michal Adamkiewicz | mikadam@stanford.edu

  File: Filters.h
  --------------------------
  Interface to guarenteed good derived values.
*/

#ifndef FILTERS_H
#define FILTERS_H

#include "Config.h"
//#include <SD.h>

class Filters {
public:
  // File debugFile;
/**********************************  SETUP  ***********************************/
  bool     init();
/********************************  FUNCTIONS  *********************************/
  void     enableSensors(bool BMP1Enable, bool BMP2Enable, bool BMP3Enable, bool BMP4Enable);
  double   getTemp(double RAW_TEMP_1,double RAW_TEMP_2,double RAW_TEMP_3,double RAW_TEMP_4);
  void     storeData(uint32_t time_stamp, double RAW_PRESSURE_1,double RAW_PRESSURE_2,double RAW_PRESSURE_3,double RAW_PRESSURE_4,double pressureBaselineArg);
  uint32_t getNumRejections(uint8_t sensor);

  double   getAvgCurrentSystem(double current);
  double   getAvgCurrentGPS(double current);
  double   getAvgCurrentRB(double current);
  double   getAvgCurrentMotorValve(double current,bool on);
  double   getAvgCurrentMotorBallast(double current,bool on);
  double   getAvgCurrentPayload(double current);

  double   getMinCurrentSystem();
  double   getMaxCurrentSystem();
  double   getMaxCurrentGPS();
  double   getMaxCurrentRB();
  double   getMaxCurrentMotorValve();
  double   getMaxCurrentMotorBallast();
  double   getMaxCurrentPayload();

  double   getPressure();
  double   getAltitude();
  double   getAscentRate();
  float    getIncentiveNoise(bool IncludeBMP1, bool IncludeBMP2, bool IncludeBMP3, bool IncludeBMP4);

  void     clearCurrentValues();

private:
/*********************************  HELPERS  **********************************/
  void     consensousCheck();
  void     velocityCheck();
  void     findLastAccepted();
  void     errorCheckAltitudes();
  double   calculateAltitude(double pressure);
  void     markFailure(uint8_t sensor);
/*********************************  OBJECTS  **********************************/
  bool     enabledSensors[4] = {true};
  uint32_t rejectedSensors[4] = {0};
  uint8_t  numSensors;

  double   currentSystemTotal = 0;
  double   currentSystemMax = 0;
  double   currentSystemMin = 10000;
  uint32_t currentSystemCount = 0;
  double   currentGPSTotal = 0;
  double   currentGPSMax = 0;
  uint32_t currentGPSCount = 0;
  double   currentRBTotal = 0;
  double   currentRBMax = 0;
  uint32_t currentRBCount = 0;
  double   currentMotorValveTotal = 0;
  double   currentMotorValveMax = 0;
  uint32_t currentMotorValveCount = 0;
  double   currentMotorBallastTotal = 0;
  double   currentMotorBallastMax = 0;
  uint32_t currentMotorBallastCount = 0;
  double   currentPayloadTotal = 0;
  double   currentPayloadMax = 0;
  uint32_t currentPayloadCount = 0;

  double   pressureBaseline;
  float    meanAscentRates[4];
  float    meanAltitudes[4];
  uint16_t altitudeIndex = 0;
  double   sampleTimeSeconds[ALTITUDE_BUFFER_SIZE] = {0};
  float    altitudeBuffer[4][ALTITUDE_BUFFER_SIZE] = {{0}};
  bool     altitudeErrors[4][ALTITUDE_BUFFER_SIZE] = {{false}};

  double   sumX[4] = {0};
  double   sumY[4] = {0};
  double   sumXY[4] = {0};
  double   sumX2[4] = {0};
  int      sampleCount[4] = {ALTITUDE_BUFFER_SIZE,ALTITUDE_BUFFER_SIZE,ALTITUDE_BUFFER_SIZE,ALTITUDE_BUFFER_SIZE};

  bool     firstBUFFER = true;

  float    lastAcceptedAltitudes[4];
  double   lastAcceptedTimes[4];
  double   pressures[4];
  bool     filtered = false;

};

#endif
