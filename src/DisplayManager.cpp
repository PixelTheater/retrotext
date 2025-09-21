#include "DisplayManager.h"
#include <Wire.h>
#include <fonts/retro_font4x6.h>
#include <fonts/modern_font4x6.h>

DisplayManager::DisplayManager(int num_boards, int board_width, int board_height)
  : num_boards_(num_boards)
  , board_width_(board_width)
  , board_height_(board_height)
  , total_width_(board_width * num_boards)
  , total_height_(board_height)
  , character_width_(4)
  , max_characters_(total_width_ / character_width_)
  , drivers_{nullptr, nullptr, nullptr, nullptr}
{
}

DisplayManager::~DisplayManager() {
  for (int i = 0; i < 4; i++) {
    if (drivers_[i]) {
      delete drivers_[i];
    }
  }
}

bool DisplayManager::initialize() {
  Serial.println("Initializing DisplayManager...");
  
  // Initialize I2C
  Serial.println("Setting up I2C...");
  Wire.begin();
  Wire.setClock(800000); // 800 kHz I2C
  Serial.println("I2C setup complete");
  
  // Initialize drivers
  Serial.println("Creating individual drivers...");
  initializeDrivers();
  
  // Initialize each driver
  for (int i = 0; i < num_boards_; i++) {
    if (drivers_[i]) {
      Serial.printf("Initializing driver %d...\n", i);
      drivers_[i]->begin();
      drivers_[i]->setGlobalCurrent(50);
      Serial.printf("Driver %d initialized\n", i);
    }
  }
  
  Serial.println("Clearing all displays...");
  clearBuffer();
  Serial.println("Updating all displays...");
  updateDisplay();
  
  Serial.println("DisplayManager initialization complete");
  return true;
}

bool DisplayManager::verifyDrivers() {
  Serial.println("Verifying LED driver communication...");
  
  bool all_ok = true;
  for (int i = 0; i < num_boards_; i++) {
    if (!drivers_[i]) {
      Serial.printf("Driver %d not initialized\n", i);
      all_ok = false;
      continue;
    }
    
    Serial.printf("Testing driver %d...\n", i);
    // Test basic operations
    drivers_[i]->clear();
    drivers_[i]->drawPixel(0, 0, 100);
    drivers_[i]->show();
    Serial.printf("Driver %d verified\n", i);
  }
  
  Serial.printf("Driver verification complete: %d boards\n", num_boards_);
  return all_ok;
}

void DisplayManager::scanI2C() {
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

void DisplayManager::setPixel(int x, int y, uint8_t brightness) {
  if (!isValidPosition(x, y)) return;
  
  // Apply the same coordinate transformation as the old working code
  int screen_x = total_width_ - x - 1;
  int screen_y = total_height_ - y - 1;
  
  // Determine which board this pixel belongs to
  int board = screen_x / board_width_;
  if (board >= num_boards_ || !drivers_[board]) return;
  
  // Calculate local coordinates within the board (24x6 logical)
  int local_x = screen_x % board_width_;  // 0-23
  int local_y = screen_y;                 // 0-5
  
  // Convert 24x6 logical coordinates to 12x12 physical coordinates for RetroText PCB
  // 6 characters in a line: chars 0,1,2 use SW1-6, chars 3,4,5 use SW7-12
  int char_index = local_x / 4;  // Which character (0-5)
  int char_pixel_x = local_x % 4;  // Pixel within character (0-3)
  
  int physical_x, physical_y;
  
  if (char_index < 3) {
    // Characters 0,1,2: use SW1-6 (top half)
    physical_x = (char_index * 4) + char_pixel_x;  // CS1-4, CS5-8, CS9-12
    physical_y = local_y;  // SW1-6 (rows 0-5)
  } else {
    // Characters 3,4,5: use SW7-12 (bottom half)  
    physical_x = ((char_index - 3) * 4) + char_pixel_x;  // CS1-4, CS5-8, CS9-12
    physical_y = local_y + 6;  // SW7-12 (rows 6-11)
  }
  
  drivers_[board]->drawPixel(physical_x, physical_y, brightness);
}

uint8_t DisplayManager::getPixel(int x, int y) const {
  if (!isValidPosition(x, y)) return 0;
  
  // IS31FL373x drivers don't support pixel readback
  // Return 0 - this is mainly used for debugging
  // Most LED drivers don't support reading pixel values back
  return 0;
}

bool DisplayManager::isValidPosition(int x, int y) const {
  return (x >= 0 && x < total_width_ && y >= 0 && y < total_height_);
}

void DisplayManager::clearBuffer() {
  for (int i = 0; i < num_boards_; i++) {
    if (drivers_[i]) {
      drivers_[i]->clear();
    }
  }
}

void DisplayManager::fillBuffer(uint8_t brightness) {
  // fillScreen doesn't exist in IS31FL373x driver, so implement manually
  for (int y = 0; y < total_height_; y++) {
    for (int x = 0; x < total_width_; x++) {
      setPixel(x, y, brightness);
    }
  }
}

void DisplayManager::dimBuffer(uint8_t amount) {
  // Use master brightness control for dimming effect
  for (int i = 0; i < num_boards_; i++) {
    if (drivers_[i]) {
      uint8_t current_brightness = 255 - amount;
      if (current_brightness < 10) current_brightness = 10;  // Minimum visibility
      drivers_[i]->setMasterBrightness(current_brightness);
    }
  }
}

void DisplayManager::updateDisplay() {
  for (int i = 0; i < num_boards_; i++) {
    if (drivers_[i]) {
      drivers_[i]->show();
    }
  }
}

void DisplayManager::drawCharacter(uint8_t character_pattern[6], int x_offset, uint8_t brightness) {
  for (int row = 0; row < 6; row++) {
    uint8_t pattern = character_pattern[row];
    for (int col = 0; col < 4; col++) {
      int x_pos = x_offset + (3 - col);  // Character positioning logic
      if (x_pos >= 0 && x_pos < total_width_) {
        if ((pattern & (1 << col)) != 0) {
          setPixel(x_pos, row, brightness);
        } else {
          setPixel(x_pos, row, 0);
        }
      }
    }
  }
}

void DisplayManager::drawText(const String& text, int start_x, uint8_t brightness, bool use_alt_font) {
  for (int i = 0; i < text.length(); i++) {
    char c = text.charAt(i);
    uint8_t character = c - 32; // ASCII offset
    
    // Get character pattern
    uint8_t pattern[6];
    for (int row = 0; row < 6; row++) {
      pattern[row] = getCharacterPattern(character, row, use_alt_font);
    }
    
    // Draw character
    int char_x = start_x + (i * character_width_);
    if (char_x < total_width_) {
      drawCharacter(pattern, char_x, brightness);
    }
  }
}

void DisplayManager::setGlobalBrightness(uint8_t brightness) {
  for (int i = 0; i < num_boards_; i++) {
    if (drivers_[i]) {
      drivers_[i]->setGlobalCurrent(brightness);
    }
  }
}

void DisplayManager::setBoardBrightness(int board_index, uint8_t brightness) {
  if (board_index >= 0 && board_index < num_boards_ && drivers_[board_index]) {
    drivers_[board_index]->setGlobalCurrent(brightness);
  }
}

int DisplayManager::getBoardForPixel(int x) const {
  return x / board_width_;
}

// mapPixelToBoard removed - IS31FL373x_Canvas handles multi-board coordinates directly

uint8_t DisplayManager::getCharacterPattern(uint8_t character, uint8_t row, bool use_alt_font) const {
  if (use_alt_font) {
    // Alternative font
    if (character >= 0 && character <= (sizeof(modern_font4x6)-3)/6) {
      return pgm_read_byte(&modern_font4x6[3+character*6+row]) >> 4;
    }
  } else {
    // Original font
    if (character >= 0 && character <= (sizeof(retro_font4x6)-3)/6) {
      return retro_font4x6[3+character*6+row] >> 4;
    }
  }
  return 0;
}

// I2C functions removed - IS31FL373x driver handles I2C directly

// Private helper methods
void DisplayManager::initializeDrivers() {
  Serial.printf("Creating %d individual drivers\n", num_boards_);
  
  for (int i = 0; i < num_boards_ && i < 4; i++) {
    ADDR addr;
    
    // Initialize with the appropriate addresses for each board
    switch (i) {
      case 0: addr = ADDR::GND; break;
      case 1: addr = ADDR::VCC; break;
      case 2: addr = ADDR::SDA; break;
      case 3: addr = ADDR::SCL; break;
      default: addr = ADDR::GND; break;
    }
    
    Serial.printf("Creating driver %d with address %d\n", i, (int)addr);
    drivers_[i] = new IS31FL3737(addr);
    Serial.printf("Driver %d created successfully\n", i);
  }
  
  Serial.println("All drivers created");
}

void DisplayManager::convertLogicalToPhysical(int logical_x, int logical_y, int& physical_x, int& physical_y) {
  // Convert 24x6 logical coordinates to 12x12 physical coordinates for RetroText PCB
  // Based on PCB_Layout.md: 6 characters arranged horizontally
  // Characters 0,1,2 use SW1-6 (CS1-4, CS5-8, CS9-12)
  // Characters 3,4,5 use SW7-12 (CS1-4, CS5-8, CS9-12)
  
  int char_index = logical_x / 4;      // Which character (0-5)
  int char_pixel_x = logical_x % 4;    // Pixel within character (0-3)
  
  if (char_index < 3) {
    // Characters 0,1,2: top half of chip (SW1-6)
    physical_x = (char_index * 4) + char_pixel_x;  // CS1-4, CS5-8, CS9-12
    physical_y = logical_y;                         // SW1-6 (rows 0-5)
  } else {
    // Characters 3,4,5: bottom half of chip (SW7-12)
    physical_x = ((char_index - 3) * 4) + char_pixel_x;  // CS1-4, CS5-8, CS9-12
    physical_y = logical_y + 6;                           // SW7-12 (rows 6-11)
  }
}
