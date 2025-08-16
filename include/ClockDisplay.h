#ifndef CLOCK_DISPLAY_H
#define CLOCK_DISPLAY_H

#include <Arduino.h>
#include "DisplayManager.h"
#include "SignTextController.h"
#include "WifiTimeLib.h"

class ClockDisplay {
public:
  // Constructor
  ClockDisplay(DisplayManager* display_manager, WifiTimeLib* wifi_time_lib = nullptr);
  
  // Destructor
  ~ClockDisplay();
  
  // Initialization
  bool initialize();
  
  // Time management
  bool syncTime(int timeout_seconds = 10);
  void setTimezone(const char* ntp_server, const char* tz_info);
  
  // Display control
  void update();
  void forceUpdate();
  String getCurrentTimeString();
  
  // Configuration
  void setUpdateInterval(unsigned long interval_ms);
  void setFont(RetroText::Font font);
  void setBrightness(uint8_t time_brightness, uint8_t date_brightness);
  
  // Status
  bool isTimeValid() const;
  unsigned long getLastUpdate() const;
  
private:
  DisplayManager* display_manager_;
  WifiTimeLib* wifi_time_lib_;
  RetroText::SignTextController* clock_controller_;
  
  // Configuration
  unsigned long update_interval_;
  unsigned long last_update_time_;
  uint8_t time_brightness_;
  uint8_t date_brightness_;
  
  // Time data
  tm timeinfo_;
  time_t now_;
  bool time_valid_;
  
  // Fallback timing for sync failures
  unsigned long sync_failure_time_;
  static const unsigned long SYNC_FAILURE_DISPLAY_DURATION = 3000; // 3 seconds
  
  // Internal methods
  String formatClockDisplay();
  void setupController();
  static uint8_t clockBrightnessCallback(char c, String text, int char_pos, bool is_time_display);
  
  // Static reference for callback
  static ClockDisplay* instance_;
};

#endif // CLOCK_DISPLAY_H
