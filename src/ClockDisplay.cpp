#include "ClockDisplay.h"

// Static instance pointer for callback
ClockDisplay* ClockDisplay::instance_ = nullptr;

ClockDisplay::ClockDisplay(DisplayManager* display_manager, WifiTimeLib* wifi_time_lib)
  : display_manager_(display_manager)
  , wifi_time_lib_(wifi_time_lib)
  , clock_controller_(nullptr)
  , update_interval_(1000)  // Update every second
  , last_update_time_(0)
  , time_brightness_(150)   // Bright for time
  , date_brightness_(20)    // Dim for date
  , time_valid_(false)
  , sync_failure_time_(0)
{
  instance_ = this;  // Set static instance for callback
}

ClockDisplay::~ClockDisplay() {
  if (clock_controller_) {
    delete clock_controller_;
  }
  if (instance_ == this) {
    instance_ = nullptr;
  }
}

bool ClockDisplay::initialize() {
  if (!display_manager_) {
    Serial.println("ClockDisplay: DisplayManager is null");
    return false;
  }
  
  setupController();
  
  Serial.println("ClockDisplay initialized");
  return true;
}

bool ClockDisplay::syncTime(int timeout_seconds) {
  if (!wifi_time_lib_) {
    Serial.println("ClockDisplay: No WiFi time library available");
    return false;
  }
  
  Serial.println("ClockDisplay: Syncing time...");
  bool success = wifi_time_lib_->getNTPtime(timeout_seconds, nullptr);
  
  if (success) {
    time_valid_ = true;
    sync_failure_time_ = 0; // Clear any previous failure time
    Serial.println("ClockDisplay: Time sync successful");
  } else {
    sync_failure_time_ = millis(); // Record when sync failed
    Serial.println("ClockDisplay: Time sync failed");
  }
  
  return success;
}

void ClockDisplay::setTimezone(const char* ntp_server, const char* tz_info) {
  if (wifi_time_lib_) {
    // Note: WifiTimeLib constructor takes these parameters
    // You might need to modify WifiTimeLib to allow runtime changes
    Serial.printf("ClockDisplay: Timezone set to %s (NTP: %s)\n", tz_info, ntp_server);
  }
}

void ClockDisplay::update() {
  if (millis() - last_update_time_ >= update_interval_) {
    forceUpdate();
  }
}

void ClockDisplay::forceUpdate() {
  if (!clock_controller_) return;
  
  String time_string = getCurrentTimeString();
  
  clock_controller_->setMessage(time_string);
  clock_controller_->reset();
  clock_controller_->update();
  
  last_update_time_ = millis();
}

String ClockDisplay::getCurrentTimeString() {
  if (!time_valid_) {
    // If sync failed and we're still within the display duration, show "Time not synced"
    if (sync_failure_time_ > 0 && (millis() - sync_failure_time_) < SYNC_FAILURE_DISPLAY_DURATION) {
      return "Time not synced";
    }
    // After the display duration, show the system time anyway (even if it might be wrong)
    return formatClockDisplay();
  }
  
  return formatClockDisplay();
}

void ClockDisplay::setUpdateInterval(unsigned long interval_ms) {
  update_interval_ = interval_ms;
}

void ClockDisplay::setFont(RetroText::Font font) {
  if (clock_controller_) {
    clock_controller_->setFont(font);
  }
}

void ClockDisplay::setBrightness(uint8_t time_brightness, uint8_t date_brightness) {
  time_brightness_ = time_brightness;
  date_brightness_ = date_brightness;
}

bool ClockDisplay::isTimeValid() const {
  return time_valid_;
}

unsigned long ClockDisplay::getLastUpdate() const {
  return last_update_time_;
}

String ClockDisplay::formatClockDisplay() {
  time(&now_);
  localtime_r(&now_, &timeinfo_);
  
  // Format as "Aug 12 Th 12:43:25" (exactly 18 characters)
  char formatted[19];
  char month_name[4];
  char day_name[3];
  
  // Get month abbreviation (3 chars)
  strftime(month_name, sizeof(month_name), "%b", &timeinfo_);
  
  // Get day abbreviation (2 chars)
  strftime(day_name, sizeof(day_name), "%a", &timeinfo_);
  day_name[2] = '\0'; // Ensure only 2 chars
  
  snprintf(formatted, sizeof(formatted), 
           "%s %2d %s %02d:%02d:%02d",
           month_name,  // Month (3 chars)
           timeinfo_.tm_mday,  // Day (2 chars)
           day_name,  // Day name (2 chars)
           timeinfo_.tm_hour,  // Hour (2 chars)
           timeinfo_.tm_min,   // Minute (2 chars) 
           timeinfo_.tm_sec);  // Second (2 chars)
  
  return String(formatted);
}

void ClockDisplay::setupController() {
  clock_controller_ = new RetroText::SignTextController(
    display_manager_->getMaxCharacters(), 
    display_manager_->getCharacterWidth()
  );
  
  clock_controller_->setFont(RetroText::MODERN_FONT);
  clock_controller_->setScrollStyle(RetroText::STATIC);
  clock_controller_->setScrollSpeed(1000);  // Not used for static display
  clock_controller_->setBrightness(RetroText::NORMAL);
  
  // Set up callbacks to use DisplayManager
  clock_controller_->setRenderCallback([this](uint8_t character, int pixel_offset, uint8_t brightness, bool use_alt_font) {
    // Get character pattern
    uint8_t pattern[6];
    for (int row = 0; row < 6; row++) {
      pattern[row] = display_manager_->getCharacterPattern(character, row, use_alt_font);
    }
    
    // Draw character using DisplayManager
    display_manager_->drawCharacter(pattern, pixel_offset, brightness);
  });
  
  clock_controller_->setClearCallback([this]() {
    display_manager_->clearBuffer();
  });
  
  clock_controller_->setDrawCallback([this]() {
    display_manager_->updateDisplay();
  });
  
  clock_controller_->setBrightnessCallback(clockBrightnessCallback);
}

uint8_t ClockDisplay::clockBrightnessCallback(char c, String text, int char_pos, bool is_time_display) {
  if (!instance_) return 70; // Default brightness
  
  // Time display: bright for time (last 8 characters), dim for date
  int time_start_pos = text.length() - 8;  // Time starts at position for "12:43:25"
  if (char_pos >= time_start_pos) {
    return instance_->time_brightness_;  // Time portion is bright
  }
  return instance_->date_brightness_;  // Date portion is dim
}
