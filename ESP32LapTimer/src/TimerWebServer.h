#pragma once

#include <stdint.h>
#include <stddef.h>

void InitWifiAP();
void InitWebServer();

void updateWifi();

bool isAirplaneModeOn();
void toggleAirplaneMode();
void airplaneModeOff();
void airplaneModeOn();
bool isUpdating();
void send_websocket(void* output, uint8_t* data, size_t len);
void read_websocket(void* output);
