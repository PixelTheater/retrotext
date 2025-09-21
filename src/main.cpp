#include <Arduino.h>
#include <Wire.h>
#include <fonts/retro_font4x6.h>
#include <fonts/modern_font4x6.h>  // Include the converted alternative font
#include <sstream> // used for parsing and building strings
#include <iostream>
#include <string>
#include "IS31FL373x.h"
#include "WifiTimeLib.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include "SignTextController.h"
#include "messages.h"
#include "DisplayManager.h"
#include "ClockDisplay.h"
#include "MeteorAnimation.h"

// Alternative font system
#define FONT_WIDTH 4
#define FONT_HEIGHT 6  // Back to 6 rows with adaptive character shifting

// Mode system - four modes: AltFont, BasicFont, Clock, Animation
enum DisplayMode {
  MODE_ALT_FONT = 0,    // Alternative font, smooth scrolling
  MODE_MIN_FONT = 1,    // Basic font, non-smooth scrolling  
  MODE_CLOCK = 2,       // Clock display with time/date
  MODE_ANIMATION = 3    // Meteor animation with parallax stars
};
const char* mode_names[] = {"AltFont", "BasicFont", "Clock", "Animation"};

DisplayMode current_mode = MODE_ALT_FONT;
bool demo_mode_enabled = true;  // Start in demo mode
bool user_mode_enabled = false; // User-controlled mode
unsigned long last_mode_change = 0;  // Track when mode was last changed
int demo_loop_count = 0;  // Track how many demo loops completed




// Timezone config
/* 
  Enter your time zone (https://remotemonitoringsystems.ca/time-zone-abbreviations.php)
  See https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv for Timezone codes for your region
  based on https://github.com/SensorsIot/NTP-time-for-ESP8266-and-ESP32/blob/master/NTP_Example/NTP_Example.ino
*/
const char* NTP_SERVER = "ch.pool.ntp.org";
const char* TZ_INFO    = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";  // Switzerland

// WiFi and time management
WifiTimeLib wifiTimeLib(NTP_SERVER, TZ_INFO);

// Global module instances
DisplayManager* display_manager = nullptr;
ClockDisplay* clock_display = nullptr;
MeteorAnimation* meteor_animation = nullptr;

// Forward declarations
void configModeCallback(WiFiManager *myWiFiManager);
bool check_button_press();
void smooth_scroll_story();
void auto_switch_mode();
void select_random_message();
// Legacy function declarations removed
void update_current_module();

// Time 
tm timeinfo;
time_t now;
int hour = 0;
int minute = 0;
int second = 0;

// Time, date, and tracking state
int t = 0;
int number = 0;
int animation=0;
String formattedDate;
String dayStamp;
long millis_offset=0;
int last_hour=0;


#define USER_BUTTON 0

// Display configuration - will be managed by DisplayManager
#define NUM_BOARDS 3
#define WIDTH 24
#define HEIGHT 6

#define DRIVER_DEFAULT_BRIGHTNESS 90
#define TEXT_BRIGHT 190        // For time, capitalized words
#define TEXT_DEFAULT_BRIGHTNESS 90  // For normal text
#define TEXT_DIM 30            // For regular lowercase text
#define TEXT_VERY_DIM 12       // For background elements
#define DEMO_MODE_INTERVAL 30000  // 30 seconds between auto mode changes

// IS31FL373x driver - no namespace needed


// ---------------------------------------------------------------------------------------------

// I2C scan moved to DisplayManager::scanI2C()



// I2C functions moved to DisplayManager - using static methods there


// LED drivers will be managed by DisplayManager instance

// LED driver verification moved to DisplayManager::verifyDrivers()


// Legacy LED number calculation - replaced by DisplayManager::mapCoordinateToLED

// Legacy buffer functions - replaced by DisplayManager methods:
// set_led -> DisplayManager::setPixel
// get_led -> DisplayManager::getPixel  
// draw_buffer -> DisplayManager::updateDisplay
// clear_buffer -> DisplayManager::clearBuffer
// dim_buffer -> DisplayManager::dimBuffer


// Character pattern functions moved to DisplayManager::getCharacterPattern

// shift_in_character removed - functionality replaced by SignTextController

// Legacy character drawing functions removed - now handled by SignTextController and DisplayManager

// Check if a word starting at position is all capitals
bool is_word_capitalized(String text, int start_pos) {
  int pos = start_pos;
  bool has_letters = false;
  
  // Check characters until space or end of string
  while (pos < text.length() && text.charAt(pos) != ' ') {
    char c = text.charAt(pos);
    if (c >= 'A' && c <= 'Z') {
      has_letters = true;  // Found at least one capital letter
    } else if (c >= 'a' && c <= 'z') {
      return false;  // Found lowercase letter, word is not all caps
    }
    pos++;
  }
  
  return has_letters;  // Word is capitalized if it has letters and no lowercase
}

// Determine brightness level based on character and context
uint8_t get_character_brightness(char c, String text, int char_pos, bool is_time_display = false) {
  if (is_time_display) {
    // Time display: bright for time (last 8 characters), dim for date
    int time_start_pos = text.length() - 8;  // Time starts at position for "12:43:25"
    if (char_pos >= time_start_pos) {
      return TEXT_BRIGHT;  // Time portion is bright
    }
    return TEXT_DIM;  // Date portion is dim
  }
  
  // Find the start of the current word
  int word_start = char_pos;
  while (word_start > 0 && text.charAt(word_start - 1) != ' ') {
    word_start--;
  }
  
  // Check if the entire word is capitalized
  if (is_word_capitalized(text, word_start)) {
    return TEXT_BRIGHT;  // Entire capitalized word is bright
  }
  
  // Regular character brightness
  if (c >= '0' && c <= '9') return TEXT_DEFAULT_BRIGHTNESS; // Numbers normal
  return TEXT_DIM;                                     // Everything else dim
}

// write_character_at_offset removed - functionality replaced by SignTextController

// Legacy scrolling functions removed - functionality replaced by SignTextController

// Meteor animation moved to MeteorAnimation class



// Message tracking variables
int current_message_index = 0;
String current_message = "";  // Will be initialized in setup

// Module announcement messages
const String MODULE_ANNOUNCEMENTS[] = {
  "Modern Font",      // MODE_ALT_FONT
  "Retro Font",       // MODE_MIN_FONT  
  "Clock Display",    // MODE_CLOCK
  "Meteor Animation"  // MODE_ANIMATION
};

// Module state tracking
bool current_module_announced = false;
bool current_module_complete = false;

// Message completion tracking for demo mode
bool message_complete = false;

// Global SignTextController instances for demonstration
RetroText::SignTextController* modern_sign = nullptr;
RetroText::SignTextController* retro_sign = nullptr;

// Callback functions for SignTextController integration
void render_character_callback(uint8_t character, int pixel_offset, uint8_t brightness, bool use_alt_font) {
  if (display_manager) {
    // Get character pattern
    uint8_t pattern[6];
    for (int row = 0; row < 6; row++) {
      pattern[row] = display_manager->getCharacterPattern(character, row, use_alt_font);
    }
    // Draw character using DisplayManager
    display_manager->drawCharacter(pattern, pixel_offset, brightness);
  }
}

void clear_display_callback() {
  if (display_manager) {
    display_manager->clearBuffer();
  }
}

void draw_display_callback() {
  if (display_manager) {
    display_manager->updateDisplay();
  }
}

uint8_t brightness_callback(char c, String text, int char_pos, bool is_time_display) {
  return get_character_brightness(c, text, char_pos, is_time_display);
}

// Initialize the SignTextController instances
void init_sign_controllers() {
  if (!display_manager) {
    Serial.println("Error: DisplayManager not initialized");
    return;
  }
  
  // Create modern font controller
  int max_chars = display_manager->getMaxCharacters();
  int char_width = display_manager->getCharacterWidth();
  Serial.printf("Creating modern sign: %d chars, %d pixels per char\n", max_chars, char_width);
  
  modern_sign = new RetroText::SignTextController(max_chars, char_width);
  modern_sign->setFont(RetroText::MODERN_FONT);
  modern_sign->setScrollStyle(RetroText::SMOOTH);  // Use smooth scrolling
  modern_sign->setScrollSpeed(40);  // Use slower speed like retro
  modern_sign->setCharacterSpacing(1);  // Add 1-pixel spacing for better readability
  modern_sign->setBrightness(TEXT_DEFAULT_BRIGHTNESS);
  
  // Use callbacks instead of DisplayManager directly
  modern_sign->setRenderCallback(render_character_callback);
  modern_sign->setClearCallback(clear_display_callback);
  modern_sign->setDrawCallback(draw_display_callback);
  modern_sign->setBrightnessCallback(brightness_callback);
  
  Serial.printf("Modern sign setup complete with callbacks\n");
  
  // Create retro font controller
  Serial.printf("Creating retro sign: %d chars, %d pixels per char\n", max_chars, char_width);
  
  retro_sign = new RetroText::SignTextController(max_chars, char_width);
  retro_sign->setFont(RetroText::ARDUBOY_FONT);
  retro_sign->setScrollStyle(RetroText::CHARACTER);
  retro_sign->setScrollSpeed(130);
  retro_sign->setBrightness(TEXT_DEFAULT_BRIGHTNESS);
  retro_sign->setRenderCallback(render_character_callback);
  retro_sign->setClearCallback(clear_display_callback);
  retro_sign->setDrawCallback(draw_display_callback);
  retro_sign->setBrightnessCallback(brightness_callback);
  
  Serial.println("SignTextController instances initialized");
}

// Helper function to display a static message using SignTextController
void display_static_message(String message, bool use_modern_font = true, int display_time_ms = 2000) {
  RetroText::SignTextController* sign = use_modern_font ? modern_sign : retro_sign;
  
  // Save the original scroll style
  RetroText::ScrollStyle original_style = sign->getScrollStyle();
  
  // Temporarily set to static for announcement
  sign->setScrollStyle(RetroText::STATIC);
  sign->setMessage(message);
  sign->reset();
  sign->update();
  
  if (display_time_ms > 0) {
    delay(display_time_ms);
  }
  
  // Restore the original scroll style
  sign->setScrollStyle(original_style);
}

// Helper function to display a scrolling message using SignTextController
void display_scrolling_message(String message, bool use_modern_font = true, bool smooth_scroll = true) {
  RetroText::SignTextController* sign = use_modern_font ? modern_sign : retro_sign;
  
  sign->setScrollStyle(smooth_scroll ? RetroText::SMOOTH : RetroText::CHARACTER);
  sign->setMessage(message);
  sign->reset();
  
  // Run until complete
  while (!sign->isComplete()) {
    sign->update();
    delay(10);
  }
}



// Function to select a random message
void select_random_message() {
  current_message = getRandomMessage(current_message_index);
  Serial.printf("Selected message %d: %s\n", current_message_index, current_message.substring(0, 50).c_str());
}

// Show brief module announcement
void show_module_announcement(DisplayMode mode) {
  if (mode >= 0 && mode < 4) {
    String announcement = MODULE_ANNOUNCEMENTS[mode];
    Serial.printf("Announcing module: %s\n", announcement.c_str());
    
    // Use modern font for all announcements, brief display
    display_static_message(announcement, true, 1000);  // 1 second display
  }
}

// Update the current module
void update_current_module() {
  // Show announcement if needed
  bool should_announce = user_mode_enabled || (demo_mode_enabled && demo_loop_count == 0);
  if (!current_module_announced && should_announce) {
    show_module_announcement(current_mode);
    current_module_announced = true;
    Serial.printf("Announced module: %s\n", MODULE_ANNOUNCEMENTS[current_mode].c_str());
  }
  
  // Run the module
  switch (current_mode) {
    case MODE_ALT_FONT:
      // Modern font - smooth scrolling text
      smooth_scroll_story();
      // Check if message scrolling is complete
      if (!current_module_complete) {
        static unsigned long modern_start_time = 0;
        if (modern_start_time == 0) {
          modern_start_time = millis();
          Serial.println("Starting Modern Font module timing");
        }
        // Consider complete after message has had time to scroll
        unsigned long elapsed = millis() - modern_start_time;
        unsigned long required_time = current_message.length() * 200; // Estimate scroll time
        if (elapsed > required_time) {
          current_module_complete = true;
          message_complete = true;
          modern_start_time = 0;
          Serial.printf("Modern Font complete after %lu ms\n", elapsed);
        }
      }
      break;
      
    case MODE_MIN_FONT:
      // Retro font - character scrolling text using SignTextController
      // Note: This is handled by the retro_sign controller which is updated in smooth_scroll_story
      // For now, we'll use the same controller approach but with retro font settings
      smooth_scroll_story(); // This handles both modern and retro fonts based on controller setup
      // Check if message scrolling is complete
      if (!current_module_complete) {
        static unsigned long retro_start_time = 0;
        if (retro_start_time == 0) {
          retro_start_time = millis();
          Serial.println("Starting Retro Font module timing");
        }
        // Consider complete after message has had time to scroll
        unsigned long elapsed = millis() - retro_start_time;
        unsigned long required_time = current_message.length() * 180; // Estimate scroll time
        if (elapsed > required_time) {
          current_module_complete = true;
          message_complete = true;
          retro_start_time = 0;
          Serial.printf("Retro Font complete after %lu ms\n", elapsed);
        }
      }
      break;
      
    case MODE_CLOCK:
      // Clock display - continuous updates using ClockDisplay module
      if (clock_display) {
        clock_display->update();
      }
      current_module_complete = true;  // Non-text modules are always "complete"
      break;
      
    case MODE_ANIMATION:
      // Meteor animation - continuous updates using MeteorAnimation module
      if (meteor_animation) {
        meteor_animation->update();
      }
      current_module_complete = true;  // Non-text modules are always "complete"
      break;
  }
}



// font_test removed - functionality replaced by SignTextController

// Smooth scrolling story text using SignTextController - adapts to current mode
void smooth_scroll_story() {
  // Use the appropriate controller based on current mode
  RetroText::SignTextController* active_sign = nullptr;
  
  if (current_mode == MODE_ALT_FONT) {
    active_sign = modern_sign;
  } else if (current_mode == MODE_MIN_FONT) {
    active_sign = retro_sign;
  }
  
  if (!active_sign) return;
  
  static String last_message = "";
  
  // Update message if it changed
  if (last_message != current_message) {
    Serial.printf("Setting message: '%s...' (length: %d)\n", 
                  current_message.substring(0, 30).c_str(), 
                  current_message.length());
    active_sign->setMessage(current_message);
    active_sign->reset();
    last_message = current_message;
  }
  
  // Call update every loop - let the controller handle its own timing
  active_sign->update();
    
    // Reset when complete for continuous scrolling
  if (active_sign->isComplete()) {
    active_sign->reset();
  }
}

// font_test_2 removed - functionality replaced by SignTextController


// xy_test and bouncy_ball removed - unused legacy functions

// Clock formatting moved to ClockDisplay::formatClockDisplay and ClockDisplay::update

// WiFi AP mode callback - displays configuration message
void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Entered WiFi config mode");
  Serial.println("AP SSID: " + myWiFiManager->getConfigPortalSSID());
  Serial.println("AP IP: " + WiFi.softAPIP().toString());
  
  // Display AP configuration message with smooth scrolling
  String ap_message = "config wifi via AP";
  display_scrolling_message(ap_message, true, true); // Use modern font with smooth scrolling
}

// Mode switching functions
void switch_mode() {
  current_mode = (DisplayMode)((current_mode + 1) % 4);
  demo_mode_enabled = false;  // Disable demo mode when user manually switches
  user_mode_enabled = true;   // Enable user-controlled mode
  last_mode_change = millis();  // Reset timer
  
  // Reset module state
  current_module_announced = false;
  current_module_complete = false;
  message_complete = false;
  
  // Select random message when switching to text modes
  if (current_mode == MODE_ALT_FONT || current_mode == MODE_MIN_FONT) {
    select_random_message();
  }
  
  Serial.printf("User switched to %s mode\n", mode_names[current_mode]);
}

// Auto-switching function that doesn't disable demo mode
void auto_switch_mode() {
  current_mode = (DisplayMode)((current_mode + 1) % 4);
  last_mode_change = millis();  // Reset timer
  
  // Reset module state
  current_module_announced = false;
  current_module_complete = false;
  message_complete = false;
  
  // If we completed a full cycle, increment demo loop count
  if (current_mode == MODE_ALT_FONT) {
    demo_loop_count++;
    Serial.printf("Demo loop %d completed\n", demo_loop_count);
  }
  
  // Select random message when switching to text modes
  if (current_mode == MODE_ALT_FONT || current_mode == MODE_MIN_FONT) {
    select_random_message();
  }
  
  Serial.printf("Auto-switched to %s mode (demo loop %d)\n", mode_names[current_mode], demo_loop_count);
}

// Display font name with button interrupt capability
void display_font_name_interruptible(String font_name, bool use_alt_font) {
  // Use SignTextController for static display
  RetroText::SignTextController* sign = use_alt_font ? modern_sign : retro_sign;
  sign->setScrollStyle(RetroText::STATIC);
  sign->setMessage(font_name);
  sign->reset();
  sign->update();
  
  // Wait up to 1.5 seconds, but check for button press every 50ms
  for (int i = 0; i < 30; i++) {  // 30 * 50ms = 1500ms
    if (check_button_press()) {
      return;  // Exit early if button pressed
    }
    delay(50);
  }
}


// Button press detection using state-based approach (like working example)
bool check_button_press() {
  static unsigned long buttonPressTime = 0;
  static unsigned long last_successful_press = 0;
  const unsigned long min_press_interval = 500;  // Minimum time between presses
  
  // Handle button input (using proven state-based approach)
  if (digitalRead(USER_BUTTON) == LOW) {
    // Button is currently pressed
    if (buttonPressTime == 0) {
      buttonPressTime = millis(); // Mark the time button was first pressed
      Serial.println("*** BUTTON PRESS DETECTED! - Display paused ***");
      
      // Clear display to show immediate response
      if (display_manager) {
        display_manager->clearBuffer();
        display_manager->updateDisplay();
      }
    }
    // Button is being held - we don't do anything until release
  } else {
    // Button is not pressed (HIGH due to pull-up)
    if (buttonPressTime > 0) {
      // Button was just released
      unsigned long press_duration = millis() - buttonPressTime;
      
      // Check if enough time has passed since last successful press
      if (millis() - last_successful_press > min_press_interval) {
        Serial.printf("Button released after %lu ms - switching mode\n", press_duration);
        last_successful_press = millis();
        buttonPressTime = 0;
        return true;
      } else {
        Serial.println("Button press ignored - too soon after last press");
      }
      
      buttonPressTime = 0; // Reset regardless
    }
  }
  
  return false;
}


void setup() {
  // Initialize serial and I2C.
  Wire.begin();
  Wire.setClock(800000); // use 800 kHz I2C

  Serial.begin(115200);
  delay(1000);

  Serial.println("Retrotext Starting");
  Serial.println("Press USER_BUTTON to switch modes (MinFont/AltFont/Clock)");
  
  // Initialize display manager first
  Serial.println("Initializing DisplayManager...");
  display_manager = new DisplayManager(NUM_BOARDS, WIDTH, HEIGHT);
  if (!display_manager->initialize()) {
    Serial.println("FATAL: DisplayManager initialization failed!");
    while(1) delay(1000); // Halt
  }
  
  // Print display configuration and connection info
  display_manager->printDisplayConfiguration();
  
  // Scan I2C bus for all devices
  display_manager->scanI2C();
  
  // Verify each driver individually with proper error handling
  if (!display_manager->verifyDrivers()) {
    Serial.println("WARNING: Some display drivers failed verification!");
    Serial.println("Check I2C connections and ADDR pin configuration.");
    Serial.println("System will continue but displays may not work properly.");
    delay(3000); // Give user time to read the warning
  }
  
  // Initialize Clock Display
  Serial.println("Initializing ClockDisplay...");
  clock_display = new ClockDisplay(display_manager, &wifiTimeLib);
  clock_display->initialize();
  
  // Initialize Meteor Animation
  Serial.println("Initializing MeteorAnimation...");
  meteor_animation = new MeteorAnimation(display_manager);
  meteor_animation->initialize();
  
  // Initialize messages
  initializeMessages();
  current_message = getMessage(0);  // Start with first message
  
  // Initialize WiFi and time synchronization
  Serial.println("Connecting to WiFi...");
  
  // Initialize SignTextController instances after display manager
  init_sign_controllers();
  
  // Show connecting message on display (static)
  display_static_message("WiFi connecting...", true, 500);
  
  // Set up WiFiManager with callback for AP mode
  WiFiManager wm;
  wm.setAPCallback(configModeCallback);
  
  // Try to connect
  if (wm.autoConnect("RetroText")) {
    Serial.println("WiFi connected, syncing time...");
    
    // Show connected message (static)
    display_static_message("OK! Syncing time...", true, 1000);
    
    if (wifiTimeLib.getNTPtime(10, nullptr)) {
      Serial.println("Time synchronized successfully");
      
      // Show success message (static)
      display_static_message("Time synced.", true, 1000);
    } else {
      Serial.println("Warning: Time sync failed, clock mode may show incorrect time");
    }
  } else {
    Serial.println("Warning: WiFi connection failed, clock mode will not work properly");
    // Show failure message but continue with demo
    display_static_message("WiFi failed - demo mode", true, 2000);
  }
  
  // LED initialization moved to DisplayManager
  
  // Setup button for mode switching
  pinMode(USER_BUTTON, INPUT_PULLUP);

  // Initialize demo mode
  last_mode_change = millis();  // Start the demo mode timer
  current_module_announced = false;  // Will trigger announcement on first loop
  current_module_complete = false;   // Module not complete yet
  
  Serial.println("Starting demo mode...");
}

void loop()
{
  static unsigned long last_debug = 0;
  static bool first_loop = true;
  
  // Debug output on first loop and every 5 seconds
  if (first_loop || (millis() - last_debug > 5000)) {
    Serial.printf("Loop: mode=%d, demo=%s, user=%s, announced=%s, complete=%s, button=%s\n", 
                  current_mode, 
                  demo_mode_enabled ? "true" : "false",
                  user_mode_enabled ? "true" : "false", 
                  current_module_announced ? "true" : "false",
                  current_module_complete ? "true" : "false",
                  digitalRead(USER_BUTTON) ? "HIGH" : "LOW");
    last_debug = millis();
    first_loop = false;
  }
  
  // Check for button press - immediate response
  if (check_button_press()) {
    switch_mode();
    // Don't return - continue to update the module
  }
  
  // Demo mode auto-switching logic
  if (demo_mode_enabled) {
    bool should_switch = false;
    
    if (current_mode == MODE_ALT_FONT || current_mode == MODE_MIN_FONT) {
      // Text modules: switch only when message is complete
      should_switch = current_module_complete;
    } else {
      // Non-text modules: switch after timer interval
      should_switch = (millis() - last_mode_change > DEMO_MODE_INTERVAL);
    }
    
    if (should_switch) {
      auto_switch_mode();
      // Don't return - continue to update the module
    }
  }
  
  // Always update the current module
  update_current_module();
  
  delay(25);  // Small delay for system stability
}