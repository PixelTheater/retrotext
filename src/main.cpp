#include <Arduino.h>
#include <Wire.h>
#include <font4x6.h>
#include <alt_font4x6_converted.h>  // Include the converted alternative font
#include <sstream> // used for parsing and building strings
#include <iostream>
#include <string>
#include "is31fl3733.hpp"

// Alternative font system
#define FONT_WIDTH 4
#define FONT_HEIGHT 6  // Back to 6 rows with adaptive character shifting

// Font selection
enum FontType {
  FONT_ORIGINAL = 0,
  FONT_ALTERNATIVE = 1
};

FontType current_font = FONT_ALTERNATIVE;




// Timezone config
/* 
  Enter your time zone (https://remotemonitoringsystems.ca/time-zone-abbreviations.php)
  See https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv for Timezone codes for your region
  based on https://github.com/SensorsIot/NTP-time-for-ESP8266-and-ESP32/blob/master/NTP_Example/NTP_Example.ino
*/
const char* NTP_SERVER = "ch.pool.ntp.org";
const char* TZ_INFO    = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";  // Switzerland

// timer
// Ticker timer;

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
#define TEXT_DEFAULT_BRIGHTNESS 50
#define TEXT_DIM_BRIGHTNESS 20

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
  if (character >= 0 && character <= (sizeof(alt_font4x6)-3)/6) {
    pattern = pgm_read_byte(&alt_font4x6[3+character*6+row]) >> 4;
  }
  
  return pattern;
}

// returns a 4-byte pattern for the specified row of a given 4x6 character
uint8_t get_character_pattern(uint8_t character, uint8_t row) {
  if (current_font == FONT_ALTERNATIVE) {
    return get_alt_character_pattern(character, row);
  }
  
  // Original font
  uint8_t pattern = 0;
  if (character >= 0 && character <= (sizeof(font4x6)-3)/6) {
    pattern = font4x6[3+character*6+row] >> 4;
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

// String message = "This dot matrix display serves as a modern art example "
//                  "using LEDs and other advanced electronics to create "
//                  "an intricate and dynamic visual display. This convergence "
//                  "of art and science showcases the potential of minaturized "
//                  "technology in the world of art. For some, this piece "
//                  "inspires a deeper appreciation for the power of art to "
//                  "push the boundaries of what is possible. For those who "
//                  "don\'t appreciate it, we apologize and encourage you "
//                  "to broaden your artistic horizons.      [msg: 42063 $AG END]                        ";
String message = "Did you know? Listen: The speed of light is approximately 299,792 kilometers per second. "
          "The Earth revolves around the Sun at a speed of about 30 kilometers per second. "
          "A single teaspoon of honey represents the life work of 12 bees. "
          "The human brain contains approximately 86 billion neurons. "
          "Water expands by about 9% when it freezes. "
          "The Eiffel Tower can be 15 cm taller during the summer due to thermal expansion. "
          "Bananas are berries, but strawberries aren't. "
          "Octopuses have three hearts and blue blood. "
          "A day on Venus is longer than a year on Venus. "
          "There are more stars in the universe than grains of sand on all the Earth's beaches."
          "      [msg: 42063 $AG END]                        ";

void font_test(int speed){
  static int current_char = 0;

  //clear_buffer();
  for (uint8_t pos=0; pos<18; pos++){
    // get the ascii number to display from the message using pos and current_char
    uint8_t ascii = message.charAt(current_char+pos);
    write_character(ascii-32, pos);
  }
  draw_buffer();

  delay(speed);

  //shift_in_character(message.charAt(current_char+6)-32, speed);

  current_char++;
  if (current_char > message.length()-6){
    current_char = 0;
  }
}

int spos=0;
void font_test_2(int speed){
  Serial.println(spos);
  Serial.println(message.length());
  clear_buffer();
  for (int x=0; x<18; x++){
    uint8_t ascii = message.charAt(x+spos);
    write_character(ascii-32, x);
  }
  draw_buffer();
  delay(speed);

  spos = spos + 1;
  if (spos > message.length()){
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

// Font switching functions
void switch_font() {
  current_font = (current_font == FONT_ORIGINAL) ? FONT_ALTERNATIVE : FONT_ORIGINAL;
  Serial.printf("Switched to %s font\n", 
                current_font == FONT_ORIGINAL ? "ORIGINAL" : "ALTERNATIVE");
}

void display_font_demo() {
  clear_buffer();
  
  // Display different text based on font type
  String demo_text;
  if (current_font == FONT_ORIGINAL) {
    demo_text = "ORIG FONT 123!";  // Original font supports full character set
  } else {
    demo_text = "ALT FONT abc123!";  // Alternative font now supports full character set too!
  }
  
  for (int i = 0; i < demo_text.length() && i < 18; i++) {
    uint8_t ascii = demo_text.charAt(i);
    write_character(ascii - 32, i);  // Convert ASCII to character index (subtract 32 for space offset)
  }
  
  draw_buffer();
  delay(2000);
}

// Simplified button press detection
bool check_button_press() {
  static bool button_was_pressed = false;
  static unsigned long last_press_time = 0;
  static unsigned long last_debug_time = 0;
  const unsigned long min_press_interval = 500;  // Minimum time between presses
  
  bool current_state = digitalRead(USER_BUTTON);
  
  // Debug output every 5 seconds
  if (millis() - last_debug_time > 5000) {
    Serial.printf("Button: %s (GPIO %d)\n", current_state ? "RELEASED" : "PRESSED", USER_BUTTON);
    last_debug_time = millis();
  }
  
  // Simple logic: detect press (HIGH to LOW transition)
  if (!current_state && !button_was_pressed) {
    // Button just pressed
    if (millis() - last_press_time > min_press_interval) {
      button_was_pressed = true;
      last_press_time = millis();
      Serial.println("*** BUTTON PRESS DETECTED! ***");
      return true;
    }
  } else if (current_state && button_was_pressed) {
    // Button released
    button_was_pressed = false;
    Serial.println("Button released");
  }
  
  return false;
}


void setup() {
  // Initialize serial and I2C.
  Wire.begin();
  Wire.setClock(800000); // use 800 kHz I2C

  Serial.begin(115200);
  delay(1500);

  // Setup button for font switching
  pinMode(USER_BUTTON, INPUT_PULLUP);

  Serial.println("Retrotext Starting");
  Serial.println("Press USER_BUTTON to switch fonts");
  
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
    drivers[board].SetLEDMatrixPWM(TEXT_DIM_BRIGHTNESS); // set brightness
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
}

void loop()
{
  // Check for button press to switch fonts
  if (check_button_press()) {
    switch_font();
    display_font_demo(); // Show a demo of the current font
  }
  
  font_test(160);
  //xy_test();
  // bouncy_ball();
  //delay(25);
}