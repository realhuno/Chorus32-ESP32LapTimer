#define SPI_ADDRESS_SYNTH_B 0x01
#define SPI_ADDRESS_POWER   0x0A
#define SPI_ADDRESS_STATE   0x0F

#define PowerDownState    0b11111111111111111111
#define DefaultPowerState 0b00010000110000010011

#define ResetReg          0b00000000000000000000
#define StandbyReg        0b00000000000000000010
#define PowerOnReg        0b00000000000000000001

#include "RX5808.h"

#include "HardwareConfig.h"

#include <Arduino.h>
#include <stdint.h>
#include <SPI.h>
#include <driver/timer.h>

extern uint8_t NumRecievers;

static volatile uint8_t RXBandModule[MaxNumRecievers];
static volatile uint8_t RXChannelModule[MaxNumRecievers];

// TODO: this doesn't really belong here
static volatile uint8_t RXBandPilot[MaxNumRecievers];
static volatile uint8_t RXChannelPilot[MaxNumRecievers];

static uint32_t lastUpdate[MaxNumRecievers] = {0,0,0,0,0,0};

void InitSPI() {
#ifdef USE_HSPI
  SPI.begin(SCK, MISO, MOSI, -1);
#endif
#ifdef USE_VSPI
  SPI.begin();
#endif
  delay(200);
}

bool isRxReady(uint8_t module) {
	Serial.print("Micros: ");
	Serial.print(micros());
	Serial.print(" last update: ");
	Serial.println(lastUpdate[module]);
	return (micros() - lastUpdate[module]) > MIN_TUNE_TIME_US;
}

void rxWrite(uint8_t addressBits, uint32_t dataBits, uint8_t CSpin) {

  uint32_t data = addressBits | (1 << 4) | (dataBits << 5);
  SPI.beginTransaction(SPISettings(1000000, LSBFIRST, SPI_MODE0));
  digitalWrite(CSpin, LOW);
  SPI.transferBits(data, NULL, 25);

  digitalWrite(CSpin, HIGH);
  SPI.endTransaction();
}


void rxWriteAll(uint8_t addressBits, uint32_t dataBits) {

  uint32_t data = addressBits | (1 << 4) | (dataBits << 5);
  SPI.beginTransaction(SPISettings(1000000, LSBFIRST, SPI_MODE0));
  digitalWrite(CS1, LOW);
  digitalWrite(CS2, LOW);
  digitalWrite(CS3, LOW);
  digitalWrite(CS4, LOW);
  digitalWrite(CS5, LOW);
  digitalWrite(CS6, LOW);

  SPI.transferBits(data, NULL, 25);

  delayMicroseconds(MIN_TUNE_TIME_US);
  digitalWrite(CS1, HIGH);
  digitalWrite(CS2, HIGH);
  digitalWrite(CS3, HIGH);
  digitalWrite(CS4, HIGH);
  digitalWrite(CS5, HIGH);
  digitalWrite(CS6, HIGH);
  SPI.endTransaction();

}

void RXstandBy(byte NodeAddr) {

  switch (NodeAddr) {

    case 0:
      rxWrite(SPI_ADDRESS_STATE, StandbyReg, CS1);
      break;

    case 1:
      rxWrite(SPI_ADDRESS_STATE, StandbyReg, CS2);
      break;

    case 2:
      rxWrite(SPI_ADDRESS_STATE, StandbyReg, CS3);
      break;

    case 3:
      rxWrite(SPI_ADDRESS_STATE, StandbyReg, CS4);
      break;

    case 4:
      rxWrite(SPI_ADDRESS_STATE, StandbyReg, CS5);
      break;

    case 5:
      rxWrite(SPI_ADDRESS_STATE, StandbyReg, CS6);
      break;
  }
}

void RXpowerOn(byte NodeAddr) {

  switch (NodeAddr) {

    case 0:
      rxWrite(SPI_ADDRESS_STATE, PowerOnReg, CS1);
      break;

    case 1:
      rxWrite(SPI_ADDRESS_STATE, PowerOnReg, CS2);
      break;

    case 2:
      rxWrite(SPI_ADDRESS_STATE, PowerOnReg, CS3);
      break;

    case 3:
      rxWrite(SPI_ADDRESS_STATE, PowerOnReg, CS4);
      break;

    case 4:
      rxWrite(SPI_ADDRESS_STATE, PowerOnReg, CS5);
      break;

    case 5:
      rxWrite(SPI_ADDRESS_STATE, PowerOnReg, CS6);
      break;
  }
}

void RXreset(byte NodeAddr) {

  switch (NodeAddr) {

    case 0:
      rxWrite(SPI_ADDRESS_STATE, ResetReg, CS1);
      break;

    case 1:
      rxWrite(SPI_ADDRESS_STATE, ResetReg, CS2);
      break;

    case 2:
      rxWrite(SPI_ADDRESS_STATE, ResetReg, CS3);
      break;

    case 3:
      rxWrite(SPI_ADDRESS_STATE, ResetReg, CS4);
      break;

    case 4:
      rxWrite(SPI_ADDRESS_STATE, ResetReg, CS5);
      break;

    case 5:
      rxWrite(SPI_ADDRESS_STATE, ResetReg, CS6);
      break;
  }
}


void PowerDownAll() {
  //for (int i = 0; i < NumRecievers; i++) {
  //rxWrite(SPI_ADDRESS_POWER, PowerDownState, i);
  //RXstandBy(i);
  //delay(100);
  //}
  rxWriteAll(SPI_ADDRESS_POWER, PowerDownState);
}

void PowerDown(byte NodeAddr) {

  switch (NodeAddr) {
    case 0:
      rxWrite(SPI_ADDRESS_POWER, PowerDownState, CS1);
      break;

    case 1:
      rxWrite(SPI_ADDRESS_POWER, PowerDownState, CS2);
      break;

    case 2:
      rxWrite(SPI_ADDRESS_POWER, PowerDownState, CS3);
      break;

    case 3:
      rxWrite(SPI_ADDRESS_POWER, PowerDownState, CS4);
      break;

    case 4:
      rxWrite(SPI_ADDRESS_POWER, PowerDownState, CS5);
      break;

    case 5:
      rxWrite(SPI_ADDRESS_POWER, PowerDownState, CS6);
      break;
  }
}

void PowerUpAll() {
  for (int i = 0; i < NumRecievers; i++) {
    rxWrite(SPI_ADDRESS_POWER, DefaultPowerState, i);
  }
}

void PowerUp(byte NodeAddr) {
  switch (NodeAddr) {
    case 0:
      rxWrite(SPI_ADDRESS_POWER, DefaultPowerState, CS1);
      break;

    case 1:
      rxWrite(SPI_ADDRESS_POWER, DefaultPowerState, CS2);
      break;

    case 2:
      rxWrite(SPI_ADDRESS_POWER, DefaultPowerState, CS3);
      break;

    case 3:
      rxWrite(SPI_ADDRESS_POWER, DefaultPowerState, CS4);
      break;

    case 4:
      rxWrite(SPI_ADDRESS_POWER, DefaultPowerState, CS5);
      break;

    case 5:
      rxWrite(SPI_ADDRESS_POWER, DefaultPowerState, CS6);
      break;
  }
}

void SelectivePowerUp() { //powerup only the RXs that have been requested
  for (int i = 0; i < NumRecievers; i++) {
    RXreset(i);
    //RXstandBy(i);
    delay(50);
    RXpowerOn(i);
    //PowerUp(i);
    rxWrite(SPI_ADDRESS_POWER, DefaultPowerState, i);
//
//    Serial.print("Power up: ");
//    Serial.println(i);
  }
}

uint16_t getSynthRegisterBFreq(uint16_t f) {
  return ((((f - 479) / 2) / 32) << 7) | (((f - 479) / 2) % 32);
}


void setChannel(uint8_t channel, uint8_t NodeAddr) {
  if (channel <= 7) {
    RXChannelModule[NodeAddr] = channel;
    uint8_t band = RXBandModule[NodeAddr];
    uint16_t SetFreq = setModuleChannelBand(channel, band, NodeAddr);
    (void)SetFreq;
  }
}

void setBand(uint8_t band, uint8_t NodeAddr) {
  if (band <= MAX_BAND) {
    RXBandModule[NodeAddr] = band;
    uint8_t channel = RXChannelModule[NodeAddr];
    uint16_t SetFreq = setModuleChannelBand(channel, band, NodeAddr);
    (void)SetFreq;
  }
}

uint16_t setModuleChannelBand(uint8_t NodeAddr) {
	return setModuleChannelBand(RXChannelModule[NodeAddr], RXBandModule[NodeAddr], NodeAddr);
}

uint16_t setModuleChannelBand(uint8_t channel, uint8_t band, uint8_t NodeAddr) {  
  uint8_t index = channel + (8 * band);
  uint16_t frequency = channelFreqTable[index];
  RXBandModule[NodeAddr] = band;
  RXChannelModule[NodeAddr] = channel;
  lastUpdate[NodeAddr] = micros();
  switch (NodeAddr) {
    case 0:
      rxWrite(SPI_ADDRESS_SYNTH_B, getSynthRegisterBFreq(frequency), CS1);
      break;

    case 1:
      rxWrite(SPI_ADDRESS_SYNTH_B, getSynthRegisterBFreq(frequency), CS2);
      break;

    case 2:
      rxWrite(SPI_ADDRESS_SYNTH_B, getSynthRegisterBFreq(frequency), CS3);
      break;

    case 3:
      rxWrite(SPI_ADDRESS_SYNTH_B, getSynthRegisterBFreq(frequency), CS4);
      break;

    case 4:
      rxWrite(SPI_ADDRESS_SYNTH_B, getSynthRegisterBFreq(frequency), CS5);
      break;

    case 5:
      rxWrite(SPI_ADDRESS_SYNTH_B, getSynthRegisterBFreq(frequency), CS6);
      break;
  }
  return frequency;
}

uint16_t setModuleFrequency(uint16_t frequency, uint8_t NodeAddr) {

  switch (NodeAddr) {
    case 0:
      rxWrite(SPI_ADDRESS_SYNTH_B, getSynthRegisterBFreq(frequency), CS1);
      break;

    case 1:
      rxWrite(SPI_ADDRESS_SYNTH_B, getSynthRegisterBFreq(frequency), CS2);
      break;

    case 2:
      rxWrite(SPI_ADDRESS_SYNTH_B, getSynthRegisterBFreq(frequency), CS3);
      break;

    case 3:
      rxWrite(SPI_ADDRESS_SYNTH_B, getSynthRegisterBFreq(frequency), CS4);
      break;

    case 4:
      rxWrite(SPI_ADDRESS_SYNTH_B, getSynthRegisterBFreq(frequency), CS5);
      break;

    case 5:
      rxWrite(SPI_ADDRESS_SYNTH_B, getSynthRegisterBFreq(frequency), CS6);
      break;
  }
  return frequency;
}

uint16_t setModuleFrequencyAll(uint16_t frequency) {

  rxWriteAll(SPI_ADDRESS_SYNTH_B, getSynthRegisterBFreq(frequency));

  return frequency;
}

String getBandLabel(int band) {

  switch (band) {
    case 0:
      return "R";
      break;
    case 1:
      return "A";
      break;
    case 2:
      return "B";
      break;
    case 3:
      return "E";
      break;
    case 4:
      return "F";
      break;
    case 5:
      return "D";
      break;
    case 6:
      return "X";
      break;
    case 7:
      return "XX";
      break;
    default:
      return "";
      break;
  }

}

void setRXBandPilot(uint8_t pilot, uint8_t band) {
	RXBandPilot[pilot] = band;
}
uint8_t getRXBandPilot(uint8_t pilot) {
	return RXBandPilot[pilot];
}

void setRXChannelPilot(uint8_t pilot, uint8_t channel) {
	RXChannelPilot[pilot] = channel;
}
uint8_t getRXChannelPilot(uint8_t pilot) {
	return RXChannelPilot[pilot];
}

void setRXBandModule(uint8_t module, uint8_t band) {
	RXBandModule[module] = band;
}
uint8_t getRXBandModule(uint8_t module) {
	return RXBandModule[module];
}

void setRXChannelModule(uint8_t module, uint8_t channel) {
	RXChannelModule[module] = channel;
}
uint8_t getRXChannelModule(uint8_t module) {
	return RXChannelModule[module];
}
