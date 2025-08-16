#include "DisplayManager.h"
#include <Wire.h>
#include <fonts/retro_font4x6.h>
#include <fonts/modern_font4x6.h>

using namespace IS31FL3733;

DisplayManager::DisplayManager(int num_boards, int board_width, int board_height)
  : num_boards_(num_boards)
  , board_width_(board_width)
  , board_height_(board_height)
  , total_width_(board_width * num_boards)
  , total_height_(board_height)
  , character_width_(4)
  , max_characters_(total_width_ / character_width_)
  , display_buffers_(nullptr)
  , drivers_(nullptr)
{
}

DisplayManager::~DisplayManager() {
  if (display_buffers_) {
    for (int i = 0; i < num_boards_; i++) {
      delete[] display_buffers_[i];
    }
    delete[] display_buffers_;
  }
  
  if (drivers_) {
    for (int i = 0; i < num_boards_; i++) {
      delete drivers_[i];
    }
    delete[] drivers_;
  }
}

bool DisplayManager::initialize() {
  Serial.println("Initializing DisplayManager...");
  
  // Initialize I2C
  Wire.begin();
  Wire.setClock(800000); // 800 kHz I2C
  
  // Initialize buffers and drivers
  initializeBuffers();
  initializeDrivers();
  
  // Initialize each board
  for (int board = 0; board < num_boards_; board++) {
    Serial.printf("Initializing board %d at address 0x%02X\n", board, drivers_[board]->GetI2CAddress());
    
    drivers_[board]->Init();
    drivers_[board]->SetGCC(50); // Default global current control
    
    // Set all LEDs to ON state but with 0 brightness initially
    drivers_[board]->SetLEDMatrixState(LED_STATE::ON);
    drivers_[board]->SetLEDMatrixPWM(0);
  }
  
  clearBuffer();
  updateDisplay();
  
  Serial.println("DisplayManager initialization complete");
  return true;
}

bool DisplayManager::verifyDrivers() {
  Serial.println("Verifying LED driver communication...");
  bool all_ok = true;
  
  for (int board = 0; board < num_boards_; board++) {
    uint8_t address = drivers_[board]->GetI2CAddress();
    Serial.printf("Testing board %d at address 0x%02X... ", board, address);
    
    // Try to read a register
    uint8_t test_data = 0;
    uint8_t result = i2c_read_reg(address, 0x00, &test_data, 1);
    
    if (result > 0) {
      Serial.printf("SUCCESS - Read data: 0x%02X\n", test_data);
    } else {
      Serial.println("FAILED - No response");
      all_ok = false;
    }
  }
  
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
  
  // Map to board coordinates
  int board, local_x, local_y;
  mapPixelToBoard(x, y, board, local_x, local_y);
  
  // Calculate LED number in buffer
  uint8_t led_number = mapCoordinateToLED(board, local_x, local_y);
  
  if (led_number < PIXELS_PER_BOARD) {
    display_buffers_[board][led_number] = brightness;
  }
}

uint8_t DisplayManager::getPixel(int x, int y) const {
  if (!isValidPosition(x, y)) return 0;
  
  // Map to board coordinates
  int board, local_x, local_y;
  mapPixelToBoard(x, y, board, local_x, local_y);
  
  // Calculate LED number in buffer
  uint8_t led_number = mapCoordinateToLED(board, local_x, local_y);
  
  if (led_number < PIXELS_PER_BOARD) {
    return display_buffers_[board][led_number];
  }
  
  return 0;
}

bool DisplayManager::isValidPosition(int x, int y) const {
  return (x >= 0 && x < total_width_ && y >= 0 && y < total_height_);
}

void DisplayManager::clearBuffer() {
  for (int board = 0; board < num_boards_; board++) {
    std::fill_n(display_buffers_[board], PIXELS_PER_BOARD, 0);
  }
}

void DisplayManager::fillBuffer(uint8_t brightness) {
  for (int board = 0; board < num_boards_; board++) {
    std::fill_n(display_buffers_[board], PIXELS_PER_BOARD, brightness);
  }
}

void DisplayManager::dimBuffer(uint8_t amount) {
  for (int board = 0; board < num_boards_; board++) {
    for (int i = 0; i < PIXELS_PER_BOARD; i++) {
      if (display_buffers_[board][i] > 35) {
        display_buffers_[board][i] *= 0.85; // Move faster over bright levels
      }
      if (amount > display_buffers_[board][i]) {
        display_buffers_[board][i] = 0;
      } else {
        display_buffers_[board][i] -= amount;
      }
    }
  }
}

void DisplayManager::updateDisplay() {
  for (int board = 0; board < num_boards_; board++) {
    drivers_[board]->SetPWM(display_buffers_[board]);
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
  for (int board = 0; board < num_boards_; board++) {
    drivers_[board]->SetGCC(brightness);
  }
}

void DisplayManager::setBoardBrightness(int board_index, uint8_t brightness) {
  if (board_index >= 0 && board_index < num_boards_) {
    drivers_[board_index]->SetGCC(brightness);
  }
}

int DisplayManager::getBoardForPixel(int x) const {
  return x / board_width_;
}

void DisplayManager::mapPixelToBoard(int x, int y, int& board, int& local_x, int& local_y) const {
  // Flip and invert coordinates (hardware-specific transformation)
  int screen_x = total_width_ - x - 1;
  int screen_y = total_height_ - y - 1;
  
  // Determine board
  board = screen_x / board_width_;
  
  // Local coordinates within the board
  local_x = screen_x % board_width_;
  local_y = screen_y;
}

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

// Static I2C functions
uint8_t DisplayManager::i2c_read_reg(const uint8_t i2c_addr, const uint8_t reg_addr, uint8_t *buffer, const uint8_t length) {
  Wire.beginTransmission(i2c_addr);
  Wire.write(reg_addr);
  Wire.endTransmission();
  byte bytesRead = Wire.requestFrom(i2c_addr, length);
  for (int i = 0; i < bytesRead && i < length; i++) {
    buffer[i] = Wire.read();
  }
  return bytesRead;
}

uint8_t DisplayManager::i2c_write_reg(const uint8_t i2c_addr, const uint8_t reg_addr, const uint8_t *buffer, const uint8_t count) {
  Wire.beginTransmission(i2c_addr);
  Wire.write(reg_addr);
  Wire.write(buffer, count);
  return Wire.endTransmission();
}

// Private helper methods
void DisplayManager::initializeBuffers() {
  display_buffers_ = new uint8_t*[num_boards_];
  for (int i = 0; i < num_boards_; i++) {
    display_buffers_[i] = new uint8_t[PIXELS_PER_BOARD];
    std::fill_n(display_buffers_[i], PIXELS_PER_BOARD, 0);
  }
}

void DisplayManager::initializeDrivers() {
  // Allocate array of pointers, not array of objects (since IS31FL3733Driver can't be copied)
  drivers_ = new IS31FL3733Driver*[num_boards_];
  
  for (int i = 0; i < num_boards_; i++) {
    ADDR addr1, addr2;
    
    // Initialize with the appropriate addresses for each board
    switch (i) {
      case 0:
        addr1 = ADDR::GND; addr2 = ADDR::GND;
        break;
      case 1:
        addr1 = ADDR::VCC; addr2 = ADDR::VCC;
        break;
      case 2:
        addr1 = ADDR::SDA; addr2 = ADDR::SDA;
        break;
      default:
        addr1 = ADDR::GND; addr2 = ADDR::GND;
        break;
    }
    
    // Create each driver instance with new
    drivers_[i] = new IS31FL3733Driver(addr1, addr2, &i2c_read_reg, &i2c_write_reg);
  }
}

uint8_t DisplayManager::mapCoordinateToLED(int board, int local_x, int local_y) const {
  // Hardware-specific coordinate mapping for IS31FL3737 using IS31FL3733 driver
  
  // cs and sw are mapped to 12x12 LED driver space
  int cs = local_x;
  int sw = cs < 12 ? local_y : local_y + 6;
  cs = cs % 12;

  // The IS31FL3737 has 12 columns and 12 rows, but we are using the driver for
  // the IS31FL3733, which has 16 columns and 12 rows. Because of this,
  // CS7-CS12 (6..11) are off by 2, and must map to values 8-13.
  if (cs >= 6 && cs < 12) cs += 2;
  
  // The buffer is just an array, y*16+x
  return sw * 16 + cs;
}

ADDR DisplayManager::getBoardAddress(int board_index) const {
  // This is a placeholder - implement based on your hardware addressing scheme
  switch (board_index) {
    case 0: return ADDR::GND;
    case 1: return ADDR::VCC;
    case 2: return ADDR::SDA;
    default: return ADDR::GND;
  }
}
