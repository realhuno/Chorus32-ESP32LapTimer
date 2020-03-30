#include "Comms.h"

#include "RX5808.h"
#include "Utils.h"
#include "HardwareConfig.h"
#include "Output.h"
#include "settings_eeprom.h"
#include "Laptime.h"
#include "ADC.h"
#include "Calibration.h"
#include "Logging.h"

///////This is mostly from the original Chorus Laptimer, need to cleanup unused functions and variables

// API in brief (sorted alphabetically):
// Req  Resp Description
// 1    1    first lap counts (opposite to prev API)
// B    B    band
// C    C    channel
// F    F    freq
// H    H    threshold setup mode
//      L    lap (response only)
// I    I    rssi monitor interval (0 = off)
// J    J    time adjustment constant
// M    M    min lap time
// R    R    race mode
// S    S    device sounds state
// T    T    threshold
// get only:
// #    #    api version #
// a    ...  all device state
// r    r    rssi value
// t    t    time in milliseconds
// v    v    voltage
//      x    end of sequence sign (response to "a")
// y    y    is module configured (response to "a")

// input control byte constants
// get/set:
#define CONTROL_WAIT_FIRST_LAP      '1'
#define CONTROL_BAND                'B'
#define CONTROL_CHANNEL             'C'
#define CONTROL_FREQUENCY           'F'
#define CONTROL_THRESHOLD_SETUP     'H'
#define CONTROL_RSSI_MON_INTERVAL   'I'
#define CONTROL_TIME_ADJUSTMENT     'J'
#define CONTROL_RACE_MODE           'R'
#define CONTROL_MIN_LAP_TIME        'M'
#define CONTROL_NUM_RECIEVERS       'N'
#define CONTROL_SOUND               'S'
#define CONTROL_THRESHOLD           'T'
#define CONTROL_PILOT_ACTIVE        'A'
#define CONTROL_EXPERIMENTAL_MODE   'E'
// get only:
#define CONTROL_GET_API_VERSION     '#'
#define CONTROL_WILDCARD_INDICATOR  '*'
#define CONTROL_GET_ALL_DATA        'a'
#define CONTROL_GET_RSSI            'r'
#define CONTROL_GET_TIME            't'
#define CONTROL_GET_VOLTAGE         'v'
#define CONTROL_GET_IS_CONFIGURED   'y'
#define CONTROL_PING                '%'

// output id byte constants
#define RESPONSE_WAIT_FIRST_LAP      '1'
#define RESPONSE_BAND                'B'
#define RESPONSE_CHANNEL             'C'
#define RESPONSE_FREQUENCY           'F'
#define RESPONSE_THRESHOLD_SETUP     'H'
#define RESPONSE_RSSI_MON_INTERVAL   'I'
#define RESPONSE_TIME_ADJUSTMENT     'J'
#define RESPONSE_LAPTIME             'L'
#define RESPONSE_RACE_MODE           'R'
#define RESPONSE_MIN_LAP_TIME        'M'
#define RESPONSE_SOUND               'S'
#define RESPONSE_THRESHOLD           'T'
#define RESPONSE_PILOT_ACTIVE        'A'
#define RESPONSE_EXPERIMENTAL_MODE   'E'

#define RESPONSE_API_VERSION         '#'
#define RESPONSE_RSSI                'r'
#define RESPONSE_TIME                't'
#define RESPONSE_VOLTAGE             'v'
#define RESPONSE_END_SEQUENCE        'x'
#define RESPONSE_IS_CONFIGURED       'y'
#define RESPONSE_PING                '%'

// Extended commands. These are 100% unofficial
#define EXTENDED_PREFIX   'E'

#define EXTENDED_ALL_SETTINGS 'a'

#define EXTENDED_RACE_NUM 'R'
#define EXTENDED_CALIB_MIN 'c'
#define EXTENDED_CALIB_MAX 'C'
#define EXTENDED_CALIB_START 's'
#define EXTENDED_CALIB_STATUS 'S'
#define EXTENDED_VOLTAGE_TYPE 'v'
#define EXTENDED_VOLTAGE_CALIB 'V'
#define EXTENDED_NUM_MODULES 'M'
#define EXTENDED_CALIBRATE_START 'r'
#define EXTENDED_EEPROM_RESET 'E'
#define EXTENDED_DISPLAY_TIMEOUT 'D'
#define EXTENDED_WIFI_CHANNEL 'W'
#define EXTENDED_WIFI_PROTOCOL 'w'
#define EXTENDED_FILTER_CUTOFF 'F'
#define EXTENDED_MULTIPLEX_OFF 'm'
#define EXTENDED_UPDATE_PROGRESS 'U'
#define EXTENDED_RSSI 'y' // Time, RSSI
#define EXTENDED_DEBUG_FREE_HEAP 'H'
#define EXTENDED_DEBUG_MIN_FREE_HEAP 'h'
#define EXTENDED_DEBUG_MAX_BLOCK_HEAP 'B'
#define EXTENDED_DEBUG_LOG 'L' // one halfbyte 0: off 1: on

// Binary commands. These are used for messages which are sent very often to reduce the overhead. e.g. for RSSI updates
// Prefix | CMD  | data (contains node id if needed)
// 8bit   | 8bit | variable | newline (for compatability)
// TODO: on hold for now, because of javascript
#define BINARY_PREFIX 'B'
#define BINARY_RSSI 1 // node id(8), time(32), rssi(16)

// send item byte constants
// Must correspond to sequence of numbers used in "send data" switch statement
// Subsequent items starting from 0 participate in "send all data" response
#define SEND_CHANNEL            0
#define SEND_RACE_MODE          1
#define SEND_MIN_LAP_TIME       2
#define SEND_THRESHOLD          3
#define SEND_ALL_LAPTIMES       4
#define SEND_SOUND_STATE        5
#define SEND_BAND               6
#define SEND_LAP0_STATE         7
#define SEND_IS_CONFIGURED      8
#define SEND_FREQUENCY          9
#define SEND_MON_INTERVAL       10
#define SEND_TIME_ADJUSTMENT    11
#define SEND_API_VERSION        12
#define SEND_VOLTAGE            13
#define SEND_THRESHOLD_SETUP_MODE 14
#define SEND_END_SEQUENCE       15
// following items don't participate in "send all items" response
#define SEND_LAST_LAPTIMES          100
#define SEND_TIME                   101
#define SEND_CURRENT_RSSI           102
// special item that sends all subsequent items from 0 (see above)
#define SEND_ALL_DEVICE_STATE       255

//----- RSSI --------------------------------------
static uint16_t rssiThreshold = 190;

static uint32_t lastRSSIsent;

static uint16_t rssiMonitorInterval = 0; // zero means the RSSI monitor is OFF

static uint16_t extendedRssiMonitorInterval = 0; // zero means the RSSI monitor is OFF

#define RSSI_SETUP_INITIALIZE 0
#define RSSI_SETUP_NEXT_STEP 1

//----- Lap timings--------------------------------
#define MIN_MIN_LAP_TIME 1 //seconds
#define MAX_MIN_LAP_TIME 120 //seconds

//----- Time Adjustment (for accuracy) ------------
#define INFINITE_TIME_ADJUSTMENT 0x7FFFFFFF // max positive 32 bit signed number
// Usage of signed int time adjustment constant inside this firmware:
// * calibratedMs = readMs + readMs/timeAdjustment
// Usage of signed int time adjustment constant from outside:
// * set to zero means time adjustment procedure was not performed for this node
// * set to INFINITE_TIME_ADJUSTMENT, means time adjustment was performed, but no need to adjust
static int32_t timeAdjustment = INFINITE_TIME_ADJUSTMENT;

//----- other globals------------------------------
static uint8_t raceMode = 0; // 0: race mode is off; 1: lap times are counted relative to last lap end; 2: lap times are relative to the race start (sum of all previous lap times);
//static uint8_t isSoundEnabled = 1; // TODO: implement this option
static uint8_t isConfigured = 0; //changes to 1 if any input changes the state of the device. it will mean that externally stored preferences should not be applied
static uint8_t use_experimental = 0;

static uint8_t thresholdSetupMode[MAX_NUM_PILOTS];

void sendExtendedCommandInt32(uint8_t set, uint8_t node, uint8_t cmd, uint32_t data) {
  uint8_t buf[13];
  buf[0] = EXTENDED_PREFIX;
  buf[1] = 'S';
  if(node == '*') {
    buf[2] = node;
  } else {
    buf[2] = TO_HEX(node);
  }
  buf[3] = cmd;
  longToHex(buf + 4, data);
  buf[12] = '\n';
  addToSendQueue(buf, 13);
}
// TODO: unify those functions
// this is 16bit
void sendExtendedCommandInt(uint8_t set, uint8_t node, uint8_t cmd, int data) {
  uint8_t buf[9];
  buf[0] = EXTENDED_PREFIX;
  buf[1] = 'S';
  if(node == '*') {
    buf[2] = node;
  } else {
    buf[2] = TO_HEX(node);
  }
  buf[3] = cmd;
  intToHex(buf + 4, data);
  buf[8] = '\n';
  addToSendQueue(buf, 9);
}

void sendExtendedCommandByte(uint8_t set, uint8_t node, uint8_t cmd, uint8_t data) {
  addToSendQueue(EXTENDED_PREFIX);
  addToSendQueue('S');
  if(node == '*') {
    addToSendQueue(node);
  } else {
    addToSendQueue(TO_HEX(node));
  }
  addToSendQueue(cmd);
  uint8_t buf[2];
  byteToHex(buf, data);
  addToSendQueue(buf, 2);
  addToSendQueue('\n');
}

void sendExtendedCommandHalfByte(uint8_t set, uint8_t node, uint8_t cmd, uint8_t data) {
  addToSendQueue(EXTENDED_PREFIX);
  addToSendQueue('S');
  if(node == '*') {
    addToSendQueue(node);
  } else {
    addToSendQueue(TO_HEX(node));
  }
  addToSendQueue(cmd);
  addToSendQueue(TO_HEX(data));
  addToSendQueue('\n');
}

void sendBinaryRSSI(uint8_t node, uint32_t time, uint16_t rssi) {
  uint8_t buf[10];
  buf[0] = BINARY_PREFIX;
  buf[1] = BINARY_RSSI;
  buf[2] = node;
  memcpy(buf + 3, &time, 4);
  memcpy(buf + 7, &rssi, 2);
  buf[9] = '\n';
  addToSendQueue(buf, 10);
}

void sendExtendedRSSI(uint8_t node, uint32_t time, uint16_t rssi) {
  uint8_t buf[17];
  buf[0] = EXTENDED_PREFIX;
  buf[1] = 'S';
  buf[2] = TO_HEX(node);
  buf[3] = EXTENDED_RSSI;
  longToHex(buf + 4, time); // ends at 11
  intToHex(buf + 12, rssi); // ends at 15
  buf[16] = '\n';
  addToSendQueue(buf, 17);
}

void sendCalibrationFinished() {
  sendExtendedCommandHalfByte('S', '*', EXTENDED_CALIB_STATUS, 1);
}

static void sendThresholdMode(uint8_t node) {
  addToSendQueue('S');
  addToSendQueue(TO_HEX(node));
  addToSendQueue(RESPONSE_THRESHOLD_SETUP);
  addToSendQueue(TO_HEX(thresholdSetupMode[node]));
  addToSendQueue('\n');
}

void commsSetup() {
  for (int i = 0; i < MAX_NUM_PILOTS; i++) {
    thresholdSetupMode[i] = 0;
  }
}

void setRaceMode(uint8_t mode) {
  if (mode == 0) { // stop race
    raceMode = 0;
    //playEndRaceTones();
  } else { // start race in specified mode
    //holeShot = true;
    raceMode = mode;
    startRaceLap();
    sendExtendedCommandByte('S', '*', EXTENDED_RACE_NUM, getRaceNum());
    for(uint8_t i = 0; i < MAX_NUM_PILOTS; ++i) {
      if(thresholdSetupMode[i]) {
        thresholdSetupMode[i] = 0;
        sendThresholdMode(i);
      }
    }
    //playStartRaceTones();
  }
}

void setMinLap(uint8_t mlt) {
  if (mlt >= MIN_MIN_LAP_TIME && mlt <= MAX_MIN_LAP_TIME) {
    setMinLapTime(mlt * 1000);
  }
}

void SendMinLap(uint8_t NodeAddr) {
  addToSendQueue('S');
  addToSendQueue(TO_HEX(NodeAddr));
  addToSendQueue(RESPONSE_MIN_LAP_TIME);
  addToSendQueue('0');
  addToSendQueue(TO_HEX((getMinLapTime() / 1000)));
  addToSendQueue('\n');
  isConfigured = 1;
}

void SendIsModuleConfigured() {
  for (int i = 0; i < MAX_NUM_PILOTS; i ++) {
    addToSendQueue('S');
    addToSendQueue(TO_HEX(i));
    addToSendQueue(RESPONSE_IS_CONFIGURED);
    addToSendQueue(TO_HEX(isConfigured));
    addToSendQueue('\n');
  }
}

void SendXdone(uint8_t NodeAddr) {
  addToSendQueue('S');
  addToSendQueue(TO_HEX(NodeAddr));
  addToSendQueue(RESPONSE_END_SEQUENCE);
  addToSendQueue('1');
  addToSendQueue('\n');
}

void SetThresholdValue(uint16_t threshold, uint8_t NodeAddr) {
  Serial.print("Setting Threshold Value: ");
  Serial.println(threshold);
  if (threshold > 340) {
    threshold = 340;
    Serial.println("Threshold was attempted to be set out of range");
  }
  // stop the "setting threshold algorithm" to avoid overwriting the explicitly set value
  if (thresholdSetupMode[NodeAddr]) {
    thresholdSetupMode[NodeAddr] = 0;
    sendThresholdMode(NodeAddr);
  }
  setRSSIThreshold(NodeAddr, threshold * 12);
  EepromSettings.RSSIthresholds[NodeAddr] = getRSSIThreshold(NodeAddr);
  setSaveRequired();
  if (threshold != 0) {
    //playClickTones();
  } else {
    //playClearThresholdTones();
  }
}

void SendMillis() {
  uint32_t CurrMillis = millis();
  uint8_t buf[8];
  longToHex(buf, CurrMillis);

  for (int i = 0; i < MAX_NUM_PILOTS; i ++) {
    addToSendQueue('S');
    addToSendQueue(TO_HEX(i));
    addToSendQueue(RESPONSE_TIME);
    addToSendQueue(buf, 8);
    addToSendQueue('\n');
  }
}

void SendThresholdValue(uint8_t NodeAddr) {
  addToSendQueue('S');
  addToSendQueue(TO_HEX(NodeAddr));
  addToSendQueue(RESPONSE_THRESHOLD);
  uint8_t buf[4];
  intToHex(buf, getRSSIThreshold(NodeAddr) / 12);
  addToSendQueue(buf, 4);
  addToSendQueue('\n');
}

void SendCurrRSSI(uint8_t NodeAddr) {

  ///Calculate Averages///
  uint16_t Result = getRSSI(NodeAddr);

  //MirrorToSerial = false;  // this so it doesn't spam the serial console with RSSI updates
  addToSendQueue('S');
  addToSendQueue(TO_HEX(NodeAddr));
  addToSendQueue(RESPONSE_RSSI);
  uint8_t buf[4];
  intToHex(buf, Result / 12);
  addToSendQueue(buf, 4);
  addToSendQueue('\n');
  //MirrorToSerial = true;
  lastRSSIsent = millis();

}

void ExtendedRSSILoop() {
  static uint32_t last_extended_sent = 0;
  if (extendedRssiMonitorInterval == 0) {
    return;
  }
  if (millis() > extendedRssiMonitorInterval + last_extended_sent) {
    last_extended_sent = millis();
    for (int i = 0; i < MAX_NUM_PILOTS; ++i) {
      if(isPilotActive(i)) {
        sendExtendedRSSI(i, millis(), getRSSI(i));
      }
    }
  }
}

void SendCurrRSSIloop() {
  if (rssiMonitorInterval == 0) {
    return;
  }
  if (millis() > rssiMonitorInterval + lastRSSIsent) {
    for (int i = 0; i < MAX_NUM_PILOTS; i ++) {
      SendCurrRSSI(i);
    }
  }
}

void setupThreshold(uint8_t phase, uint8_t node) {
  // this process assumes the following:
  // 1. before the process all VTXs are turned ON, but are distant from the Chorus device, so that Chorus sees the "background" rssi values only
  // 2. once the setup process is initiated by Chorus operator, all pilots walk towards the Chorus device
  // 3. setup process starts tracking top rssi values
  // 4. as pilots come closer, rssi should rise above the value defined by RISE_RSSI_THRESHOLD_PERCENT
  // 5. after that setup expects rssi to fall from the reached top, down by FALL_RSSI_THRESHOLD_PERCENT
  // 6. after the rssi falls, the top recorded value (decreased by TOP_RSSI_DECREASE_PERCENT) is set as a threshold

  // time constant for accumulation filter: higher value => more delay
  // value of 20 should give about 100 readings before value reaches the settled rssi
  // don't make it bigger than 2000 to avoid overflow of accumulatedShiftedRssi
#define ACCUMULATION_TIME_CONSTANT 150
#define MILLIS_BETWEEN_ACCU_READS 10 // artificial delay between rssi reads to slow down the accumulation
#define TOP_RSSI_DECREASE_PERCENT 10 // decrease top value by this percent using diff between low and high as a base
#define RISE_RSSI_THRESHOLD_PERCENT 25 // rssi value should pass this percentage above low value to continue finding the peak and further fall down of rssi
#define FALL_RSSI_THRESHOLD_PERCENT 50 // rssi should fall below this percentage of diff between high and low to finalize setup of the threshold

  static uint16_t rssiLow[MAX_NUM_PILOTS];
  static uint16_t rssiHigh[MAX_NUM_PILOTS];
  static uint16_t rssiHighEnoughForMonitoring[MAX_NUM_PILOTS];
  static uint32_t accumulatedShiftedRssi[MAX_NUM_PILOTS]; // accumulates rssi slowly; contains multiplied rssi value for better accuracy
  static uint32_t lastRssiAccumulationTime[MAX_NUM_PILOTS];

  if (!thresholdSetupMode[node]) return;

  uint16_t rssi = getRSSI(node);

  if (phase == RSSI_SETUP_INITIALIZE) {
    // initialization step
    //playThresholdSetupStartTones();
    thresholdSetupMode[node] = 1;
    rssiLow[node] = rssi; // using slowRssi to avoid catching random current rssi
    rssiHigh[node] = rssiLow[node];
    accumulatedShiftedRssi[node] = rssiLow[node] * ACCUMULATION_TIME_CONSTANT; // multiply to prevent loss in accuracy
    rssiHighEnoughForMonitoring[node] = rssiLow[node] + rssiLow[node] * RISE_RSSI_THRESHOLD_PERCENT / 100;
    lastRssiAccumulationTime[node] = millis();
    sendThresholdMode(node);
  } else {
    // active phase step (searching for high value and fall down)
    if (thresholdSetupMode[node] == 1) {
      // in this phase of the setup we are tracking rssi growth until it reaches the predefined percentage from low
      // searching for peak; using slowRssi to avoid catching sudden random peaks
      if (rssi > rssiHigh[node]) {
        rssiHigh[node] = rssi;
      }

      // since filter runs too fast, we have to introduce a delay between subsequent readings of filter values
      uint32_t curTime = millis();
      if ((curTime - lastRssiAccumulationTime[node]) > MILLIS_BETWEEN_ACCU_READS) {
        lastRssiAccumulationTime[node] = curTime;
        // this is actually a filter with a delay determined by ACCUMULATION_TIME_CONSTANT
        accumulatedShiftedRssi[node] = rssi  + (accumulatedShiftedRssi[node] * (ACCUMULATION_TIME_CONSTANT - 1) / ACCUMULATION_TIME_CONSTANT);
      }

      uint16_t accumulatedRssi = accumulatedShiftedRssi[node] / ACCUMULATION_TIME_CONSTANT; // find actual rssi from multiplied value

      if (accumulatedRssi > rssiHighEnoughForMonitoring[node]) {
        thresholdSetupMode[node] = 2;
        accumulatedShiftedRssi[node] = rssiHigh[node] * ACCUMULATION_TIME_CONSTANT;
        //playThresholdSetupMiddleTones();
        sendThresholdMode(node);
      }
    } else {
      // in this phase of the setup we are tracking highest rssi and expect it to fall back down so that we know that the process is complete
      // continue searching for peak; using slowRssi to avoid catching sudden random peaks
      if (rssi > rssiHigh[node]) {
        rssiHigh[node] = rssi;
        accumulatedShiftedRssi[node] = rssiHigh[node] * ACCUMULATION_TIME_CONSTANT; // set to highest found rssi
      }

      // since filter runs too fast, we have to introduce a delay between subsequent readings of filter values
      uint32_t curTime = millis();
      if ((curTime - lastRssiAccumulationTime[node]) > MILLIS_BETWEEN_ACCU_READS) {
        lastRssiAccumulationTime[node] = curTime;
        // this is actually a filter with a delay determined by ACCUMULATION_TIME_CONSTANT
        accumulatedShiftedRssi[node] = rssi  + (accumulatedShiftedRssi[node] * (ACCUMULATION_TIME_CONSTANT - 1) / ACCUMULATION_TIME_CONSTANT );
      }
      uint16_t accumulatedRssi = accumulatedShiftedRssi[node] / ACCUMULATION_TIME_CONSTANT;

      uint16_t rssiLowEnoughForSetup = rssiHigh[node] - (rssiHigh[node] - rssiLow[node]) * FALL_RSSI_THRESHOLD_PERCENT / 100;
      if (accumulatedRssi < rssiLowEnoughForSetup) {
        rssiThreshold = rssiHigh[node] - ((rssiHigh[node] - rssiLow[node]) * TOP_RSSI_DECREASE_PERCENT) / 100;
        SetThresholdValue(rssiThreshold / 12, node); // Function expects the threshold in / 12
        thresholdSetupMode[node] = 0;
        isConfigured = 1;
        //playThresholdSetupDoneTones();
        sendThresholdMode(node);
        SendThresholdValue(node);
      }
    }
  }
}

void IRAM_ATTR sendLap(uint8_t Lap, uint8_t NodeAddr) {
  uint32_t RequestedLap = 0;

  if (Lap == 0) {
    Serial.println("Lap == 0 and sendlap was called");
    return;
  }

  if (raceMode == 1) {
    RequestedLap = getLaptimeRel(NodeAddr, Lap); // realtive mode
  } else if (raceMode == 2) {
    RequestedLap = getLaptimeRelToStart(NodeAddr, Lap);  //absolute mode
  } else {
    Serial.println("Error: Invalid RaceMode Set");
    return;
  }

  uint8_t buf1[2];
  uint8_t buf2[8];

  addToSendQueue('S');
  addToSendQueue(TO_HEX(NodeAddr));
  addToSendQueue(RESPONSE_LAPTIME);

  byteToHex(buf1, Lap - 1);
  addToSendQueue(buf1, 2);

  longToHex(buf2, RequestedLap);
  addToSendQueue(buf2, 8);
  addToSendQueue('\n');
}

void SendNumberOfnodes() {
    addToSendQueue('N');
    addToSendQueue(TO_HEX(getNumReceivers()));
    addToSendQueue('\n');
}

void IRAM_ATTR SendAllLaps(uint8_t NodeAddr) {
  uint8_t lap_count = getCurrentLap(NodeAddr);
  for (uint8_t i = 0; i < lap_count; i++) {
    sendLap(i + 1, NodeAddr);
    update_outputs(); // Flush outputs as the buffer could overflow with a large number of laps
  }
}

void sendPilotActive(uint8_t pilot) {
  uint8_t status = isPilotActive(pilot);
  addToSendQueue('S');
  addToSendQueue(TO_HEX(pilot));
  addToSendQueue('A');
  addToSendQueue(TO_HEX(status));
  addToSendQueue('\n');
}

void sendExperimentalMode(uint8_t NodeAddr) {
  uint8_t status = use_experimental;
  addToSendQueue('S');
  addToSendQueue(TO_HEX(NodeAddr));
  addToSendQueue(RESPONSE_EXPERIMENTAL_MODE);
  addToSendQueue(TO_HEX(status));
  addToSendQueue('\n');
}

void SendRSSImonitorInterval(uint8_t NodeAddr) {
  addToSendQueue('S');
  addToSendQueue(TO_HEX(NodeAddr));
  uint8_t buf[4];
  addToSendQueue(RESPONSE_RSSI_MON_INTERVAL);
  intToHex(buf, rssiMonitorInterval);
  addToSendQueue(buf, 4);
  addToSendQueue('\n');
}

void SendSoundMode(uint8_t NodeAddr) {
  addToSendQueue('S');
  addToSendQueue(TO_HEX(NodeAddr));
  addToSendQueue(RESPONSE_SOUND);
  addToSendQueue('0');
  addToSendQueue('\n');
}

void SendLipoVoltage() {
  addToSendQueue('S');
  addToSendQueue(TO_HEX(0));
  addToSendQueue(RESPONSE_VOLTAGE);
  uint8_t buf[4];
  float VbatFloat = 0;

  if(getADCVBATmode() != OFF) {
    VbatFloat = (getVbatFloat() / 11.0) * (1024.0 / 5.0); // App expects raw pin reading through a potential divider.
  }

  intToHex(buf, int(VbatFloat));
  addToSendQueue(buf, 4);
  addToSendQueue('\n');
}

void WaitFirstLap(uint8_t NodeAddr) {
  addToSendQueue('S');
  addToSendQueue(TO_HEX(NodeAddr));
  addToSendQueue(RESPONSE_WAIT_FIRST_LAP);
  addToSendQueue(TO_HEX(getCountFirstLap()));
  addToSendQueue('\n');
}

void SendTimerCalibration(uint8_t NodeAddr) {

  uint8_t buf[8];
  longToHex(buf, timeAdjustment);
  addToSendQueue('S');
  addToSendQueue(TO_HEX(NodeAddr));
  addToSendQueue(RESPONSE_TIME_ADJUSTMENT);
  addToSendQueue(buf, 8);
  addToSendQueue('\n');
}

void SendRaceMode(uint8_t NodeAddr) {

  addToSendQueue('S');
  addToSendQueue(TO_HEX(NodeAddr));
  addToSendQueue(RESPONSE_RACE_MODE);
  addToSendQueue(TO_HEX(raceMode));
  addToSendQueue('\n');

}


void SendVRxBand(uint8_t NodeAddr) {
  //Cmd Byte B
  addToSendQueue('S');
  addToSendQueue(TO_HEX(NodeAddr));
  addToSendQueue(RESPONSE_BAND);
  addToSendQueue(TO_HEX(getRXBandPilot(NodeAddr)));
  addToSendQueue('\n');
  //SendVRxFreq(NodeAddr);

}

void SendVRxChannel(uint8_t NodeAddr) {

  addToSendQueue('S');
  addToSendQueue(TO_HEX(NodeAddr));
  addToSendQueue(RESPONSE_CHANNEL);
  addToSendQueue(TO_HEX(getRXChannelPilot(NodeAddr)));
  addToSendQueue('\n');
  //SendVRxFreq(NodeAddr);

}

void SendVRxFreq(uint8_t NodeAddr) {
  //Cmd Byte F
  uint8_t index = getRXChannelPilot(NodeAddr) + (8 * getRXBandPilot(NodeAddr));
  uint16_t frequency = channelFreqTable[index];

  addToSendQueue('S');
  addToSendQueue(TO_HEX(NodeAddr));
  addToSendQueue(RESPONSE_FREQUENCY);

  uint8_t buf[4];
  intToHex(buf, frequency);
  addToSendQueue(buf, 4);
  addToSendQueue('\n');
}

void sendAPIversion() {

  for (int i = 0; i < MAX_NUM_PILOTS; i++) {
    addToSendQueue('S');
    addToSendQueue(TO_HEX(i));
    addToSendQueue(RESPONSE_API_VERSION);
    addToSendQueue('0');
    addToSendQueue('0');
    addToSendQueue('0');
    addToSendQueue('4');
    addToSendQueue('\n');
  }
}

void SendAllSettings(uint8_t NodeAddr) {
  //1BCFHIJLMRSTvy# + x

  //v Lipo Voltage
  //y Configured State, True/False
  //T Threshold Valule
  //S Enable/Disable Sound
  //R Race Mode
  //M Minimal Lap Time
  //L Lap Report, // laps?
  //J Set Timer Adjustment Value
  //I RSSI monitoring interval
  //H Setup threshold mode
  //F VRx frequqnecy
  //C VRx channel
  //B VRx band
  //1 Enum Devices
  //SendCurrRSSI(NodeAddr);

  //  WaitFirstLap(NodeAddr); //1 Wait First lap
  //  SendVRxBand(NodeAddr); //B
  //  SendVRxChannel(NodeAddr); //C
  //  SendVRxFreq(NodeAddr); //F VRx Freq
  //  SendSetThresholdMode(NodeAddr); //H send Threshold Mode
  //  SendRSSImonitorInterval(NodeAddr); //I RSSI monitor interval
  //  SendTimerCalibration(NodeAddr); //J timer calibration
  //  SendAllLaps(NodeAddr); //L Lap Report
  //  SendMinLap(NodeAddr); //M Minumum Lap Time
  //  SendRaceMode(NodeAddr); //R
  //  SendSoundMode(NodeAddr); //S
  //  SendThresholdValue(NodeAddr); // T
  //  SendLipoVoltage(); // v
  //  SendIsModuleConfigured(NodeAddr); //y
  //  sendAPIversion(); // #
  //  SendXdone(NodeAddr); //x

  SendVRxChannel(NodeAddr);
  SendRaceMode(NodeAddr);
  SendMinLap(NodeAddr);
  SendThresholdValue(NodeAddr);
  SendSoundMode(NodeAddr);
  SendVRxBand(NodeAddr);
  WaitFirstLap(NodeAddr);
  SendIsModuleConfigured();
  SendVRxFreq(NodeAddr);
  SendRSSImonitorInterval(NodeAddr);
  SendTimerCalibration(NodeAddr);
  SendAllLaps(NodeAddr);
  sendAPIversion();
  sendThresholdMode(NodeAddr);
  SendXdone(NodeAddr);
  sendPilotActive(NodeAddr);
  sendExperimentalMode(NodeAddr);

  // flush outputs after this long message
  update_outputs();

  update_outputs(); // Flush output after each node to prevent lost messages
}

// TODO: find a better way to handle this duplicate code. Add a function for every setting?
void sendAllExtendedSettings() {
  sendExtendedCommandByte('S', '*', EXTENDED_RACE_NUM, getRaceNum());
  sendExtendedCommandHalfByte('S', '*', EXTENDED_WIFI_CHANNEL, EepromSettings.WiFiChannel);
  sendExtendedCommandHalfByte('S', '*', EXTENDED_WIFI_PROTOCOL, EepromSettings.WiFiProtocol);
  sendExtendedCommandHalfByte('S', '*', EXTENDED_VOLTAGE_TYPE, (uint8_t)EepromSettings.ADCVBATmode);
  sendExtendedCommandInt('S', '*', EXTENDED_VOLTAGE_CALIB, EepromSettings.VBATcalibration * 1000);
  sendExtendedCommandInt('S', '*', EXTENDED_DISPLAY_TIMEOUT, EepromSettings.display_timeout_ms / 1000);
  sendExtendedCommandInt('S', '*', EXTENDED_FILTER_CUTOFF, EepromSettings.RXADCfilterCutoff);
  sendExtendedCommandHalfByte('S', '*', EXTENDED_NUM_MODULES, EepromSettings.NumReceivers);
  for(int i = 0; i < MAX_NUM_PILOTS; ++i) {
    if(i < getNumReceivers()) sendExtendedCommandInt('S', i, EXTENDED_CALIB_MIN, EepromSettings.RxCalibrationMin[i]);
    if(i < getNumReceivers()) sendExtendedCommandInt('S', i, EXTENDED_CALIB_MAX, EepromSettings.RxCalibrationMax[i]);
    sendExtendedCommandHalfByte('S', i, EXTENDED_MULTIPLEX_OFF, isPilotMultiplexOff(i));
  }
}


void handleExtendedCommands(uint8_t* data, uint8_t length) {
  uint8_t node_addr = TO_BYTE(data[1]);
  uint8_t control_byte = data[2];
  //set commands
  // TODO: remove length > 4 here to reduce duplicate code
  if(length > 4) {
    switch(control_byte) {
      case EXTENDED_VOLTAGE_TYPE:
        EepromSettings.ADCVBATmode = (ADCVBATmode_)TO_BYTE(data[3]);
        sendExtendedCommandHalfByte('S', '*', EXTENDED_VOLTAGE_TYPE, EepromSettings.ADCVBATmode);
        setSaveRequired();
        break;
      case EXTENDED_VOLTAGE_CALIB:
        EepromSettings.VBATcalibration = HEX_TO_UINT16(data + 3) / 1000.0;
        sendExtendedCommandInt('S', '*', EXTENDED_VOLTAGE_CALIB, EepromSettings.VBATcalibration * 1000);
        setSaveRequired();
        break;
      case EXTENDED_DISPLAY_TIMEOUT:
        EepromSettings.display_timeout_ms = HEX_TO_UINT16(data + 3) * 1000;
        sendExtendedCommandInt('S', '*', control_byte, EepromSettings.display_timeout_ms / 1000);
        setSaveRequired();
        break;
      case EXTENDED_WIFI_CHANNEL:
        EepromSettings.WiFiChannel = TO_BYTE(data[3]);
        sendExtendedCommandHalfByte('S', '*', control_byte, EepromSettings.WiFiChannel);
        setSaveRequired();
        break;
      case EXTENDED_WIFI_PROTOCOL:
        EepromSettings.WiFiProtocol = TO_BYTE(data[3]);
        sendExtendedCommandHalfByte('S', '*', control_byte, EepromSettings.WiFiProtocol);
        setSaveRequired();
        break;
      case EXTENDED_FILTER_CUTOFF:
        EepromSettings.RXADCfilterCutoff = HEX_TO_UINT16(data + 3);
        sendExtendedCommandInt('S', '*', control_byte, EepromSettings.RXADCfilterCutoff);
        setSaveRequired();
        break;
      case EXTENDED_NUM_MODULES:
        EepromSettings.NumReceivers = TO_BYTE(data[3]);
        sendExtendedCommandHalfByte('S', '*', control_byte, EepromSettings.NumReceivers);
        setSaveRequired();
        break;
      case EXTENDED_MULTIPLEX_OFF:
        setPilotMultiplexOff(node_addr, TO_BYTE(data[3]));
        sendExtendedCommandHalfByte('S', node_addr, control_byte, isPilotMultiplexOff(node_addr));
        break;
      case EXTENDED_RSSI:
        extendedRssiMonitorInterval = HEX_TO_UINT16(data + 3);
        sendExtendedCommandInt('S', '*', control_byte, extendedRssiMonitorInterval);
        break;
      case EXTENDED_DEBUG_LOG:
        set_chorus_log(TO_BYTE(data[3]));
        break;
    }

  } else {
    switch(control_byte) {
      case EXTENDED_ALL_SETTINGS:
        sendAllExtendedSettings();
        break;
      case EXTENDED_RACE_NUM:
        sendExtendedCommandByte('S', '*', EXTENDED_RACE_NUM, getRaceNum());
        break;
      case EXTENDED_CALIB_MIN:
        if(node_addr < getNumReceivers()) sendExtendedCommandInt('S', node_addr, EXTENDED_CALIB_MIN, EepromSettings.RxCalibrationMin[node_addr]);
        break;
      case EXTENDED_CALIB_MAX:
        if(node_addr < getNumReceivers()) sendExtendedCommandInt('S', node_addr, EXTENDED_CALIB_MAX, EepromSettings.RxCalibrationMax[node_addr]);
        break;
      case EXTENDED_CALIB_START:
        rssiCalibration();
        sendExtendedCommandHalfByte('S', node_addr, EXTENDED_CALIB_START, 1);
      case EXTENDED_VOLTAGE_TYPE:
        sendExtendedCommandHalfByte('S', '*', EXTENDED_VOLTAGE_TYPE, (uint8_t)EepromSettings.ADCVBATmode);
        break;
      case EXTENDED_VOLTAGE_CALIB:
        sendExtendedCommandInt('S', '*', EXTENDED_VOLTAGE_CALIB, EepromSettings.VBATcalibration * 1000);
        break;
      case EXTENDED_DISPLAY_TIMEOUT:
        sendExtendedCommandInt('S', '*', control_byte, EepromSettings.display_timeout_ms / 1000);
        break;
      case EXTENDED_EEPROM_RESET:
        EepromSettings.defaults();
        sendExtendedCommandHalfByte('S', '*', control_byte, 1);
        break;
      case EXTENDED_DEBUG_FREE_HEAP:
        sendExtendedCommandInt32('S', '*', control_byte, ESP.getFreeHeap());
        break;
      case EXTENDED_DEBUG_MAX_BLOCK_HEAP:
        sendExtendedCommandInt32('S', '*', control_byte, ESP.getMaxAllocHeap());
        break;
      case EXTENDED_DEBUG_MIN_FREE_HEAP:
        sendExtendedCommandInt32('S', '*', control_byte, ESP.getMinFreeHeap());
        break;
    }
  }
}

void handleSerialControlInput(char *controlData, uint8_t  ControlByte, uint8_t NodeAddr, uint8_t length) {
  String InString = "";
  uint8_t valueToSet;
  uint8_t NodeAddrByte = TO_BYTE(NodeAddr); // convert ASCII to real byte values

  //Serial.println(length);

  if (ControlByte == CONTROL_NUM_RECIEVERS) {
    SendNumberOfnodes();
  }

  if(ControlByte == CONTROL_PING) {
      addToSendQueue((uint8_t*)controlData, length);
      update_outputs();
  }

  // our unofficial extension commands
  if(ControlByte == EXTENDED_PREFIX) {
    // We are removing the prefix here to make handling easier and if we ever decide to use another method
    handleExtendedCommands((uint8_t*)controlData + 1, length - 1);
    return;
  }

  if (controlData[2] == CONTROL_GET_TIME) {
    //Serial.println("Sending Time.....");
    SendMillis();
  }


  if (controlData[2] == CONTROL_GET_ALL_DATA) {
    for (int i = 0; i < MAX_NUM_PILOTS; i++) {
      SendAllSettings(i);
      //delay(100);
    }
  }

  //  if (controlData[2] == RESPONSE_API_VERSION) {
  //   // for (int i = 0; i < getNumReceivers(); i++) {
  //      sendAPIversion();
  //    //}
  //  }


  ControlByte = controlData[2]; //This is dirty but we rewrite this byte....

  if (length > 4) { // set value commands  changed to n+1 ie, 3+1 = 4.
    switch (ControlByte) {

    case CONTROL_PILOT_ACTIVE:
      valueToSet = TO_BYTE(controlData[3]);
      setPilotActive(NodeAddrByte, valueToSet);
      sendPilotActive(NodeAddrByte);
      break;
    case CONTROL_EXPERIMENTAL_MODE:
      valueToSet = TO_BYTE(controlData[3]);
      use_experimental = valueToSet;
      sendExperimentalMode(NodeAddrByte);
      break;

      case CONTROL_RACE_MODE:
        valueToSet = TO_BYTE(controlData[3]);
        setRaceMode(valueToSet);
        for (int i = 0; i < MAX_NUM_PILOTS; i++) {
          SendRaceMode(i);
        }
        isConfigured = 1;
        break;

      case CONTROL_WAIT_FIRST_LAP:
        valueToSet = TO_BYTE(controlData[3]);
        setCountFirstLap(valueToSet);
        WaitFirstLap(0);
        //playClickTones();
        isConfigured = 1;
        break;

      case CONTROL_BAND:
        setPilotBand(NodeAddrByte, TO_BYTE(controlData[3]));
        SendVRxBand(NodeAddrByte);
        SendVRxFreq(NodeAddrByte);
        isConfigured = 1;
        break;

      case CONTROL_CHANNEL:
        setPilotChannel(NodeAddrByte, TO_BYTE(controlData[3]));
        SendVRxChannel(NodeAddrByte);
        SendVRxFreq(NodeAddrByte);
        isConfigured = 1;
        break;

      case CONTROL_FREQUENCY:
        //  TODO: convert to band/freq?
        isConfigured = 1;
        break;

      case CONTROL_RSSI_MON_INTERVAL:
        rssiMonitorInterval = (HEX_TO_UINT16((uint8_t*)&controlData[3]));
        isConfigured = 1;
        SendRSSImonitorInterval(NodeAddrByte);
        break;

      case CONTROL_MIN_LAP_TIME:
        valueToSet = HEX_TO_BYTE(controlData[3], controlData[4]);
        setMinLap(valueToSet);
        SendMinLap(NodeAddrByte);
        //playClickTones();

        break;

      case CONTROL_SOUND:
        //valueToSet = TO_BYTE(controlData[1]);
        //        isSoundEnabled = valueToSet;
        //        if (!isSoundEnabled) {
        //          noTone(buzzerPin);
        //        }
        //addToSendQueue(SEND_SOUND_STATE);
        //playClickTones();
        for (int i = 0; i < MAX_NUM_PILOTS; i++) {
          SendSoundMode(i);
        }
        isConfigured = 1;

        break;

      case CONTROL_THRESHOLD:
        SetThresholdValue(HEX_TO_UINT16((uint8_t*)&controlData[3]), NodeAddrByte);
        SendThresholdValue(NodeAddrByte);
        isConfigured = 1;
        break;
      case CONTROL_TIME_ADJUSTMENT:
        timeAdjustment = HEX_TO_SIGNED_LONG((uint8_t*)&controlData[3]);
        SendTimerCalibration(NodeAddrByte);

        isConfigured = 1;
        break;
      case CONTROL_THRESHOLD_SETUP: // setup threshold using sophisticated algorithm
        valueToSet = TO_BYTE(controlData[3]);
        uint8_t node = TO_BYTE(controlData[1]);
        // Skip this if we get an invalid node id
        if(node >= MAX_NUM_PILOTS) {
          break;
        }
        if (!raceMode) { // don't run threshold setup in race mode because we don't calculate slowRssi in race mode, but it's needed for setup threshold algorithm
           thresholdSetupMode[node] = valueToSet;
        }
        if (thresholdSetupMode[node]) {
          setupThreshold(RSSI_SETUP_INITIALIZE, node);
        } else {
          //playThresholdSetupStopTones();
        }
        break;
    }
  } else { // get value and other instructions
    switch (ControlByte) {
      case CONTROL_GET_TIME:
        //millisUponRequest = millis();
        //addToSendQueue(SEND_TIME);
        break;
      case CONTROL_WAIT_FIRST_LAP:
        WaitFirstLap(NodeAddrByte);
        break;
      case CONTROL_BAND:
        SendVRxBand(NodeAddrByte);
        break;
      case CONTROL_CHANNEL:
        SendVRxChannel(NodeAddrByte);
        break;
      case CONTROL_FREQUENCY:
        SendVRxFreq(NodeAddrByte);
        break;
      case CONTROL_RSSI_MON_INTERVAL:
        SendRSSImonitorInterval(NodeAddrByte);
        break;
      case CONTROL_RACE_MODE:
        SendRaceMode(NodeAddrByte);
        break;
      case CONTROL_MIN_LAP_TIME:
        SendMinLap(NodeAddrByte);
        break;
      case CONTROL_SOUND:
        for (int i = 0; i < MAX_NUM_PILOTS; i++) {
          SendSoundMode(i);
        }
        break;
      case CONTROL_THRESHOLD:
        SendThresholdValue(NodeAddrByte);
        break;
      case CONTROL_GET_RSSI: // get current RSSI value
        //Serial.println("sending current RSSI");
        for (int i = 0; i < MAX_NUM_PILOTS; i++) {
          SendCurrRSSI(i);
        }
        break;
      case CONTROL_GET_VOLTAGE: //get battery voltage
        //addToSendQueue(SEND_VOLTAGE);
        SendLipoVoltage();
        break;
      case CONTROL_GET_ALL_DATA: // request all data
        //addToSendQueue(SEND_ALL_DEVICE_STATE);
        break;
      case CONTROL_GET_API_VERSION: //get API version
        //for (int i = 0; i < getNumReceivers(); i++) {
        sendAPIversion();
        //}
        break;
      case CONTROL_TIME_ADJUSTMENT:
        SendTimerCalibration(NodeAddrByte);
        break;
      case CONTROL_THRESHOLD_SETUP: // get state of threshold setup process
        sendThresholdMode(NodeAddrByte);
        break;
      case CONTROL_GET_IS_CONFIGURED:
        SendIsModuleConfigured();
        break;
    }
  }
}

void thresholdModeStep() {
  for(uint8_t i = 0; i < MAX_NUM_PILOTS; ++i) {
    setupThreshold(RSSI_SETUP_NEXT_STEP, i);
  }
}

bool isInRaceMode() {
  return raceMode;
}

void startRace() {
  setRaceMode(1);
  for (int i = 0; i < MAX_NUM_PILOTS; i++) {
    SendRaceMode(i);
  }
}

void stopRace() {
  setRaceMode(0);
  for (int i = 0; i < MAX_NUM_PILOTS; i++) {
    SendRaceMode(i);
  }
}

bool isExperimentalModeOn() {
  return use_experimental;
}

void update_comms() {
  SendCurrRSSIloop();
  ExtendedRSSILoop();
}
