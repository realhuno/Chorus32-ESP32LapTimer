#if BOARD==BOARD_DEFAULT
  #include "config_default.h"
#elif BOARD==BOARD_OLD
  #include "config_old.h"
#elif BOARD==BOARD_TTGO_LORA
  #include "config_ttgo_lora_v1.h"
#elif BOARD==BOARD_DEV_BOARD
  #include "config_dev_board.h"
#elif BOARD==BOARD_WROOM
  #include "config_wroom.h"
#else 
  #error "Unknown board!"
#endif

#if defined(BUTTON1) && defined(BUTTON2)
#define USE_BUTTONS
#endif

// Define unconfigured pins
#ifndef VRX_SCK
#define VRX_SCK -1
#endif
#ifndef VRX_MOSI
#define VRX_MOSI -1
#endif
#ifndef VRX_MISO
#define VRX_MISO -1
#endif

#ifndef I2C_SDA
#define I2C_SDA SDA
#endif

#ifndef I2C_SCL
#define I2C_SCL SCL
#endif

#if !defined(WIFI_MODE_CLIENT) && !defined(WIFI_MODE_ACCESSPOINT)
  #error "No WIFI_MODE selected"
#endif
