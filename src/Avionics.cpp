/*
  Stanford Student Space Initiative
  Balloons | VALBAL | May 2017
  Davy Ragland | dragland@stanford.edu
  Claire Huang | chuang20@stanford.edu
  Aria Tedjarati | satedjarati@stanford.edu
  Joan Creus-Costa | jcreus@stanford.edu

  File: Avionics.cpp
  --------------------------
  Implementation of Avionics.h
*/

#include "Avionics.h"

/**********************************  SETUP  ***********************************/
/*
 * Function: init
 * -------------------
 * This function initializes the avionics flight controller.
 */
void Avionics::init() {
  PCB.init();
  Serial.begin(CONSOLE_BAUD);
  if(!setupSDCard())                               logAlert("unable to initialize SD Card", true);
  if(!readHistory())                               logAlert("unable to initialize EEPROM", true);
  if(!sensors.init())                              logAlert("unable to initialize Sensors", true);
  #ifdef HITL_ENABLED_FLAG
    if(!HITL.init())                               logAlert("unable to initialize Simulations", true);
  #endif
  if(!filter.init())                               logAlert("unable to initialize Filters", true);
  if(!computer.init())                             logAlert("unable to initialize Flight Controller", true);
  if(!gpsModule.init(data.GPS_SHOULD_USE))         logAlert("unable to initialize GPS", true);
  #ifndef RB_DISABLED_FLAG
    if(!RBModule.init(data.RB_SHOULD_USE))         logAlert("unable to initialize RockBlock", true);
  #endif
  if(!PCB.startUpHeaters(data.HEATER_SHOULD_USE))  logAlert("unable to initialize Heaters", true);
  if(!ValMU.init(data.PAYLOAD_SHOULD_USE))         logAlert("unable to initialize Payload", true);
  PCB.initResolutions();
  data.SETUP_STATE = false;
  data.TIME = millis();
}

/*
 * Function: test
 * -------------------
 * This function tests the hardware.
 */
void Avionics::test() {
  data.MANUAL_MODE = false;
  data.SHOULD_CUTDOWN = true;
  PCB.queueValve(30000, true);
  PCB.queueBallast(120000, true);
}

/********************************  FUNCTIONS  *********************************/
/*
 * Function: updateState
 * -------------------
 * This function handles basic flight data collection.
 */
void Avionics::updateState() {
  #ifndef HITL_ENABLED_FLAG
    if(!readData())     logAlert("unable to read Data", true);
  #endif
  #ifdef HITL_ENABLED_FLAG
    if(!simulateData()) logAlert("unable to simulate Data", true);
  #endif
  if(!processData())    logAlert("unable to process Data", true);
}

/*
 * Function: evaluateState
 * -------------------
 * This function intelligently calculates the current state.
 */
void Avionics::evaluateState() {
  if(!calcVitals())     logAlert("unable to calculate vitals", true);
  if(!calcDebug())      logAlert("unable to calculate debug", true);
  if(!calcIncentives()) logAlert("unable to calculate incentives", true);
}

/*
 * Function: actuateState
 * -------------------
 * This function intelligently reacts to the current data frame.
 */
void Avionics::actuateState() {
  if(!runHeaters()) logAlert("unable to run heaters", true);
  if(!runValve())   logAlert("unable to run valve", true);
  if(!runBallast()) logAlert("unable to run ballast", true);
  if(!runCutdown()) logAlert("unable to run cutdown", true);
  if(!runLED())     logAlert("unable to run LED", true);
}

/*
 * Function: logState
 * -------------------
 * This function logs the current data frame.
 */
void Avionics::logState() {
  if (millis() - data.DATAFILE_LAST > FILE_RESET_TIME) {
    dataFile.close();
    logFile.close();
    setupLog();
    printHeader();
    data.DATAFILE_LAST = millis();
  }
  if(!logData())    logAlert("unable to log Data", true);
  if(!debugState()) logAlert("unable to debug state", true);
}

/*
 * Function: sendComms
 * -------------------
 * This function sends the current data frame down.
 */
void Avionics::sendComms() {
  if(data.DEBUG_STATE && ((millis() - data.COMMS_LAST) < COMMS_DEBUG_INTERVAL)) return;
  if(!data.DEBUG_STATE && ((millis() - data.COMMS_LAST) < data.COMMS_INTERVAL)) return;
  #ifndef RB_DISABLED_FLAG
    if (!data.RB_SHOULD_USE && ((millis() - data.COMMS_LAST) > COMMS_RESTART_INTERVAL)) {
      data.RB_SHOULD_USE = true;
      RBModule.restart();
    }
  #endif
  if(compressData() < 0) logAlert("unable to compress Data", true);
  if(!sendSATCOMS()) logAlert("unable to communicate over RB", true);
  data.COMMS_LAST = millis();
}

/*
 * Function: sleep
 * -------------------
 * This function sleeps at the end of the loop.
 */
void Avionics::sleep() {
  uint32_t loopTime = millis() - data.TIME;
  if (loopTime < LOOP_INTERVAL) gpsModule.smartDelay(LOOP_INTERVAL - loopTime);
}

/*
 * Function: finishedSetup
 * -------------------
 * This function returns true if the avionics has completed setup.
 */
bool Avionics::finishedSetup() {
  return !data.SETUP_STATE;
}

/*********************************  HELPERS  **********************************/
/*
 * Function: setupSDCard
 * -------------------
 * This function sets up the SD card for logging.
 */
bool Avionics::setupSDCard() {
  bool success = false;
  printHeader();
  if(SD.begin(SD_CS)) success = true;
  setupLog();
  logHeader();
  return success;
}

/*
 * Function: readHistory
 * -------------------
 * This function updates the data frame with values from EEPROM
 * if avionics is restarted mid flight.
 */
bool Avionics::readHistory() {
  #ifdef RESET_EEPROM_FLAG
    for(size_t i = 0; i < 1023; i++)   EEPROM.write(i, 0x0);
    EEPROM.write(EEPROM_ROCKBLOCK, true);
    EEPROM.write(EEPROM_GPS, true);
    EEPROM.write(EEPROM_HEATER, true);
    EEPROM.write(EEPROM_PAYLOAD, true);
    PCB.EEPROMWritelong(EEPROM_VALVE_ALT_LAST, data.VALVE_ALT_LAST);
    PCB.EEPROMWritelong(EEPROM_BALLAST_ALT_LAST, data.BALLAST_ALT_LAST);
  #endif
  #ifndef RESET_EEPROM_FLAG
    if(!EEPROM.read(EEPROM_ROCKBLOCK)) data.RB_SHOULD_USE = false;
    if(!EEPROM.read(EEPROM_GPS)) data.GPS_SHOULD_USE = false;
    if(!EEPROM.read(EEPROM_HEATER)) data.HEATER_SHOULD_USE = false;
    if(!EEPROM.read(EEPROM_PAYLOAD)) data.PAYLOAD_SHOULD_USE = false;
    double valveAltLast = PCB.EEPROMReadlong(EEPROM_VALVE_ALT_LAST);
    if (valveAltLast != 0) data.VALVE_ALT_LAST = valveAltLast;
    double ballastAltLast = PCB.EEPROMReadlong(EEPROM_BALLAST_ALT_LAST);
    if (ballastAltLast != 0) data.BALLAST_ALT_LAST = ballastAltLast;
  #endif
  return true;
}

uint32_t max = 0;

/*
 * Function: readData
 * -------------------
 * This function updates the current data frame.
 */
bool Avionics::readData() {
  data.LOOP_TIME        = millis() - data.TIME;
  data.TIME             = millis();
  data.VOLTAGE          = sensors.getVoltage();
  data.CURRENT          = sensors.getCurrent();
  data.JOULES           = sensors.getJoules();
  data.CURRENT_GPS      = sensors.getCurrentSubsystem(GPS_CURRENT);
  data.CURRENT_RB       = sensors.getCurrentSubsystem(RB_CURRENT);
  data.CURRENT_MOTORS   = sensors.getCurrentSubsystem(MOTORS_CURRENT);
  data.CURRENT_PAYLOAD  = sensors.getCurrentSubsystem(PAYLOAD_CURRENT);
  data.TEMP_EXT         = sensors.getDerivedTemp(EXT_TEMP_SENSOR);
  data.RAW_TEMP_1       = sensors.getRawTemp(1);
  data.RAW_TEMP_2       = sensors.getRawTemp(2);
  data.RAW_TEMP_3       = sensors.getRawTemp(3);
  data.RAW_TEMP_4       = sensors.getRawTemp(4);
  data.RAW_PRESSURE_1   = sensors.getRawPressure(1);
  data.RAW_PRESSURE_2   = sensors.getRawPressure(2);
  data.RAW_PRESSURE_3   = sensors.getRawPressure(3);
  data.RAW_PRESSURE_4   = sensors.getRawPressure(4);
  if (data.GPS_SHOULD_USE && ((millis() - data.GPS_LAST) >= data.GPS_INTERVAL) && (!data.VALVE_STATE)) readGPS();
  if (data.PAYLOAD_SHOULD_USE) readPayload();
  return true;
}

/*
 * Function: readGPS
 * -------------------
 * This function reads data from the GPS module.
 */
bool Avionics::readGPS() {
  gpsModule.smartDelay(GPS_LOCK_TIME);
  data.LAT_GPS          = gpsModule.getLatitude();
  data.LONG_GPS         = gpsModule.getLongitude();
  data.ALTITUDE_GPS     = gpsModule.getAltitude();
  data.HEADING_GPS      = gpsModule.getCourse();
  data.SPEED_GPS        = gpsModule.getSpeed();
  data.NUM_SATS_GPS     = gpsModule.getSats();
  data.GPS_LAST         = millis();
  return true;
}

/*
 * Function: readPayload
 * -------------------
 * This function reads data from the Payload.
 */
bool Avionics::readPayload() {
  ValMU.querrySensors();
  data.EULER_X          = ValMU.getCurrentEuler(0);
  data.EULER_Y          = ValMU.getCurrentEuler(1);
  data.EULER_Z          = ValMU.getCurrentEuler(2);
  return true;
}

bool Avionics::simulateData() {
  DataFrame simulation = HITL.readData();
  data.LOOP_TIME                      = millis() - data.TIME;
  data.TIME                           = millis();
  data.RAW_PRESSURE_1                 = simulation.RAW_PRESSURE_1;
  data.RAW_PRESSURE_2                 = simulation.RAW_PRESSURE_2;
  data.RAW_PRESSURE_3                 = simulation.RAW_PRESSURE_3;
  data.RAW_PRESSURE_4                 = simulation.RAW_PRESSURE_4;
  data.BMP_1_ENABLE                   = simulation.BMP_1_ENABLE;
  data.BMP_2_ENABLE                   = simulation.BMP_2_ENABLE;
  data.BMP_3_ENABLE                   = simulation.BMP_3_ENABLE;
  data.BMP_4_ENABLE                   = simulation.BMP_4_ENABLE;
  // data.ALTITUDE                       = simulation.ALTITUDE;
  // data.ASCENT_RATE                    = simulation.ASCENT_RATE;
  data.PRESS_BASELINE                 = simulation.PRESS_BASELINE;
  // data.INCENTIVE_THRESHOLD            = simulation.INCENTIVE_THRESHOLD;
  // data.RE_ARM_CONSTANT                = simulation.RE_ARM_CONSTANT;
  data.BALLAST_ARM_ALT                = simulation.BALLAST_ARM_ALT;
  data.VALVE_SETPOINT                 = simulation.VALVE_SETPOINT;
  data.VALVE_DURATION                 = simulation.VALVE_DURATION;
  // data.VALVE_ALT_LAST                 = simulation.VALVE_ALT_LAST;
  data.VALVE_VELOCITY_CONSTANT        = simulation.VALVE_VELOCITY_CONSTANT;
  // data.VALVE_ALTITUDE_DIFF_CONSTANT   = simulation.VALVE_ALTITUDE_DIFF_CONSTANT;
  // data.VALVE_LAST_ACTION_CONSTANT     = simulation.VALVE_LAST_ACTION_CONSTANT;
  data.BALLAST_SETPOINT               = simulation.BALLAST_SETPOINT;
  data.BALLAST_DURATION               = simulation.BALLAST_DURATION;
  // data.BALLAST_ALT_LAST               = simulation.BALLAST_ALT_LAST;
  data.BALLAST_VELOCITY_CONSTANT      = simulation.BALLAST_VELOCITY_CONSTANT;
  // data.BALLAST_ALTITUDE_DIFF_CONSTANT = simulation.BALLAST_ALTITUDE_DIFF_CONSTANT;
  // data.BALLAST_LAST_ACTION_CONSTANT   = simulation.BALLAST_LAST_ACTION_CONSTANT;
  // data.MANUAL_MODE                    = simulation.MANUAL_MODE;
  return true;
}

/*
 * Function: processData
 * -------------------
 * This function updates the current data frame with derived values.
 */
bool Avionics::processData() {
  bool success = true;
  filter.enableSensors(data.BMP_1_ENABLE, data.BMP_2_ENABLE, data.BMP_3_ENABLE, data.BMP_4_ENABLE);
  filter.storeData(data.TIME, data.RAW_PRESSURE_1, data.RAW_PRESSURE_2, data.RAW_PRESSURE_3, data.RAW_PRESSURE_4,data.PRESS_BASELINE);
  data.TEMP_IN             = filter.getTemp(data.RAW_TEMP_1, data.RAW_TEMP_2, data.RAW_TEMP_3, data.RAW_TEMP_4);
  data.PRESS               = filter.getPressure();
  data.BMP_1_REJECTIONS    = filter.getNumRejections(1);
  data.BMP_2_REJECTIONS    = filter.getNumRejections(2);
  data.BMP_3_REJECTIONS    = filter.getNumRejections(3);
  data.BMP_4_REJECTIONS    = filter.getNumRejections(4);

  data.CURRENT_AVG         = filter.getAvgCurrentSystem(data.CURRENT);
  data.CURRENT_MAX         = filter.getMaxCurrentSystem();
  data.CURRENT_GPS_AVG     = filter.getAvgCurrentGPS(data.CURRENT_GPS);
  data.CURRENT_GPS_MAX     = filter.getMaxCurrentGPS();
  data.CURRENT_RB_AVG      = filter.getAvgCurrentRB(data.CURRENT_RB);
  data.CURRENT_RB_MAX      = filter.getMaxCurrentRB();
  data.CURRENT_MOTORS_AVG  = filter.getAvgCurrentMotors(data.CURRENT_MOTORS, (data.VALVE_STATE || data.BALLAST_STATE));
  data.CURRENT_MOTORS_MAX  = filter.getMaxCurrentMotors();
  data.CURRENT_PAYLOAD_AVG = filter.getAvgCurrentPayload(data.CURRENT_PAYLOAD);
  data.CURRENT_PAYLOAD_MAX = filter.getMaxCurrentPayload();

  data.EULER_X_AVG         = ValMU.getAverageEuler(0, 0);
  data.EULER_Y_AVG         = ValMU.getAverageEuler(1, 0);
  data.EULER_Z_AVG         = ValMU.getAverageEuler(2, 0);

  data.ALTITUDE            = filter.getAltitude();
  data.ASCENT_RATE         = filter.getAscentRate();
  data.INCENTIVE_NOISE     = filter.getIncentiveNoise(data.BMP_1_ENABLE, data.BMP_2_ENABLE, data.BMP_3_ENABLE, data.BMP_4_ENABLE);
  if (data.ASCENT_RATE    >= 10) success = false;
  return success;
}

/*
 * Function: calcVitals
 * -------------------
 * This function calculates if the current state is within bounds.
 */
bool Avionics::calcVitals() {
  if(!data.SHOULD_REPORT) data.SHOULD_REPORT = (data.ASCENT_RATE >= 10);
  if(!data.MANUAL_MODE)   data.MANUAL_MODE   = (data.ASCENT_RATE >= 10);
  data.GPS_GOOD_STATE   = (data.LAT_GPS != 1000.0 && data.LAT_GPS != 0.0 && data.LONG_GPS != 1000.0 && data.LONG_GPS != 0.0);
  return true;
}

/*
 * Function: calcDebug
 * -------------------
 * This function calculates if the avionics is in debug mode.
 */
bool Avionics::calcDebug() {
  if(data.DEBUG_STATE && data.ALTITUDE >= DEBUG_ALT) data.DEBUG_STATE = false;
  return true;
}

/*
 * Function: calcIncentives
 * -------------------
 * This function gets the updated incentives from the flight computer.
 */
bool Avionics::calcIncentives() {
  bool success = true;
  computer.updateValveConstants(data.VALVE_SETPOINT, data.VALVE_VELOCITY_CONSTANT, data.VALVE_ALTITUDE_DIFF_CONSTANT, data.VALVE_LAST_ACTION_CONSTANT);
  computer.updateBallastConstants(data.BALLAST_SETPOINT, data.BALLAST_VELOCITY_CONSTANT, data.BALLAST_ALTITUDE_DIFF_CONSTANT, data.BALLAST_LAST_ACTION_CONSTANT);
  data.RE_ARM_CONSTANT   = computer.updateControllerConstants(data.BALLAST_ARM_ALT, data.INCENTIVE_THRESHOLD);
  data.VALVE_ALT_LAST    = computer.getAltitudeSinceLastVentCorrected(data.ALTITUDE, data.VALVE_ALT_LAST);
  data.BALLAST_ALT_LAST  = computer.getAltitudeSinceLastDropCorrected(data.ALTITUDE, data.BALLAST_ALT_LAST);
  data.VALVE_INCENTIVE   = computer.getValveIncentive(data.ASCENT_RATE, data.ALTITUDE, data.VALVE_ALT_LAST);
  data.BALLAST_INCENTIVE = computer.getBallastIncentive(data.ASCENT_RATE, data.ALTITUDE, data.BALLAST_ALT_LAST);
  if (!data.MANUAL_MODE && data.VALVE_INCENTIVE >= 1 && data.BALLAST_INCENTIVE >= 1) success =  false;
  return success;
}

/*
 * Function: runHeaters
 * -------------------
 * This function thermally regulates the avionics. Disables heaters
 * if either the ballast or valve is running.
 */
bool Avionics::runHeaters() {
  if (!data.HEATER_SHOULD_USE || data.VALVE_STATE || data.BALLAST_STATE) {
    PCB.turnOffHeaters();
  } else {
    PCB.heater(data.TEMP_SETPOINT, data.TEMP_IN, data.HEATER_STRONG_ENABLE, data.HEATER_WEEK_ENABLE);
  }
  return true;
}

/*
 * Function: runValve
 * -------------------
 * This function actuates the valve based on the calculated incentive.
 */
bool Avionics::runValve() {
  PCB.updateMechanicalConstants(data.VALVE_MOTOR_SPEED, data.BALLAST_MOTOR_SPEED, data.VALVE_OPENING_TIMEOUT, data.VALVE_CLOSING_TIMEOUT);
  if((data.VALVE_INCENTIVE >= (1 + data.INCENTIVE_NOISE) && PCB.getValveQueue() <= QUEUE_APPEND_THRESHOLD) || data.FORCE_VALVE) {
    data.NUM_VALVE_ATTEMPTS++;
    bool shouldValve = (!data.MANUAL_MODE || data.FORCE_VALVE);
    if(shouldValve) data.NUM_VALVES++;
    if(!data.FORCE_VALVE) data.VALVE_ALT_LAST = data.ALTITUDE;
    uint32_t valveTime = data.VALVE_DURATION;
    if(data.FORCE_VALVE) valveTime = data.VALVE_FORCE_DURATION;
    PCB.EEPROMWritelong(EEPROM_VALVE_ALT_LAST, data.VALVE_ALT_LAST);
    PCB.queueValve(valveTime, shouldValve);
    data.VALVE_TIME_TOTAL += valveTime;
    data.FORCE_VALVE = false;
  }
  data.VALVE_QUEUE = PCB.getValveQueue();
  data.VALVE_STATE = PCB.checkValve(data.CURRENT_MOTORS);
  return true;
}

/*
 * Function: runBallast
 * -------------------
 * This function actuates the valve based on the calculated incentive.
 */
bool Avionics::runBallast() {
  PCB.updateMechanicalConstants(data.VALVE_MOTOR_SPEED, data.BALLAST_MOTOR_SPEED, data.VALVE_OPENING_TIMEOUT, data.VALVE_CLOSING_TIMEOUT);
  if((data.BALLAST_INCENTIVE >= (1 + data.INCENTIVE_NOISE) && PCB.getBallastQueue() <= QUEUE_APPEND_THRESHOLD) || data.FORCE_BALLAST) {
    data.NUM_BALLAST_ATTEMPTS++;
    bool shouldBallast = (!data.MANUAL_MODE || data.FORCE_BALLAST);
    if(shouldBallast) data.NUM_BALLASTS++;
    if(!data.FORCE_BALLAST) data.BALLAST_ALT_LAST = data.ALTITUDE;
    uint32_t ballastTime = data.BALLAST_DURATION;
    if(data.FORCE_BALLAST) ballastTime = data.BALLAST_FORCE_DURATION;
    PCB.EEPROMWritelong(EEPROM_BALLAST_ALT_LAST, data.BALLAST_ALT_LAST);
    PCB.queueBallast(ballastTime, shouldBallast);
    data.BALLAST_TIME_TOTAL += ballastTime;
    data.FORCE_BALLAST = false;
  }
  data.BALLAST_QUEUE = PCB.getBallastQueue();
  data.NUM_BALLAST_OVER_CURRENTS = PCB.getNumBallastOverCurrents();
  data.BALLAST_STATE = PCB.checkBallast(data.CURRENT_MOTORS, data.BALLAST_REVERSE_TIMEOUT, data.BALLAST_STALL_CURRENT);
  return true;
}

/*
 * Function: runCutdown
 * -------------------
 * This function cuts down the payload if necessary.
 */
bool Avionics::runCutdown() {
  if(data.SHOULD_CUTDOWN) {
    PCB.cutDown();
    data.SHOULD_CUTDOWN = false;
    data.CUTDOWN_STATE = true;
    logAlert("completed cutdown", false);
  }
  return true;
}

/*
 * Function: runLED
 * -------------------
 * This function blinks the 1HZ LED required by the FAA.
 */
bool Avionics::runLED() {
  if (data.SHOULD_LED && (uint32_t(millis() / 1000.0) % 2 == 1)) PCB.runLED(true);
  else PCB.runLED(false);
  return true;
}

/*
 * Function: sendSATCOMS
 * -------------------
 * This function sends the current data frame over the ROCKBLOCK IO.
 */
bool Avionics::sendSATCOMS() {
  logAlert("sending Rockblock message", false);
  data.RB_SENT_COMMS++;
  #ifndef RB_DISABLED_FLAG
    int16_t ret = RBModule.writeRead(COMMS_BUFFER, data.COMMS_LENGTH);
    if(ret < 0) return false;
    if(ret > 0) parseCommand(ret);
  #endif
  return true;
}

/*
 * Function: parseCommand
 * -------------------
 * This function parses the command received from the RockBLOCK.
 */
void Avionics::parseCommand(int16_t len) {
  COMMS_BUFFER[len] = 0;
  const char* commandStrFormat = "%d,%s %d,%s %d,%s %d,%s %d,%s %d,%s %d,%s %d,%s";
  uint8_t commandIndexes[8] = {0};
  char commandStrings[8][100] = {{0},{0},{0},{0},{0},{0},{0},{0}};
  uint8_t numScanned = sscanf(COMMS_BUFFER, commandStrFormat,
    &commandIndexes[0], commandStrings[0],
    &commandIndexes[1], commandStrings[1],
    &commandIndexes[2], commandStrings[2],
    &commandIndexes[3], commandStrings[3],
    &commandIndexes[4], commandStrings[4],
    &commandIndexes[5], commandStrings[5],
    &commandIndexes[6], commandStrings[6],
    &commandIndexes[7], commandStrings[7]);
  if (numScanned % 2 != 0) return;
  data.SHOULD_REPORT = true;

  for (uint8_t i = 0; i < numScanned / 2; i++) {
    uint8_t index = commandIndexes[i];
    if (index == CUTDOWN_INDEX && (strlen(CUTDOWN_COMMAND) == strlen(commandStrings[i])) && strncmp(commandStrings[i], CUTDOWN_COMMAND, strlen(commandStrings[i])) == 0) {
      data.SHOULD_CUTDOWN = true;
    }
    if (index < 0 || index > 80) return;
    char* charAfterNumbers;
    float commandValue = (float) strtod(commandStrings[i], &charAfterNumbers);
    if (*charAfterNumbers) return;
    updateConstant(index, commandValue);
  }
}

/*
 * Function: updateConstant
 * -------------------
 * This function updates the state appropriate state variable
 * based on the command index.
 */
void Avionics::updateConstant(uint8_t index, float value) {
  if      (index ==  0) data.VALVE_ALT_LAST = value;
  else if (index ==  1) data.BALLAST_ALT_LAST = value;
  else if (index ==  2) data.VALVE_SETPOINT = value;
  else if (index ==  3) data.BALLAST_SETPOINT = value;
  else if (index ==  4) data.BALLAST_ARM_ALT = value;
  else if (index ==  5) data.INCENTIVE_THRESHOLD = value;
  else if (index ==  6) data.VALVE_VELOCITY_CONSTANT = value;
  else if (index ==  7) data.VALVE_ALTITUDE_DIFF_CONSTANT = 1.0 / value;
  else if (index ==  8) data.VALVE_LAST_ACTION_CONSTANT = 1.0 / value;
  else if (index ==  9) data.BALLAST_VELOCITY_CONSTANT = value;
  else if (index == 10) data.BALLAST_ALTITUDE_DIFF_CONSTANT = 1.0 / value;
  else if (index == 11) data.BALLAST_LAST_ACTION_CONSTANT = 1.0 / value;
  else if (index == 12) data.VALVE_DURATION = value;
  else if (index == 13) data.BALLAST_DURATION = value;
  else if (index == 14) data.PRESS_BASELINE = value;
  else if (index == 15) data.BALLAST_REVERSE_TIMEOUT = value * 60000;
  else if (index == 16) data.BALLAST_STALL_CURRENT = value;
  else if (index == 17) data.VALVE_MOTOR_SPEED = value;
  else if (index == 18) data.BALLAST_MOTOR_SPEED = value;
  else if (index == 19) data.VALVE_OPENING_TIMEOUT = value;
  else if (index == 20) data.VALVE_CLOSING_TIMEOUT = value;
  else if (index == 21) data.TEMP_SETPOINT = value;
  else if (index == 22) data.SHOULD_LED = value;
  else if (index == 23) data.COMMS_INTERVAL = value * 60000;
  else if (index == 24) data.GPS_INTERVAL = value * 60000;
  else if (index == 25) parseManualCommand(value);
  else if (index == 26) parseReportCommand(value);
  else if (index == 27) parseSensorsCommand(value);
  else if (index == 28) parseValveCommand(value);
  else if (index == 29) parseBallastCommand(value);
  else if (index == 30) parseRockBLOCKPowerCommand(value);
  else if (index == 31) parseGPSPowerCommand(value);
  else if (index == 32) parseHeaterPowerCommand(value);
  else if (index == 33) parseHeaterModeCommand(value);
  else if (index == 34) parsePayloadPowerCommand(value);
}

/*
 * Function: parseManualCommand
 * -------------------
 * This function parses the manual mode command.
 */
void Avionics::parseManualCommand(bool command) {
  PCB.clearValveQueue();
  PCB.clearBallastQueue();
  data.MANUAL_MODE = command;
}

/*
 * Function: parseReportCommand
 * -------------------
 * This function parses the REPORT_MODE mode command.
 */
void Avionics::parseReportCommand(uint8_t command) {
  data.REPORT_MODE = command;
}

/*
 * Function: parseSensorsCommand
 * -------------------
 * This function parses the active sensors.
 */
void Avionics::parseSensorsCommand(uint8_t command) {
  data.BMP_1_ENABLE = command & 0b0001;
  data.BMP_2_ENABLE = command & 0b0010;
  data.BMP_3_ENABLE = command & 0b0100;
  data.BMP_4_ENABLE = command & 0b1000;
}

/*
 * Function: parseValveCommand
 * -------------------
 * This function parses a forced valve command.
 */
void Avionics::parseValveCommand(uint32_t command) {
  if(command == 0) PCB.clearValveQueue();
  else {
    data.FORCE_VALVE = true;
    data.VALVE_FORCE_DURATION = command;
  }
}

/*
 * Function: parseBallastCommand
 * -------------------
 * This function parses a forced ballast command.
 */
void Avionics::parseBallastCommand(uint32_t command) {
  if(command == 0) PCB.clearBallastQueue();
  else {
    data.FORCE_BALLAST = true;
    data.BALLAST_FORCE_DURATION = command;
  }
}

/*
 * Function: parseRockBLOCKPowerCommand
 * -------------------
 * This function parses the power RockBLOCK command.
 */
void Avionics::parseRockBLOCKPowerCommand(bool command) {
  if (command && !data.RB_SHOULD_USE) {
    data.RB_SHOULD_USE = true;
    RBModule.restart();
  }
  else if (!command) {
    data.RB_SHOULD_USE = false;
    RBModule.shutdown();
  }
}

/*
 * Function: parseGPSPowerCommand
 * -------------------
 * This function parses the GPS power command.
 */
void Avionics::parseGPSPowerCommand(uint8_t command) {
  if (command == 0) {
    data.GPS_SHOULD_USE = false;
    gpsModule.shutdown();
  }
  else if (command == 1) {
    data.GPS_SHOULD_USE = true;
    gpsModule.restart();
  }
  else if (command == 2) {
    gpsModule.hotstart();
    readGPS();
  }
}

/*
 * Function: parseHeaterPowerCommand
 * -------------------
 * This function parses the heater power command.
 */
void Avionics::parseHeaterPowerCommand(bool command) {
  data.HEATER_SHOULD_USE = command;
  PCB.setHeaterMode(command);
  if (!command) PCB.turnOffHeaters();
}

/*
 * Function: parseHeaterModeCommand
 * -------------------
 * This function parses the heater mode.
 */
void Avionics::parseHeaterModeCommand(uint8_t command) {
  data.HEATER_STRONG_ENABLE = command & 0b0001;
  data.HEATER_WEEK_ENABLE   = command & 0b0010;
}

/*
 * Function: parsePayloadPowerCommand
 * -------------------
 * This function parses the Payload power command.
 */
void Avionics::parsePayloadPowerCommand(bool command) {
  if (command && !data.PAYLOAD_SHOULD_USE) {
    data.PAYLOAD_SHOULD_USE = true;
    ValMU.restart();
  }
  else if (!command) {
    data.PAYLOAD_SHOULD_USE = false;
    ValMU.shutdown();
  }
}

/*
 * Function: debugState
 * -------------------
 * This function provides debuging information.
 */
bool Avionics::debugState() {
  if(data.DEBUG_STATE) printState();
  return true;
}

/*
 * Function: setupLog
 * -------------------
 * This function initializes the SD card file.
 */
void Avionics::setupLog() {
  Serial.println("Card Initialitzed");
  char filename[] = "LOGGER00.txt";
  for (uint8_t i = 0; i < 100; i++) {
    filename[6] = i / 10 + '0';
    filename[7] = i % 10 + '0';
    if (!SD.exists(filename)) {
      dataFile = SD.open(filename, O_WRITE | O_CREAT);
      break;
    }
  }
  logFile = SD.open("EVENTS.txt", O_WRITE | O_CREAT);
  if (!dataFile || !logFile) {
    Serial.println ("ERROR: COULD NOT CREATE FILE");
  }
  else {
    Serial.print("Logging to: ");
    Serial.println(filename);
  }
}

/*
 * Function: printHeader
 * -------------------
 * This function prints the CSV header.
 */
void Avionics::printHeader() {
  Serial.print("Stanford Student Space Initiative Balloons Launch ");
  Serial.print(MISSION_NUMBER);
  Serial.print('\n');
}

/*
 * Function: logHeader
 * -------------------
 * This function logs the CSV header.
 */
void Avionics::logHeader() {
  dataFile.print("Stanford Student Space Initiative Balloons Launch ");
  dataFile.print(MISSION_NUMBER);
  dataFile.print('\n');
  dataFile.print(CSV_DATA_HEADER);
  dataFile.print('\n');
  dataFile.flush();
}

/*
   * Function: logAlert
 * -------------------
 * This function logs important information whenever a specific event occurs.
 */
void Avionics::logAlert(const char* debug, bool fatal) {
  if(logFile) {
    logFile.print(millis());
    logFile.print(',');
    if(fatal) logFile.print("FATAL ERROR!!!!!!!!!!: ");
    else logFile.print("Alert: ");
    logFile.print(debug);
    logFile.print("...\n");
  }
  if(data.DEBUG_STATE) {
    Serial.print(millis());
    Serial.print(',');
    if(fatal) Serial.print("FATAL ERROR!!!!!!!!!!: ");
    else Serial.print("Alert: ");
    Serial.print(debug);
    Serial.print("...\n");
  }
}

/*
 * Function: compressVariable
 * -------------------
 * This function compresses a single variable into a scaled digital bitmask.
 */
int16_t Avionics::compressVariable(float var, float minimum, float maximum, int16_t resolution, int16_t length) {
  if (resolution <= 0) return -1;
  if (var < minimum) var = minimum;
  if (var > maximum) var = maximum;
  int32_t adc = round( (pow(2, resolution) - 1) * (var - minimum) / (maximum - minimum));
  int16_t byteIndex = length / 8;
  int16_t bitIndex = 7 - (length % 8);
  for (int16_t i = resolution - 1; i >= 0; i--) {
    bool bit = adc & (1 << i);
    if (bit) COMMS_BUFFER[byteIndex] |= (1 << bitIndex);
    bitIndex -= 1;
    if (bitIndex < 0) {
      bitIndex = 7;
      byteIndex++;
    }
  }
  return resolution;
}

/*
 * Function: compressData
 * -------------------
 * This function compresses the data frame into a bit stream.
 * The total bitstream cannot exceed 400 bytes.
 */
int16_t Avionics::compressData() {
  int16_t lengthBits  = 0;
  int16_t lengthBytes = 0;
  for(uint16_t i = 0; i < BUFFER_SIZE; i++) COMMS_BUFFER[i] = 0;
  lengthBits += compressVariable(data.TIME / 1000,                           0,    3000000, 20, lengthBits);
  lengthBits += compressVariable(data.LAT_GPS,                              -90,   90,      21, lengthBits);
  lengthBits += compressVariable(data.LONG_GPS,                             -180,  180,     22, lengthBits);
  lengthBits += compressVariable(data.ALTITUDE,                             -2000, 40000,   16, lengthBits);
  lengthBits += compressVariable(data.ALTITUDE_GPS,                         -2000, 40000,   14, lengthBits);
  lengthBits += compressVariable(data.ASCENT_RATE,                          -10,   10,      11, lengthBits);
  lengthBits += compressVariable(data.VALVE_INCENTIVE,                      -50,   10,      12, lengthBits);
  lengthBits += compressVariable(data.BALLAST_INCENTIVE,                    -50,   10,      12, lengthBits);
  lengthBits += compressVariable(data.VALVE_STATE,                           0,    1,       1,  lengthBits);
  lengthBits += compressVariable(data.BALLAST_STATE,                         0,    1,       1,  lengthBits);
  lengthBits += compressVariable(data.VALVE_QUEUE,                           0,    1000000, 10, lengthBits);
  lengthBits += compressVariable(data.BALLAST_QUEUE,                         0,    1000000, 10, lengthBits);
  lengthBits += compressVariable(data.VALVE_TIME_TOTAL / 1000,               0,    16384,   13, lengthBits);
  lengthBits += compressVariable(data.BALLAST_TIME_TOTAL / 1000,             0,    16384,   13, lengthBits);
  lengthBits += compressVariable(data.NUM_VALVES,                            0,    50,      6,  lengthBits);
  lengthBits += compressVariable(data.NUM_BALLASTS,                          0,    50,      6,  lengthBits);
  lengthBits += compressVariable(data.NUM_VALVE_ATTEMPTS,                    0,    50,      6,  lengthBits);
  lengthBits += compressVariable(data.NUM_BALLAST_ATTEMPTS,                  0,    50,      6,  lengthBits);
  lengthBits += compressVariable(data.NUM_BALLAST_OVER_CURRENTS,             0,    50,      6,  lengthBits);
  lengthBits += compressVariable(data.CUTDOWN_STATE,                         0,    1,       1,  lengthBits);
  lengthBits += compressVariable(data.TEMP_IN,                              -70,   80,      9,  lengthBits);
  lengthBits += compressVariable(data.JOULES,                                0,    1500000, 18, lengthBits);
  lengthBits += compressVariable(data.VOLTAGE,                               0,    5,       9,  lengthBits);
  lengthBits += compressVariable(data.CURRENT_AVG,                           0,    4000,    12, lengthBits);
  lengthBits += compressVariable(data.CURRENT_MIN,                           0,    4000,    12, lengthBits);
  lengthBits += compressVariable(data.CURRENT_MAX,                           0,    4000,    12, lengthBits);
  lengthBits += compressVariable(data.CURRENT_GPS_AVG,                       0,    200,     8,  lengthBits);
  lengthBits += compressVariable(data.CURRENT_GPS_MAX,                       0,    200,     8,  lengthBits);
  lengthBits += compressVariable(data.CURRENT_RB_AVG,                        0,    1000,    8,  lengthBits);
  lengthBits += compressVariable(data.CURRENT_RB_MAX,                        0,    1000,    8,  lengthBits);
  lengthBits += compressVariable(data.CURRENT_MOTORS_AVG,                    0,    1000,    8,  lengthBits);
  lengthBits += compressVariable(data.CURRENT_MOTORS_MAX,                    0,    1000,    8,  lengthBits);
  lengthBits += compressVariable(data.CURRENT_PAYLOAD_AVG,                   0,    500,     8,  lengthBits);
  lengthBits += compressVariable(data.CURRENT_PAYLOAD_MAX,                   0,    500,     8,  lengthBits);
  lengthBits += compressVariable(data.TEMP_EXT,                             -100,  30,      6,  lengthBits);
  lengthBits += compressVariable(data.LOOP_TIME,                             0,    10000,   10, lengthBits);
  lengthBits += compressVariable(data.RB_SENT_COMMS,                         0,    8191,    13, lengthBits);
  lengthBits += compressVariable(data.EULER_X_AVG,                           0,    360,     8,  lengthBits);
  lengthBits += compressVariable(data.EULER_Y_AVG,                          -180,  180,     8,  lengthBits);
  lengthBits += compressVariable(data.EULER_Z_AVG,                          -90,   90,      7,  lengthBits);
  lengthBits += compressVariable(data.MANUAL_MODE,                           0,    1,       1,  lengthBits);
  lengthBits += compressVariable(data.REPORT_MODE,                           0,    2,       2,  lengthBits);
  lengthBits += compressVariable(data.SHOULD_REPORT,                         0,    1,       1,  lengthBits);
  if (data.SHOULD_REPORT || data.REPORT_MODE != 0) {
    lengthBits += compressVariable(data.RB_SHOULD_USE,                       0,    1,       1,  lengthBits);
    lengthBits += compressVariable(data.GPS_SHOULD_USE,                      0,    1,       1,  lengthBits);
    lengthBits += compressVariable(data.HEATER_SHOULD_USE,                   0,    1,       1,  lengthBits);
    lengthBits += compressVariable(data.PAYLOAD_SHOULD_USE,                  0,    1,       1,  lengthBits);
    lengthBits += compressVariable(data.HEATER_STRONG_ENABLE,                0,    1,       1,  lengthBits);
    lengthBits += compressVariable(data.HEATER_WEEK_ENABLE,                  0,    1,       1,  lengthBits);
    lengthBits += compressVariable(data.GPS_GOOD_STATE,                      0,    1,       1,  lengthBits);
    lengthBits += compressVariable(data.SPEED_GPS,                           0,    150,     7,  lengthBits);
    lengthBits += compressVariable(data.HEADING_GPS,                         0,    360,     8,  lengthBits);
    lengthBits += compressVariable(data.NUM_SATS_GPS,                        0,    25,      3,  lengthBits);
    lengthBits += compressVariable(data.INCENTIVE_NOISE,                     0,    4,       8,  lengthBits);
    lengthBits += compressVariable(data.RE_ARM_CONSTANT,                     0,    4,       8,  lengthBits);
    lengthBits += compressVariable(data.VALVE_ALT_LAST,                     -2000, 50000,   11, lengthBits);
    lengthBits += compressVariable(data.BALLAST_ALT_LAST,                   -2000, 50000,   11, lengthBits);
    lengthBits += compressVariable(data.SHOULD_LED,                          0,    1,       1,  lengthBits);
    lengthBits += compressVariable(data.DEBUG_STATE,                         0,    1,       1,  lengthBits);
    lengthBits += compressVariable(data.FORCE_VALVE,                         0,    1,       1,  lengthBits);
    lengthBits += compressVariable(data.FORCE_BALLAST,                       0,    1,       1,  lengthBits);
    lengthBits += compressVariable(data.BMP_1_ENABLE,                        0,    1,       1,  lengthBits);
    lengthBits += compressVariable(data.BMP_2_ENABLE,                        0,    1,       1,  lengthBits);
    lengthBits += compressVariable(data.BMP_3_ENABLE,                        0,    1,       1,  lengthBits);
    lengthBits += compressVariable(data.BMP_4_ENABLE,                        0,    1,       1,  lengthBits);
    lengthBits += compressVariable(log2(data.BMP_1_REJECTIONS + 1),          0,    6,       4,  lengthBits);
    lengthBits += compressVariable(log2(data.BMP_2_REJECTIONS + 1),          0,    6,       4,  lengthBits);
    lengthBits += compressVariable(log2(data.BMP_3_REJECTIONS + 1),          0,    6,       4,  lengthBits);
    lengthBits += compressVariable(log2(data.BMP_4_REJECTIONS + 1),          0,    6,       4,  lengthBits);
    for(size_t i = 0; i < 12; i++) {
      lengthBits += compressVariable(ValMU.getAverageEuler(0, i),            0,    360,     8,  lengthBits);
      lengthBits += compressVariable(ValMU.getAverageEuler(1, i),           -180,  180,     8,  lengthBits);
      lengthBits += compressVariable(ValMU.getAverageEuler(2, i),           -90,   90,      4,  lengthBits);
    }
  }
  if (data.SHOULD_REPORT || data.REPORT_MODE == 2) {
    lengthBits += compressVariable(data.TEMP_SETPOINT,                      -40,   40,      6,  lengthBits);
    lengthBits += compressVariable(data.COMMS_INTERVAL,                      0,    1000000, 10, lengthBits);
    lengthBits += compressVariable(data.GPS_INTERVAL,                        0,    1000000, 10, lengthBits);
    lengthBits += compressVariable(data.PRESS_BASELINE,                      0,    500000,  19, lengthBits);
    lengthBits += compressVariable(data.INCENTIVE_THRESHOLD,                 0,    4,       8,  lengthBits);
    lengthBits += compressVariable(data.BALLAST_ARM_ALT,                    -2000, 40000,   16, lengthBits);
    lengthBits += compressVariable(data.BALLAST_REVERSE_TIMEOUT,             0,    1000000, 4,  lengthBits);
    lengthBits += compressVariable(data.BALLAST_STALL_CURRENT,               0,    500,     4,  lengthBits);
    lengthBits += compressVariable(data.VALVE_MOTOR_SPEED,                   0,    255,     4,  lengthBits);
    lengthBits += compressVariable(data.BALLAST_MOTOR_SPEED,                 0,    255,     4,  lengthBits);
    lengthBits += compressVariable(data.VALVE_OPENING_TIMEOUT,               0,    10000,   5,  lengthBits);
    lengthBits += compressVariable(data.VALVE_CLOSING_TIMEOUT,               0,    10000,   5,  lengthBits);
    lengthBits += compressVariable(data.VALVE_SETPOINT,                     -2000, 50000,   11, lengthBits);
    lengthBits += compressVariable(data.VALVE_DURATION,                      0,    1000000, 6,  lengthBits);
    lengthBits += compressVariable(data.VALVE_FORCE_DURATION,                0,    1000000, 6,  lengthBits);
    lengthBits += compressVariable(data.VALVE_VELOCITY_CONSTANT,             0,    5,       8,  lengthBits);
    lengthBits += compressVariable(1.0/ data.VALVE_ALTITUDE_DIFF_CONSTANT,   0,    4000,    8,  lengthBits);
    lengthBits += compressVariable(1.0/ data.VALVE_LAST_ACTION_CONSTANT,     0,    4000,    8,  lengthBits);
    lengthBits += compressVariable(data.BALLAST_SETPOINT,                   -2000, 50000,   11, lengthBits);
    lengthBits += compressVariable(data.BALLAST_DURATION,                    0,    1000000, 6,  lengthBits);
    lengthBits += compressVariable(data.BALLAST_FORCE_DURATION,              0,    1000000, 6,  lengthBits);
    lengthBits += compressVariable(data.BALLAST_VELOCITY_CONSTANT,           0,    5,       8,  lengthBits);
    lengthBits += compressVariable(1.0/ data.BALLAST_ALTITUDE_DIFF_CONSTANT, 0,    4000,    8,  lengthBits);
    lengthBits += compressVariable(1.0/ data.BALLAST_LAST_ACTION_CONSTANT,   0,    4000,    8,  lengthBits);
  }
  lengthBits += 8 - (lengthBits % 8);
  lengthBytes = lengthBits / 8;
  data.SHOULD_REPORT = false;
  data.COMMS_LENGTH = lengthBytes;
  logFile.print(data.TIME);
  logFile.print(',');
  for (int16_t i = 0; i < lengthBytes; i++) {
    uint8_t byte = COMMS_BUFFER[i];
    (byte & 0x80 ? logFile.print('1') : logFile.print('0'));
    (byte & 0x40 ? logFile.print('1') : logFile.print('0'));
    (byte & 0x20 ? logFile.print('1') : logFile.print('0'));
    (byte & 0x10 ? logFile.print('1') : logFile.print('0'));
    (byte & 0x08 ? logFile.print('1') : logFile.print('0'));
    (byte & 0x04 ? logFile.print('1') : logFile.print('0'));
    (byte & 0x02 ? logFile.print('1') : logFile.print('0'));
    (byte & 0x01 ? logFile.print('1') : logFile.print('0'));
    if(data.DEBUG_STATE) {
      (byte & 0x80 ? Serial.print('1') : Serial.print('0'));
      (byte & 0x40 ? Serial.print('1') : Serial.print('0'));
      (byte & 0x20 ? Serial.print('1') : Serial.print('0'));
      (byte & 0x10 ? Serial.print('1') : Serial.print('0'));
      (byte & 0x08 ? Serial.print('1') : Serial.print('0'));
      (byte & 0x04 ? Serial.print('1') : Serial.print('0'));
      (byte & 0x02 ? Serial.print('1') : Serial.print('0'));
      (byte & 0x01 ? Serial.print('1') : Serial.print('0'));
    }
  }
  logFile.print('\n');
  if(data.DEBUG_STATE) Serial.print('\n');
  filter.clearCurrentValues();
  data.NUM_VALVES = 0;
  data.NUM_BALLASTS = 0;
  data.NUM_VALVE_ATTEMPTS = 0;
  data.NUM_BALLAST_ATTEMPTS = 0;
  data.NUM_BALLAST_OVER_CURRENTS = 0;
  return lengthBytes;
}

/*
 * Function: printState
 * -------------------
 * This function prints the current avionics state.
 */
void Avionics::printState() {
  Serial.print("TIME:");
  Serial.print(data.TIME);
  Serial.print(',');
  Serial.print(" LAT_GPS:");
  Serial.print(data.LAT_GPS, 4);
  Serial.print(',');
  Serial.print(" LONG_GPS:");
  Serial.print(data.LONG_GPS, 4);
  Serial.print(',');
  Serial.print(" ALTITUDE:");
  Serial.print(data.ALTITUDE);
  Serial.print(',');
  Serial.print(" ALTITUDE_GPS:");
  Serial.print(data.ALTITUDE_GPS);
  Serial.print(',');
  Serial.print(" ASCENT_RATE:");
  Serial.print(data.ASCENT_RATE);
  Serial.print(',');
  Serial.print(" VALVE_INCENTIVE:");
  Serial.print(data.VALVE_INCENTIVE);
  Serial.print(',');
  Serial.print(" BALLAST_INCENTIVE:");
  Serial.print(data.BALLAST_INCENTIVE);
  Serial.print(',');
  Serial.print(" VALVE_STATE:");
  Serial.print(data.VALVE_STATE);
  Serial.print(',');
  Serial.print(" BALLAST_STATE:");
  Serial.print(data.BALLAST_STATE);
  Serial.print(',');
  Serial.print(" VALVE_QUEUE:");
  Serial.print(data.VALVE_QUEUE);
  Serial.print(',');
  Serial.print(" BALLAST_QUEUE:");
  Serial.print(data.BALLAST_QUEUE);
  Serial.print(',');
  Serial.print(" VALVE_TIME_TOTAL:");
  Serial.print(data.VALVE_TIME_TOTAL);
  Serial.print(',');
  Serial.print(" BALLAST_TIME_TOTAL:");
  Serial.print(data.BALLAST_TIME_TOTAL);
  Serial.print(',');
  Serial.print(" NUM_VALVES:");
  Serial.print(data.NUM_VALVES);
  Serial.print(',');
  Serial.print(" NUM_BALLASTS:");
  Serial.print(data.NUM_BALLASTS);
  Serial.print(',');
  Serial.print(" NUM_VALVE_ATTEMPTS:");
  Serial.print(data.NUM_VALVE_ATTEMPTS);
  Serial.print(',');
  Serial.print(" NUM_BALLAST_ATTEMPTS:");
  Serial.print(data.NUM_BALLAST_ATTEMPTS);
  Serial.print(',');
  Serial.print(" NUM_BALLAST_OVER_CURRENTS:");
  Serial.print(data.NUM_BALLAST_OVER_CURRENTS);
  Serial.print(',');
  Serial.print(" CUTDOWN_STATE:");
  Serial.print(data.CUTDOWN_STATE);
  Serial.print(',');
  Serial.print(" TEMP_IN:");
  Serial.print(data.TEMP_IN);
  Serial.print(',');
  Serial.print(" JOULES:");
  Serial.print(data.JOULES);
  Serial.print(',');
  Serial.print(" VOLTAGE:");
  Serial.print(data.VOLTAGE);
  Serial.print(',');
  Serial.print(" CURRENT_AVG:");
  Serial.print(data.CURRENT_AVG);
  Serial.print(',');
  Serial.print(" CURRENT_MIN:");
  Serial.print(data.CURRENT_MIN);
  Serial.print(',');
  Serial.print(" CURRENT_MAX:");
  Serial.print(data.CURRENT_MAX);
  Serial.print(',');
  Serial.print(" CURRENT_GPS_AVG:");
  Serial.print(data.CURRENT_GPS_AVG);
  Serial.print(',');
  Serial.print(" CURRENT_GPS_MAX:");
  Serial.print(data.CURRENT_GPS_MAX);
  Serial.print(',');
  Serial.print(" CURRENT_RB_AVG:");
  Serial.print(data.CURRENT_RB_AVG);
  Serial.print(',');
  Serial.print(" CURRENT_RB_MAX:");
  Serial.print(data.CURRENT_RB_MAX);
  Serial.print(',');
  Serial.print(" CURRENT_MOTORS_AVG:");
  Serial.print(data.CURRENT_MOTORS_AVG);
  Serial.print(',');
  Serial.print(" CURRENT_MOTORS_MAX:");
  Serial.print(data.CURRENT_MOTORS_MAX);
  Serial.print(',');
  Serial.print(" CURRENT_PAYLOAD_AVG:");
  Serial.print(data.CURRENT_PAYLOAD_AVG);
  Serial.print(',');
  Serial.print(" CURRENT_PAYLOAD_MAX:");
  Serial.print(data.CURRENT_PAYLOAD_MAX);
  Serial.print(',');
  Serial.print(" TEMP_EXT:");
  Serial.print(data.TEMP_EXT);
  Serial.print(',');
  Serial.print(" LOOP_TIME:");
  Serial.print(data.LOOP_TIME);
  Serial.print(',');
  Serial.print(" RB_SENT_COMMS:");
  Serial.print(data.RB_SENT_COMMS);
  Serial.print(',');
  Serial.print(" EULER_X_AVG:");
  Serial.print(data.EULER_X_AVG);
  Serial.print(',');
  Serial.print(" EULER_Y_AVG:");
  Serial.print(data.EULER_Y_AVG);
  Serial.print(',');
  Serial.print(" EULER_Z_AVG:");
  Serial.print(data.EULER_Z_AVG);
  Serial.print(',');
  Serial.print(" MANUAL_MODE:");
  Serial.print(data.MANUAL_MODE);
  Serial.print(',');
  Serial.print(" REPORT_MODE:");
  Serial.print(data.REPORT_MODE);
  Serial.print(',');
  Serial.print(" SHOULD_REPORT:");
  Serial.print(data.SHOULD_REPORT);
  Serial.print(',');
  Serial.print(" RB_SHOULD_USE:");
  Serial.print(data.RB_SHOULD_USE);
  Serial.print(',');
  Serial.print(" GPS_SHOULD_USE:");
  Serial.print(data.GPS_SHOULD_USE);
  Serial.print(',');
  Serial.print(" HEATER_SHOULD_USE:");
  Serial.print(data.HEATER_SHOULD_USE);
  Serial.print(',');
  Serial.print(" PAYLOAD_SHOULD_USE:");
  Serial.print(data.PAYLOAD_SHOULD_USE);
  Serial.print(',');
  Serial.print(" HEATER_STRONG_ENABLE:");
  Serial.print(data.HEATER_STRONG_ENABLE);
  Serial.print(',');
  Serial.print(" HEATER_WEEK_ENABLE:");
  Serial.print(data.HEATER_WEEK_ENABLE);
  Serial.print(',');
  Serial.print(" GPS_GOOD_STATE:");
  Serial.print(data.GPS_GOOD_STATE);
  Serial.print(',');
  Serial.print(" SPEED_GPS:");
  Serial.print(data.SPEED_GPS);
  Serial.print(',');
  Serial.print(" HEADING_GPS:");
  Serial.print(data.HEADING_GPS);
  Serial.print(',');
  Serial.print(" NUM_SATS_GPS:");
  Serial.print(data.NUM_SATS_GPS);
  Serial.print(',');
  Serial.print(" INCENTIVE_NOISE:");
  Serial.print(data.INCENTIVE_NOISE);
  Serial.print(',');
  Serial.print(" RE_ARM_CONSTANT:");
  Serial.print(data.RE_ARM_CONSTANT);
  Serial.print(',');
  Serial.print(" VALVE_ALT_LAST:");
  Serial.print(data.VALVE_ALT_LAST);
  Serial.print(',');
  Serial.print(" BALLAST_ALT_LAST:");
  Serial.print(data.BALLAST_ALT_LAST);
  Serial.print(',');
  Serial.print(" SHOULD_LED:");
  Serial.print(data.SHOULD_LED);
  Serial.print(',');
  Serial.print(" DEBUG_STATE:");
  Serial.print(data.DEBUG_STATE);
  Serial.print(',');
  Serial.print(" FORCE_VALVE:");
  Serial.print(data.FORCE_VALVE);
  Serial.print(',');
  Serial.print(" FORCE_BALLAST:");
  Serial.print(data.FORCE_BALLAST);
  Serial.print(',');
  Serial.print(" BMP_1_ENABLE:");
  Serial.print(data.BMP_1_ENABLE);
  Serial.print(',');
  Serial.print(" BMP_2_ENABLE:");
  Serial.print(data.BMP_2_ENABLE);
  Serial.print(',');
  Serial.print(" BMP_3_ENABLE:");
  Serial.print(data.BMP_3_ENABLE);
  Serial.print(',');
  Serial.print(" BMP_4_ENABLE:");
  Serial.print(data.BMP_4_ENABLE);
  Serial.print(',');
  Serial.print(" BMP_1_REJECTIONS:");
  Serial.print(data.BMP_1_REJECTIONS);
  Serial.print(',');
  Serial.print(" BMP_2_REJECTIONS:");
  Serial.print(data.BMP_2_REJECTIONS);
  Serial.print(',');
  Serial.print(" BMP_3_REJECTIONS:");
  Serial.print(data.BMP_3_REJECTIONS);
  Serial.print(',');
  Serial.print(" BMP_4_REJECTIONS:");
  Serial.print(data.BMP_4_REJECTIONS);
  Serial.print(',');
  Serial.print(" TEMP_SETPOINT:");
  Serial.print(data.TEMP_SETPOINT);
  Serial.print(',');
  Serial.print(" COMMS_INTERVAL:");
  Serial.print(data.COMMS_INTERVAL);
  Serial.print(',');
  Serial.print(" GPS_INTERVAL:");
  Serial.print(data.GPS_INTERVAL);
  Serial.print(',');
  Serial.print(" PRESS_BASELINE:");
  Serial.print(data.PRESS_BASELINE);
  Serial.print(',');
  Serial.print(" INCENTIVE_THRESHOLD:");
  Serial.print(data.INCENTIVE_THRESHOLD);
  Serial.print(',');
  Serial.print(" BALLAST_ARM_ALT:");
  Serial.print(data.BALLAST_ARM_ALT);
  Serial.print(',');
  Serial.print(" BALLAST_REVERSE_TIMEOUT:");
  Serial.print(data.BALLAST_REVERSE_TIMEOUT);
  Serial.print(',');
  Serial.print(" BALLAST_STALL_CURRENT:");
  Serial.print(data.BALLAST_STALL_CURRENT);
  Serial.print(',');
  Serial.print(" VALVE_MOTOR_SPEED:");
  Serial.print(data.VALVE_MOTOR_SPEED);
  Serial.print(',');
  Serial.print(" BALLAST_MOTOR_SPEED:");
  Serial.print(data.BALLAST_MOTOR_SPEED);
  Serial.print(',');
  Serial.print(" VALVE_OPENING_TIMEOUT:");
  Serial.print(data.VALVE_OPENING_TIMEOUT);
  Serial.print(',');
  Serial.print(" VALVE_CLOSING_TIMEOUT:");
  Serial.print(data.VALVE_CLOSING_TIMEOUT);
  Serial.print(',');
  Serial.print(" VALVE_SETPOINT:");
  Serial.print(data.VALVE_SETPOINT);
  Serial.print(',');
  Serial.print(" VALVE_DURATION:");
  Serial.print(data.VALVE_DURATION);
  Serial.print(',');
  Serial.print(" VALVE_FORCE_DURATION:");
  Serial.print(data.VALVE_FORCE_DURATION);
  Serial.print(',');
  Serial.print(" VALVE_VELOCITY_CONSTANT:");
  Serial.print(data.VALVE_VELOCITY_CONSTANT);
  Serial.print(',');
  Serial.print(" VALVE_ALTITUDE_DIFF_CONSTANT (1.0 / ):");
  Serial.print(1.0 / data.VALVE_ALTITUDE_DIFF_CONSTANT);
  Serial.print(',');
  Serial.print(" VALVE_LAST_ACTION_CONSTANT (1.0 / ):");
  Serial.print(1.0 / data.VALVE_LAST_ACTION_CONSTANT);
  Serial.print(',');
  Serial.print(" BALLAST_SETPOINT:");
  Serial.print(data.BALLAST_SETPOINT);
  Serial.print(',');
  Serial.print(" BALLAST_DURATION:");
  Serial.print(data.BALLAST_DURATION);
  Serial.print(',');
  Serial.print(" BALLAST_FORCE_DURATION:");
  Serial.print(data.BALLAST_FORCE_DURATION);
  Serial.print(',');
  Serial.print(" BALLAST_VELOCITY_CONSTANT:");
  Serial.print(data.BALLAST_VELOCITY_CONSTANT);
  Serial.print(',');
  Serial.print(" BALLAST_ALTITUDE_DIFF_CONSTANT (1.0 / ):");
  Serial.print(1.0 / data.BALLAST_ALTITUDE_DIFF_CONSTANT);
  Serial.print(',');
  Serial.print(" BALLAST_LAST_ACTION_CONSTANT (1.0 / ):");
  Serial.print(1.0 / data.BALLAST_LAST_ACTION_CONSTANT);
  Serial.print(',');
  Serial.print(" SETUP_STATE:");
  Serial.print(data.SETUP_STATE);
  Serial.print(',');
  Serial.print(" SHOULD_CUTDOWN:");
  Serial.print(data.SHOULD_CUTDOWN);
  Serial.print(',');
  Serial.print(" RAW_TEMP_1:");
  Serial.print(data.RAW_TEMP_1);
  Serial.print(',');
  Serial.print(" RAW_TEMP_2:");
  Serial.print(data.RAW_TEMP_2);
  Serial.print(',');
  Serial.print(" RAW_TEMP_3:");
  Serial.print(data.RAW_TEMP_3);
  Serial.print(',');
  Serial.print(" RAW_TEMP_4:");
  Serial.print(data.RAW_TEMP_4);
  Serial.print(',');
  Serial.print(" RAW_PRESSURE_1:");
  Serial.print(data.RAW_PRESSURE_1);
  Serial.print(',');
  Serial.print(" RAW_PRESSURE_2:");
  Serial.print(data.RAW_PRESSURE_2);
  Serial.print(',');
  Serial.print(" RAW_PRESSURE_3:");
  Serial.print(data.RAW_PRESSURE_3);
  Serial.print(',');
  Serial.print(" RAW_PRESSURE_4:");
  Serial.print(data.RAW_PRESSURE_4);
  Serial.print(',');
  Serial.print(" PRESS:");
  Serial.print(data.PRESS);
  Serial.print(',');
  Serial.print(" CURRENT:");
  Serial.print(data.CURRENT);
  Serial.print(',');
  Serial.print(" CURRENT_GPS:");
  Serial.print(data.CURRENT_GPS);
  Serial.print(',');
  Serial.print(" CURRENT_RB:");
  Serial.print(data.CURRENT_RB);
  Serial.print(',');
  Serial.print(" CURRENT_MOTORS:");
  Serial.print(data.CURRENT_MOTORS);
  Serial.print(',');
  Serial.print(" CURRENT_PAYLOAD:");
  Serial.print(data.CURRENT_PAYLOAD);
  Serial.print(',');
  Serial.print(" EULER_X:");
  Serial.print(data.EULER_X);
  Serial.print(',');
  Serial.print(" EULER_Y:");
  Serial.print(data.EULER_Y);
  Serial.print(',');
  Serial.print(" EULER_Z:");
  Serial.print(data.EULER_Z);
  Serial.print(',');
  Serial.print(" GPS_LAST:");
  Serial.print(data.GPS_LAST);
  Serial.print(',');
  Serial.print(" COMMS_LAST:");
  Serial.print(data.COMMS_LAST);
  Serial.print(',');
  Serial.print(" DATAFILE_LAST:");
  Serial.print(data.DATAFILE_LAST);
  Serial.print(',');
  Serial.print(" COMMS_LENGTH:");
  Serial.print(data.COMMS_LENGTH);
  Serial.print("\n\r");
  Serial.print("\n\r");
}

/*
 * Function: logData
 * -------------------
 * This function logs the current data frame.
 */
bool Avionics::logData() {
  bool sucess = true;
  dataFile.print(data.TIME);
  dataFile.print(',');
  dataFile.print(data.LAT_GPS, 4);
  dataFile.print(',');
  dataFile.print(data.LONG_GPS, 4);
  dataFile.print(',');
  dataFile.print(data.ALTITUDE);
  dataFile.print(',');
  dataFile.print(data.ALTITUDE_GPS);
  dataFile.print(',');
  dataFile.print(data.ASCENT_RATE);
  dataFile.print(',');
  dataFile.print(data.VALVE_INCENTIVE);
  dataFile.print(',');
  dataFile.print(data.BALLAST_INCENTIVE);
  dataFile.print(',');
  dataFile.print(data.VALVE_STATE);
  dataFile.print(',');
  dataFile.print(data.BALLAST_STATE);
  dataFile.print(',');
  dataFile.print(data.VALVE_QUEUE);
  dataFile.print(',');
  dataFile.print(data.BALLAST_QUEUE);
  dataFile.print(',');
  dataFile.print(data.VALVE_TIME_TOTAL);
  dataFile.print(',');
  dataFile.print(data.BALLAST_TIME_TOTAL);
  dataFile.print(',');
  dataFile.print(data.NUM_VALVES);
  dataFile.print(',');
  dataFile.print(data.NUM_BALLASTS);
  dataFile.print(',');
  dataFile.print(data.NUM_VALVE_ATTEMPTS);
  dataFile.print(',');
  dataFile.print(data.NUM_BALLAST_ATTEMPTS);
  dataFile.print(',');
  dataFile.print(data.NUM_BALLAST_OVER_CURRENTS);
  dataFile.print(',');
  dataFile.print(data.CUTDOWN_STATE);
  dataFile.print(',');
  dataFile.print(data.TEMP_IN);
  dataFile.print(',');
  dataFile.print(data.JOULES);
  dataFile.print(',');
  dataFile.print(data.VOLTAGE);
  dataFile.print(',');
  dataFile.print(data.CURRENT_AVG);
  dataFile.print(',');
  dataFile.print(data.CURRENT_MIN);
  dataFile.print(',');
  dataFile.print(data.CURRENT_MAX);
  dataFile.print(',');
  dataFile.print(data.CURRENT_GPS_AVG);
  dataFile.print(',');
  dataFile.print(data.CURRENT_GPS_MAX);
  dataFile.print(',');
  dataFile.print(data.CURRENT_RB_AVG);
  dataFile.print(',');
  dataFile.print(data.CURRENT_RB_MAX);
  dataFile.print(',');
  dataFile.print(data.CURRENT_MOTORS_AVG);
  dataFile.print(',');
  dataFile.print(data.CURRENT_MOTORS_MAX);
  dataFile.print(',');
  dataFile.print(data.CURRENT_PAYLOAD_AVG);
  dataFile.print(',');
  dataFile.print(data.CURRENT_PAYLOAD_MAX);
  dataFile.print(',');
  dataFile.print(data.TEMP_EXT);
  dataFile.print(',');
  dataFile.print(data.LOOP_TIME);
  dataFile.print(',');
  dataFile.print(data.RB_SENT_COMMS);
  dataFile.print(',');
  dataFile.print(data.EULER_X_AVG);
  dataFile.print(',');
  dataFile.print(data.EULER_Y_AVG);
  dataFile.print(',');
  dataFile.print(data.EULER_Z_AVG);
  dataFile.print(',');
  dataFile.print(data.MANUAL_MODE);
  dataFile.print(',');
  dataFile.print(data.REPORT_MODE);
  dataFile.print(',');
  dataFile.print(data.SHOULD_REPORT);
  dataFile.print(',');
  dataFile.print(data.RB_SHOULD_USE);
  dataFile.print(',');
  dataFile.print(data.GPS_SHOULD_USE);
  dataFile.print(',');
  dataFile.print(data.HEATER_SHOULD_USE);
  dataFile.print(',');
  dataFile.print(data.PAYLOAD_SHOULD_USE);
  dataFile.print(',');
  dataFile.print(data.HEATER_STRONG_ENABLE);
  dataFile.print(',');
  dataFile.print(data.HEATER_WEEK_ENABLE);
  dataFile.print(',');
  dataFile.print(data.GPS_GOOD_STATE);
  dataFile.print(',');
  dataFile.print(data.SPEED_GPS);
  dataFile.print(',');
  dataFile.print(data.HEADING_GPS);
  dataFile.print(',');
  dataFile.print(data.NUM_SATS_GPS);
  dataFile.print(',');
  dataFile.print(data.INCENTIVE_NOISE);
  dataFile.print(',');
  dataFile.print(data.RE_ARM_CONSTANT);
  dataFile.print(',');
  dataFile.print(data.VALVE_ALT_LAST);
  dataFile.print(',');
  dataFile.print(data.BALLAST_ALT_LAST);
  dataFile.print(',');
  dataFile.print(data.SHOULD_LED);
  dataFile.print(',');
  dataFile.print(data.DEBUG_STATE);
  dataFile.print(',');
  dataFile.print(data.FORCE_VALVE);
  dataFile.print(',');
  dataFile.print(data.FORCE_BALLAST);
  dataFile.print(',');
  dataFile.print(data.BMP_1_ENABLE);
  dataFile.print(',');
  dataFile.print(data.BMP_2_ENABLE);
  dataFile.print(',');
  dataFile.print(data.BMP_3_ENABLE);
  dataFile.print(',');
  dataFile.print(data.BMP_4_ENABLE);
  dataFile.print(',');
  dataFile.print(data.BMP_1_REJECTIONS);
  dataFile.print(',');
  dataFile.print(data.BMP_2_REJECTIONS);
  dataFile.print(',');
  dataFile.print(data.BMP_3_REJECTIONS);
  dataFile.print(',');
  dataFile.print(data.BMP_4_REJECTIONS);
  dataFile.print(',');
  dataFile.print(data.TEMP_SETPOINT);
  dataFile.print(',');
  dataFile.print(data.COMMS_INTERVAL);
  dataFile.print(',');
  dataFile.print(data.GPS_INTERVAL);
  dataFile.print(',');
  dataFile.print(data.PRESS_BASELINE);
  dataFile.print(',');
  dataFile.print(data.INCENTIVE_THRESHOLD);
  dataFile.print(',');
  dataFile.print(data.BALLAST_ARM_ALT);
  dataFile.print(',');
  dataFile.print(data.BALLAST_REVERSE_TIMEOUT);
  dataFile.print(',');
  dataFile.print(data.BALLAST_STALL_CURRENT);
  dataFile.print(',');
  dataFile.print(data.VALVE_MOTOR_SPEED);
  dataFile.print(',');
  dataFile.print(data.BALLAST_MOTOR_SPEED);
  dataFile.print(',');
  dataFile.print(data.VALVE_OPENING_TIMEOUT);
  dataFile.print(',');
  dataFile.print(data.VALVE_CLOSING_TIMEOUT);
  dataFile.print(',');
  dataFile.print(data.VALVE_SETPOINT);
  dataFile.print(',');
  dataFile.print(data.VALVE_DURATION);
  dataFile.print(',');
  dataFile.print(data.VALVE_FORCE_DURATION);
  dataFile.print(',');
  dataFile.print(data.VALVE_VELOCITY_CONSTANT);
  dataFile.print(',');
  dataFile.print(1.0 / data.VALVE_ALTITUDE_DIFF_CONSTANT);
  dataFile.print(',');
  dataFile.print(1.0 / data.VALVE_LAST_ACTION_CONSTANT);
  dataFile.print(',');
  dataFile.print(data.BALLAST_SETPOINT);
  dataFile.print(',');
  dataFile.print(data.BALLAST_DURATION);
  dataFile.print(',');
  dataFile.print(data.BALLAST_FORCE_DURATION);
  dataFile.print(',');
  dataFile.print(data.BALLAST_VELOCITY_CONSTANT);
  dataFile.print(',');
  dataFile.print(1.0 / data.BALLAST_ALTITUDE_DIFF_CONSTANT);
  dataFile.print(',');
  dataFile.print(1.0 / data.BALLAST_LAST_ACTION_CONSTANT);
  dataFile.print(',');
  dataFile.print(data.SETUP_STATE);
  dataFile.print(',');
  dataFile.print(data.SHOULD_CUTDOWN);
  dataFile.print(',');
  dataFile.print(data.RAW_TEMP_1);
  dataFile.print(',');
  dataFile.print(data.RAW_TEMP_2);
  dataFile.print(',');
  dataFile.print(data.RAW_TEMP_3);
  dataFile.print(',');
  dataFile.print(data.RAW_TEMP_4);
  dataFile.print(',');
  dataFile.print(data.RAW_PRESSURE_1);
  dataFile.print(',');
  dataFile.print(data.RAW_PRESSURE_2);
  dataFile.print(',');
  dataFile.print(data.RAW_PRESSURE_3);
  dataFile.print(',');
  dataFile.print(data.RAW_PRESSURE_4);
  dataFile.print(',');
  dataFile.print(data.PRESS);
  dataFile.print(',');
  dataFile.print(data.CURRENT);
  dataFile.print(',');
  dataFile.print(data.CURRENT_GPS);
  dataFile.print(',');
  dataFile.print(data.CURRENT_RB);
  dataFile.print(',');
  dataFile.print(data.CURRENT_MOTORS);
  dataFile.print(',');
  dataFile.print(data.CURRENT_PAYLOAD);
  dataFile.print(',');
  dataFile.print(data.EULER_X);
  dataFile.print(',');
  dataFile.print(data.EULER_Y);
  dataFile.print(',');
  dataFile.print(data.EULER_Z);
  dataFile.print(',');
  dataFile.print(data.GPS_LAST);
  dataFile.print(',');
  dataFile.print(data.COMMS_LAST);
  dataFile.print(',');
  dataFile.print(data.DATAFILE_LAST);
  if(dataFile.print(',') != 1) sucess = false;
  dataFile.print(data.COMMS_LENGTH);
  dataFile.print('\n');
  dataFile.flush();
  logFile.flush();
  return sucess;
}
