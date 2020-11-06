#ifndef _LED_CONTROLLER_H_
#define _LED_CONTROLLER_H_

#include "led_blinker.h"
#include "visual_controller.h"

class LedController : public VisualController {
 protected:
  PatternBlinker* blinker;

  virtual void set_wifi_no_ap();
  virtual void set_wifi_disconnected();
  virtual void set_wifi_connected();
  virtual void set_wifimanager();

  virtual void set_ws_disconnected();
  virtual void set_ws_authorizing();
  virtual void set_ws_connecting();
  virtual void set_ws_connected();

 public:
  LedController(int pin);

  // ValueConsumer interface for ValueConsumer<WifiState> (Networking object
  // state updates)
  virtual void set_input(WifiState new_value, uint8_t input_channel = 0) override;
  // ValueConsumer interface for ValueConsumer<WSConnectionState>
  // (WSClient object state updates)
  virtual void set_input(WSConnectionState new_value,
                 uint8_t input_channel = 0) override;
  // ValueConsumer interface for ValueConsumer<int> (delta count producer
  // updates)
  virtual void set_input(int new_value, uint8_t input_channel = 0) override;
};

#endif