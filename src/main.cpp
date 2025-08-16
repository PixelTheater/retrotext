#include <Arduino.h>
#include <Wire.h>
#include <fonts/retro_font4x6.h>
#include <fonts/modern_font4x6.h>  // Include the converted alternative font
#include <sstream> // used for parsing and building strings
#include <iostream>
#include <string>
#include "is31fl3733.hpp"
#include "WifiTimeLib.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include "SignTextController.h"
#include "messages.h"

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

// Forward declarations
void configModeCallback(WiFiManager *myWiFiManager);
bool check_button_press();
void smooth_scroll_story();
void auto_switch_mode();
void select_random_message();
void font_test(int speed);
void update_clock_display();
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

// Total LED driver boards
#define NUM_BOARDS 3
#define WIDTH 24 // arranged in one row
#define HEIGHT 6
// virtual screen
#define MAX_X (WIDTH * NUM_BOARDS) // 24 columns
#define MAX_Y HEIGHT  // 6 rows
#define PIXELS_PER_BOARD 16*12  // We're abusing the IS31FL3733 driver with an IS31FL3737 (12*12)
uint8_t dig_buffer[NUM_BOARDS][PIXELS_PER_BOARD];  // screen buffer for bulk updates

#define DRIVER_DEFAULT_BRIGHTNESS 50
#define TEXT_BRIGHT 150        // For time, capitalized words
#define TEXT_DEFAULT_BRIGHTNESS 70  // For normal text
#define TEXT_DIM 20            // For regular lowercase text
#define TEXT_VERY_DIM 8       // For background elements
#define DEMO_MODE_INTERVAL 30000  // 30 seconds between auto mode changes

using namespace IS31FL3733;


// ---------------------------------------------------------------------------------------------

void i2c_scan() {
  Serial.println("\nScanning I2C bus for devices...");
  uint8_t device_count = 0;
  
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.printf("I2C device found at address 0x%02X (%d)\n", address, address);
      device_count++;
    } else if (error == 4) {
      // Only show timeout errors for expected LED driver addresses
      if (address == 0x50 || address == 0x5A || address == 0x5F) {
        Serial.printf("Timeout at expected LED driver address 0x%02X\n", address);
      }
    }
  }
  
  if (device_count == 0) {
    Serial.println("No I2C devices found via scan");
  } else {
    Serial.printf("Found %d I2C device(s) via scan\n", device_count);
  }
  Serial.println("I2C scan complete\n");
}



uint8_t i2c_read_reg(const uint8_t i2c_addr, const uint8_t reg_addr, uint8_t *buffer, const uint8_t length)
/**
 * @brief Read a buffer of data from the specified register.
 * 
 * @param i2c_addr I2C address of the device to read the data from.
 * @param reg_addr Address of the register to read from.
 * @param buffer Buffer to read the data into.
 * @param length Length of the buffer.
 * @return uint8_t 
 */
{
  Wire.beginTransmission(i2c_addr);
  Wire.write(reg_addr);
  Wire.endTransmission();
  byte bytesRead = Wire.requestFrom(i2c_addr, length);
  for (int i = 0; i < bytesRead && i < length; i++)
  {
    buffer[i] = Wire.read();
  }
  return bytesRead;
}
uint8_t i2c_write_reg(const uint8_t i2c_addr, const uint8_t reg_addr, const uint8_t *buffer, const uint8_t count)
/**
 * @brief Writes a buffer to the specified register. It is up to the caller to ensure the count of
 * bytes to write doesn't exceed 31, which is the Arduino's write buffer size (32) minus one byte for
 * the register address.
 * 
 * @param i2c_addr I2C address of the device to write the data to.
 * @param reg_addr Address of the register to write to.
 * @param buffer Pointer to an array of bytes to write.
 * @param count Number of bytes in the buffer.
 * @return uint8_t 0 if success, non-zero on error.
 */
{
  Wire.beginTransmission(i2c_addr);
  Wire.write(reg_addr);
  Wire.write(buffer, count);
  return Wire.endTransmission();
}


// Create a driver for each board with the address, and provide the I2C read and write functions.
IS31FL3733Driver drivers[NUM_BOARDS] = {
  IS31FL3733Driver(ADDR::GND, ADDR::GND, &i2c_read_reg, &i2c_write_reg), 
  IS31FL3733Driver(ADDR::VCC, ADDR::VCC, &i2c_read_reg, &i2c_write_reg),
  IS31FL3733Driver(ADDR::SDA, ADDR::SDA, &i2c_read_reg, &i2c_write_reg),
};

void verify_led_drivers() {
  Serial.println("Verifying LED driver communication...");
  
  for (int board = 0; board < NUM_BOARDS; board++) {
    uint8_t address = drivers[board].GetI2CAddress();
    Serial.printf("Testing communication with board %d at address 0x%02X... ", board, address);
    
    // Try to read a register from the device
    uint8_t test_data = 0;
    uint8_t result = i2c_read_reg(address, 0x00, &test_data, 1); // Read register 0x00
    
    if (result > 0) {
      Serial.printf("SUCCESS - Read %d byte(s), data: 0x%02X\n", result, test_data);
    } else {
      Serial.println("FAILED - No response");
    }
    
    // Also test a simple ping
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();
    Serial.printf("  Direct ping result: %s (error code: %d)\n", 
                  error == 0 ? "SUCCESS" : "FAILED", error);
  }
  Serial.println("LED driver verification complete\n");
}


uint8_t get_led_number(uint8_t x, uint8_t y) {
  uint8_t led_number = 0;

  if (x < WIDTH && y < HEIGHT) {
    // display is in two halves, as 12x6 and 12x6
    if (x < 12) {
      //if (x>=6 && x<12) x+=2;
      led_number = y * 12 + x;
    } else {
      //if (x>=6 && x<12) x+=2;
      led_number = 72 + y * 12 + (x - 12);
    }
  }
  return led_number;
}

// pass virtual screen coordinates and set a specific LED
// this maps the layout to the correct board and 
// translates coordinates (12x12) to match the correct LED on the PCB
void set_led(uint8_t x, uint8_t y, uint8_t brightness) {
  // flip and invert
  int screen_x = MAX_X - x - 1;
  int screen_y = MAX_Y - y - 1;

  // first locate the board based on x position
  int board = screen_x / WIDTH;

  // cs and sw are mapped to 12x12 LED driver space
  int cs = screen_x % WIDTH;
  int sw = cs < 12 ? screen_y : screen_y + 6;
  cs = cs % 12;


  // The IS31FL3737 has 12 columns and 12 rows, but we are using the driver for
  // the IS31FL3733, which has 16 columns and 12 rows. Because of this,
  // CS7-CS12 (6..11) are off by 2, and must map to values 8-13.
  if (cs >= 6 && cs < 12) cs += 2;
  // the buffer is just an array, y*12+x
  int led_number = sw * 16 + cs;
  if (led_number < PIXELS_PER_BOARD){
    dig_buffer[board][led_number] = brightness;  
  }
}


uint8_t get_led(uint8_t x, uint8_t y) {
  int board = x / 24;
  int lx = x % 24;
  uint8_t led_number = get_led_number(lx, y);
  return dig_buffer[board][led_number];
}

void draw_buffer(){
  for (int b=0; b<NUM_BOARDS; b++){
    drivers[b].SetPWM(dig_buffer[b]);
  }
}

void clear_buffer(){
  for (int b=0; b<NUM_BOARDS; b++){
    std::fill_n(dig_buffer[b], 16*12, 0);
  }
}

void dim_buffer(uint8_t amount){
  for (int b = 0; b < NUM_BOARDS; b++) {
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
      if (dig_buffer[b][i] > 35){
        dig_buffer[b][i] *= 0.85; // move faster over bright levels
      }
      if (amount > dig_buffer[b][i]) {
        dig_buffer[b][i] = 0;
      } else {
        dig_buffer[b][i] -= amount;
      }
    }
  }
}


// Convert alternative font character to pattern for our display
uint8_t get_alt_character_pattern(uint8_t character, uint8_t row) {
  uint8_t pattern = 0;
  
  // Now using the converted font with adaptive shifting (6 rows optimally positioned)
  if (character >= 0 && character <= (sizeof(modern_font4x6)-3)/6) {
    pattern = pgm_read_byte(&modern_font4x6[3+character*6+row]) >> 4;
  }
  
  return pattern;
}

// returns a 4-byte pattern for the specified row of a given 4x6 character
uint8_t get_character_pattern(uint8_t character, uint8_t row) {
  // Only used for MODE_MIN_FONT and MODE_ALT_FONT (message modes)
  if (current_mode == MODE_ALT_FONT) {
    return get_alt_character_pattern(character, row);
  }
  
  // Default to original font (MODE_MIN_FONT)
  uint8_t pattern = 0;
  if (character >= 0 && character <= (sizeof(retro_font4x6)-3)/6) {
    pattern = retro_font4x6[3+character*6+row] >> 4;
  }
  return pattern;
}

// move display left four pixels smoothly, filling in the new character on the right
void shift_in_character(uint8_t character, int speed){
  for (uint8_t pixel=0; pixel<4; pixel++){
    for (uint8_t row=0; row<6; row++){
      // shift all pixels left by 1
      for (uint8_t col=0; col<23; col++){
        set_led(col, row, get_led(col+1, row));
      }
      // fill in the rightmost column with the new character
      uint8_t pattern = get_character_pattern(character, row);
      if ((pattern & (1 << (3-pixel))) != 0) { // column is on in pattern
        set_led(23, row, TEXT_DEFAULT_BRIGHTNESS);
      } else {
        set_led(23, row, 0);
      }
    }
    draw_buffer();
    delay(speed);
  }

}

// write a character to the display using 4x6 font
// position is 0-5, character is 0-95
void write_character(uint8_t character, uint8_t pos){
  if (pos < 0 || pos >= MAX_X) return;
  int col_offset = pos * 4;
  for (uint8_t row = 0; row < 6; row++) { // draw 6 rows
    uint8_t pattern = get_character_pattern(character, row);
    for (uint8_t col = 0; col < 4; col++) { // draw 4 columns
      if ((pattern & (1 << col)) != 0) { // column is on in pattern
        set_led((3-col)+col_offset, row, TEXT_DEFAULT_BRIGHTNESS);
      } else {
        set_led((3-col)+col_offset, row, 0);
      }
    }
  }
}

// Helper function to properly copy pixel from one location to another
// Handles the coordinate system mismatch between set_led and get_led
void copy_pixel(uint8_t from_x, uint8_t from_y, uint8_t to_x, uint8_t to_y) {
  uint8_t brightness = get_led(from_x, from_y);
  set_led(to_x, to_y, brightness);
}

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

// Enhanced write_character that can position at any pixel offset with dynamic brightness
void write_character_at_offset(uint8_t character, int pixel_offset, uint8_t brightness, bool use_alt_font = true) {
  // Set font mode temporarily
  DisplayMode old_mode = current_mode;
  if (use_alt_font) {
    current_mode = MODE_ALT_FONT;
  } else {
    current_mode = MODE_MIN_FONT;
  }
  
  for (uint8_t row = 0; row < 6; row++) {
    uint8_t pattern = get_character_pattern(character, row);
    for (uint8_t col = 0; col < 4; col++) {
      int x_pos = pixel_offset + (3 - col);  // Character positioning logic from write_character
      if (x_pos >= 0 && x_pos < MAX_X) {  // Only draw if within display bounds
        if ((pattern & (1 << col)) != 0) {
          set_led(x_pos, row, brightness);
        } else {
          set_led(x_pos, row, 0);
        }
      }
    }
  }
  
  current_mode = old_mode;  // Restore original mode
}

// Smooth scrolling using character positioning - renders entire message at sub-pixel offsets
void display_smooth_scrolling_text(String text, int scroll_speed = 40, bool use_alt_font = true) {
  Serial.printf("Smooth scrolling (char positioning): %s\n", text.c_str());
  
  // If text fits in 18 characters, just display it statically
  if (text.length() <= 18) {
    clear_buffer();
    for (int i = 0; i < text.length(); i++) {
      char c = text.charAt(i);
      uint8_t ascii = c - 32;
      uint8_t brightness = get_character_brightness(c, text, i, false);
      write_character_at_offset(ascii, i * 4, brightness, use_alt_font);
    }
    draw_buffer();
    delay(2000);
    return;
  }
  
  // Calculate total scroll range: need to scroll until last char is visible
  // Total text width = text.length() * 4 pixels
  // Display width = 72 pixels (18 chars * 4)
  // Need to scroll: total_width - display_width + 4 (to fully exit last char)
  int total_scroll_pixels = (text.length() * 4) - 72 + 4;
  
  // Show initial position briefly
  clear_buffer();
  for (int i = 0; i < min((int)text.length(), 18); i++) {
    char c = text.charAt(i);
    uint8_t ascii = c - 32;
    uint8_t brightness = get_character_brightness(c, text, i, false);
    write_character_at_offset(ascii, i * 4, brightness, use_alt_font);
  }
  draw_buffer();
  delay(scroll_speed * 15);
  
  // Now scroll pixel by pixel
  for (int scroll_offset = 0; scroll_offset < total_scroll_pixels; scroll_offset++) {
    clear_buffer();
    
    // Render all characters of the message at their offset positions
    for (int char_idx = 0; char_idx < text.length(); char_idx++) {
      char c = text.charAt(char_idx);
      uint8_t ascii = c - 32;
      int char_pixel_pos = (char_idx * 4) - scroll_offset;  // Character position minus scroll offset
      
      // Only render characters that are at least partially visible
      if (char_pixel_pos > -4 && char_pixel_pos < MAX_X) {
        uint8_t brightness = get_character_brightness(c, text, char_idx, false);
        write_character_at_offset(ascii, char_pixel_pos, brightness, use_alt_font);
      }
    }
    
    draw_buffer();
    delay(scroll_speed);
  }
  
  delay(scroll_speed * 15); // Show final position longer
}

// Non-smooth character-by-character scrolling (like original font_test)
void display_character_scrolling_text(String text, int scroll_speed = 120, bool use_alt_font = false) {
  Serial.printf("Character scrolling: %s\n", text.c_str());
  
  // If text fits in 18 characters, just display it
  if (text.length() <= 18) {
    clear_buffer();
    for (int i = 0; i < text.length(); i++) {
      char c = text.charAt(i);
      uint8_t ascii = c - 32;
      uint8_t brightness = get_character_brightness(c, text, i, false);
      write_character_at_offset(ascii, i * 4, brightness, use_alt_font);
    }
    draw_buffer();
    delay(2000);
    return;
  }
  
  // Scroll character by character (like font_test)
  for (int start_char = 0; start_char <= text.length() - 18; start_char++) {
    clear_buffer();
    
    // Display 18 characters starting from start_char
    for (int i = 0; i < 18 && (start_char + i) < text.length(); i++) {
      char c = text.charAt(start_char + i);
      uint8_t ascii = c - 32;
      uint8_t brightness = get_character_brightness(c, text, start_char + i, false);
      write_character_at_offset(ascii, i * 4, brightness, use_alt_font);
    }
    
    draw_buffer();
    delay(scroll_speed);
  }
  
  delay(1000); // Show final text longer
}

// Meteor animation with parallax stars
void display_meteor_animation() {
  const int NUM_METEORS = 9;
  const int NUM_STARS = 24;
  
  // Animation state
  static bool initialized = false;
  static float meteor_positions[NUM_METEORS];
  static float star_positions[NUM_STARS];
  if (!initialized) {
    for (int i = 0; i < NUM_METEORS; i++) {
      // Meteors start at random negative positions to stagger their entry
      meteor_positions[i] = -10.0f - (rand() % 40);
    }
    for (int i = 0; i < NUM_STARS; i++) {
      // Stars start at random positions across the display width
      star_positions[i] = (float)(rand() % (MAX_X + 20));
    }
    initialized = true;
  }
  static unsigned long last_update = 0;
  
  if (millis() - last_update < 50) return; // Update at 20 FPS
  last_update = millis();
  
  clear_buffer();
  
  // Draw background stars (parallax effect - odd stars move faster, slower stars are dimmer)
  for (int i = 0; i < NUM_STARS; i++) {
    int star_x = (int)star_positions[i] % (MAX_X + 10);
    int star_y = (i % 6); // Distribute stars across rows 1-3
    
    if (star_x >= 0 && star_x < MAX_X) {
      // Odd stars move faster and are brighter, even stars move slower and are dimmer
      uint8_t star_brightness = (i % 2 == 0) ? TEXT_VERY_DIM : TEXT_DIM;
      set_led(star_x, star_y, star_brightness);
    }
    
    // Movement speed: odd stars faster, even stars slower
    float move_speed = (i % 2 == 0) ? 0.2f : 0.5f;
    star_positions[i] -= move_speed;
    if (star_positions[i] < -10) star_positions[i] = MAX_X + 10;
  }
  
  // Draw meteors (fast movement with trails)
  for (int m = 0; m < NUM_METEORS; m++) {
    int meteor_x = (int)meteor_positions[m];
    int meteor_y = m % 6; // Distribute meteors across all 6 rows
    
    // Meteor speed based on index - higher index = faster
    float meteor_speed = 1.0f + (m * 0.2f);
    
    // Trail length based on speed - faster meteors have longer trails
    int trail_length = 3 + (m / 2); // 3-7 trails based on meteor index
    
    // Draw meteor trail (fading brightness)
    for (int trail = 0; trail < trail_length; trail++) {
      int trail_x = meteor_x - trail;
      if (trail_x >= 0 && trail_x < MAX_X) {
        uint8_t trail_brightness = TEXT_BRIGHT - (trail * 15);
        if (trail_brightness > TEXT_VERY_DIM) {
          set_led(trail_x, meteor_y, trail_brightness);
        }
      }
    }
    
    // Move meteor
    meteor_positions[m] += meteor_speed;
    if (meteor_positions[m] > MAX_X + 10) {
      meteor_positions[m] = -15 - (m * 5); // Reset with staggered timing
    }
  }
  
  draw_buffer();
}



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
  write_character_at_offset(character, pixel_offset, brightness, use_alt_font);
}

void clear_display_callback() {
  clear_buffer();
}

void draw_display_callback() {
  draw_buffer();
}

uint8_t brightness_callback(char c, String text, int char_pos, bool is_time_display) {
  return get_character_brightness(c, text, char_pos, is_time_display);
}

// Initialize the SignTextController instances
void init_sign_controllers() {
  // Create modern font controller
  modern_sign = new RetroText::SignTextController(18, 4);  // 18 chars, 4 pixels per char
  modern_sign->setFont(RetroText::MODERN_FONT);
  modern_sign->setScrollStyle(RetroText::SMOOTH);
  modern_sign->setScrollSpeed(45);
  modern_sign->setBrightness(TEXT_DEFAULT_BRIGHTNESS);
  modern_sign->setRenderCallback(render_character_callback);
  modern_sign->setClearCallback(clear_display_callback);
  modern_sign->setDrawCallback(draw_display_callback);
  modern_sign->setBrightnessCallback(brightness_callback);
  
  // Create retro font controller
  retro_sign = new RetroText::SignTextController(18, 4);  // 18 chars, 4 pixels per char
  retro_sign->setFont(RetroText::ARDUBOY_FONT);
  retro_sign->setScrollStyle(RetroText::CHARACTER);
  retro_sign->setScrollSpeed(120);
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
  
  sign->setScrollStyle(RetroText::STATIC);
  sign->setMessage(message);
  sign->reset();
  sign->update();
  
  if (display_time_ms > 0) {
    delay(display_time_ms);
  }
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
      // Retro font - character scrolling text  
      font_test(160);
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
      // Clock display - continuous updates
      static unsigned long last_clock_update = 0;
      if (millis() - last_clock_update > 1000) { // Update every second
        update_clock_display();
        last_clock_update = millis();
      }
      current_module_complete = true;  // Non-text modules are always "complete"
      break;
      
    case MODE_ANIMATION:
      // Meteor animation - continuous updates
      display_meteor_animation();
      current_module_complete = true;  // Non-text modules are always "complete"
      break;
  }
}



void font_test(int speed){
  static int current_char = 0;

  //clear_buffer();
  for (uint8_t pos=0; pos<18; pos++){
    // get the ascii number to display from the message using pos and current_char
    uint8_t ascii = current_message.charAt(current_char+pos);
    write_character(ascii-32, pos);
  }
  draw_buffer();

  delay(speed);

  //shift_in_character(current_message.charAt(current_char+6)-32, speed);

  current_char++;
  if (current_char > current_message.length()-6){
    current_char = 0;
  }
}

// Smooth scrolling story text for modern font mode using SignTextController
void smooth_scroll_story() {
  static bool initialized = false;
  static RetroText::SignTextController* story_sign = nullptr;
  static unsigned long last_update = 0;
  
  // Initialize the story controller with infinite scrolling settings
  if (!initialized) {
    story_sign = new RetroText::SignTextController(18, 4);
    story_sign->setFont(RetroText::MODERN_FONT);
    story_sign->setScrollStyle(RetroText::SMOOTH);
    story_sign->setScrollSpeed(50); // 50ms between updates for smooth scrolling
    story_sign->setRenderCallback(render_character_callback);
    story_sign->setClearCallback(clear_display_callback);
    story_sign->setDrawCallback(draw_display_callback);
    story_sign->setBrightnessCallback(brightness_callback);
    story_sign->setMessage(current_message);
    initialized = true;
  }
  
  // Update message if it changed
  if (story_sign->getMessage() != current_message) {
    story_sign->setMessage(current_message);
    story_sign->reset();
  }
  
  // Update at controller's speed
  if (millis() - last_update >= 50) {
    story_sign->update();
    
    // Reset when complete for continuous scrolling
    if (story_sign->isComplete()) {
      story_sign->reset();
    }
    
    last_update = millis();
  }
}

int spos=0;
void font_test_2(int speed){
  Serial.println(spos);
  Serial.println(current_message.length());
  clear_buffer();
  for (int x=0; x<18; x++){
    uint8_t ascii = current_message.charAt(x+spos);
    write_character(ascii-32, x);
  }
  draw_buffer();
  delay(speed);

  spos = spos + 1;
  if (spos > current_message.length()){
    spos = 0;
    Serial.println("reset spos");
  }
}


void xy_test(){
  clear_buffer();

  for (uint8_t x=0; x<MAX_X; x++){
    for (uint8_t y=0; y<MAX_Y; y++){
      set_led(x, y, TEXT_DEFAULT_BRIGHTNESS);
      // dig_buffer[0][y*WIDTH+x] = 90; 
      // set_pixel(x,y,90);
      delay(20);
      draw_buffer();
    }
  }
  delay(2000);
  //clear_buffer();

  for (uint8_t y=0; y<MAX_Y; y++){
    for (uint8_t x=0; x<MAX_X; x++){
      set_led(x, y, 0);
      //dig_buffer[0][y*WIDTH+x] = 90; 
      //set_pixel(x,y,90);
      delay(20);
      draw_buffer();
    }
  }
  delay(2000);
}

void bouncy_ball(){
  static uint8_t x = 0;
  static uint8_t y = 0;
  static int x_dir = 1;
  static int y_dir = 1;
  uint8_t x_max = MAX_X;
  uint8_t y_max = MAX_Y;

  clear_buffer();

  if (x>=x_max){
    x_dir = -1; x=x_max;
  }
  if (x<=0){
    x_dir = 1; x=0;
  }
  if (y>=y_max){
    y_dir = -1; y=y_max;
  }
  if (y<=0){
    y_dir = 1; y=0;
  }
  x += x_dir; 
  y += y_dir;
  set_led(x, y, TEXT_DEFAULT_BRIGHTNESS);
  draw_buffer();
}

// Time formatting for clock mode
String format_clock_display() {
  time(&now);
  localtime_r(&now, &timeinfo);
  
  // Format as "Aug 12 Th 12:43:25" (exactly 18 characters)
  char formatted[19];
  char month_name[4];
  char day_name[3];
  
  // Get month abbreviation (3 chars)
  strftime(month_name, sizeof(month_name), "%b", &timeinfo);
  
  // Get day abbreviation (2 chars)
  strftime(day_name, sizeof(day_name), "%a", &timeinfo);
  day_name[2] = '\0'; // Ensure only 2 chars
  
  snprintf(formatted, sizeof(formatted), 
           "%s %2d %s %02d:%02d:%02d",
           month_name,  // Month (3 chars)
           timeinfo.tm_mday,  // Day (2 chars)
           day_name,  // Day name (2 chars)
           timeinfo.tm_hour,  // Hour (2 chars)
           timeinfo.tm_min,   // Minute (2 chars) 
           timeinfo.tm_sec);  // Second (2 chars)
  
  return String(formatted);
}

// Helper function to update the clock display using SignTextController
void update_clock_display() {
  String time_display = format_clock_display();
  
  // Use modern font for clock display with static mode
  modern_sign->setScrollStyle(RetroText::STATIC);
  modern_sign->setMessage(time_display);
  modern_sign->reset();
  modern_sign->update();
}

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
  
  const char* mode_names[] = {"AltFont", "BasicFont", "Clock", "Animation"};
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
  
  const char* mode_names[] = {"AltFont", "BasicFont", "Clock", "Animation"};
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
  static unsigned long startup_time = millis();  // Record startup time
  static unsigned long last_successful_press = 0;
  const unsigned long min_press_interval = 500;  // Minimum time between presses
  const unsigned long startup_delay = 10000;     // Ignore button for 10 seconds after startup
  
  // Ignore button presses for the first few seconds after startup
  if (millis() - startup_time < startup_delay) {
    return false;
  }
  
  // Handle button input (using proven state-based approach)
  if (digitalRead(USER_BUTTON) == LOW) {
    // Button is currently pressed
    if (buttonPressTime == 0) {
      buttonPressTime = millis(); // Mark the time button was first pressed
      Serial.println("*** BUTTON PRESS DETECTED! - Display paused ***");
      
      // Clear display to show immediate response
      clear_buffer();
      draw_buffer();
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
  
  // Initialize messages
  initializeMessages();
  current_message = getMessage(0);  // Start with first message
  
  // Initialize WiFi and time synchronization
  Serial.println("Connecting to WiFi...");
  
  // Initialize SignTextController instances early for startup messages
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
  
  // Add extra delay to ensure I2C devices are ready
  Serial.println("Waiting for I2C devices to stabilize...");
  delay(1000);
  
  // Scan for I2C devices
  i2c_scan();

  Serial.print("I2C speed is ");
  Serial.println(Wire.getClock());

  Serial.println("Initializing LED Drivers");
  for (int board=0; board<NUM_BOARDS; board++) {
    //if (board != 2) continue; // only initialize the 3rd board
    Serial.printf("\nIS31FL3733B driver init board %d at address 0x", board);
    Serial.println(drivers[board].GetI2CAddress(), HEX);  
    drivers[board].Init();

    Serial.println(" -> Setting global current control");
    drivers[board].SetGCC(DRIVER_DEFAULT_BRIGHTNESS);

    Serial.println(" -> Setting state of all LEDs");
    drivers[board].SetLEDMatrixPWM(TEXT_DIM); // set brightness
    for (int x=0; x<6; x++){
      drivers[board].SetLEDMatrixState(x % 2 == 0 ? LED_STATE::ON : LED_STATE::OFF);
      delay(70);
    }
    drivers[board].SetLEDMatrixState(LED_STATE::ON);
    drivers[board].SetLEDMatrixPWM(0); // set brightness
  }

  // Verify LED driver communication after initialization
  verify_led_drivers();

  clear_buffer();
  draw_buffer();
  
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