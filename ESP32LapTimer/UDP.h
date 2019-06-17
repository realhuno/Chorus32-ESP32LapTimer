#pragma once

#include <esp32-hal.h>
/**
 * \return true if the item has been added to the queue
 */
bool IRAM_ATTR addToSendQueue(uint8_t item);
/**
 * \return the number of items added to the queue
 */
uint8_t IRAM_ATTR addToSendQueue(uint8_t * buff, uint8_t length);
void IRAM_ATTR SendUDPpacket();

void UDPinit();
