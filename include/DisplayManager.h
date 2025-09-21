#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include "IS31FL373x.h"

class DisplayManager {
public:
  // Constructor - initializes the display with board configuration
  DisplayManager(int num_boards = 3, int board_width = 24, int board_height = 6);
  
  // Destructor
  ~DisplayManager();
  
  // Initialization
  bool initialize();
  bool verifyDrivers();
  void scanI2C();
  
  // Display properties
  int getWidth() const { return total_width_; }
  int getHeight() const { return total_height_; }
  int getCharacterWidth() const { return character_width_; }
  int getMaxCharacters() const { return max_characters_; }
  
  // Pixel operations
  void setPixel(int x, int y, uint8_t brightness);
  uint8_t getPixel(int x, int y) const;
  bool isValidPosition(int x, int y) const;
  
  // Buffer operations
  void clearBuffer();
  void fillBuffer(uint8_t brightness);
  void dimBuffer(uint8_t amount);
  void updateDisplay();  // Push buffer to hardware
  
  // Higher-level drawing operations
  void drawCharacter(uint8_t character_pattern[6], int x_offset, uint8_t brightness);
  void drawText(const String& text, int start_x, uint8_t brightness, bool use_alt_font = true);
  
  // Configuration
  void setGlobalBrightness(uint8_t brightness);
  void setBoardBrightness(int board_index, uint8_t brightness);
  
  // Hardware-specific methods
  int getBoardForPixel(int x) const;
  
  // Font access (temporary - should be moved to font manager later)
  uint8_t getCharacterPattern(uint8_t character, uint8_t row, bool use_alt_font = true) const;
  
private:
  // Hardware configuration
  int num_boards_;
  int board_width_;
  int board_height_;
  int total_width_;
  int total_height_;
  int character_width_;
  int max_characters_;
  
  // Hardware abstraction - IS31FL373x driver integration
  static const int PIXELS_PER_BOARD = 12 * 12;  // IS31FL3737 native configuration
  IS31FL3737* drivers_[4];  // Individual drivers for each board
  
  // Internal helper methods
  void initializeDrivers();
  void convertLogicalToPhysical(int logical_x, int logical_y, int& physical_x, int& physical_y);
};

#endif // DISPLAY_MANAGER_H
