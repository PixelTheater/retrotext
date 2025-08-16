#ifndef SIGN_TEXT_CONTROLLER_H
#define SIGN_TEXT_CONTROLLER_H

#include <Arduino.h>
#include <functional>

// Forward declaration
class DisplayManager;

namespace RetroText {

// Font constants
enum Font {
  MODERN_FONT = 0,
  ARDUBOY_FONT = 1
};

// Scroll style constants
enum ScrollStyle {
  SMOOTH = 0,
  CHARACTER = 1,
  STATIC = 2
};

// Brightness levels
enum Brightness {
  BRIGHT = 150,
  NORMAL = 70,
  DIM = 20,
  VERY_DIM = 8
};

// Highlight span structure
struct HighlightSpan {
  int start_char;
  int end_char;
  uint8_t brightness;
  bool active;
};

class SignTextController {
public:
  // Constructor
  SignTextController(int display_width_chars = 18, int char_width_pixels = 4);
  
  // Destructor
  ~SignTextController();
  
  // Configuration methods
  void setFont(Font font);
  void setScrollStyle(ScrollStyle style);
  void setScrollSpeed(int speed_ms);
  void setBrightness(uint8_t default_brightness);
  void setCharacterSpacing(int spacing_pixels);  // Set spacing between characters for smooth scroll
  
  // Message control
  void setMessage(String message);
  String getMessage() const;
  
  // Scroll position control
  void setScrollChars(int char_position);
  void setScrollPixels(int pixel_offset);
  void resetScroll();
  
  // Highlight control
  void highlightText(int start_char, int end_char, uint8_t brightness);
  void clearHighlights();
  
  // Display control
  void update();
  bool isComplete() const;
  void reset();
  
  // Status methods
  int getCurrentCharPosition() const;
  int getCurrentPixelOffset() const;
  bool isScrolling() const;
  ScrollStyle getScrollStyle() const;
  
  // Direct DisplayManager integration (preferred method)
  void setDisplayManager(::DisplayManager* display_manager);
  
  // Callback for actual display rendering (legacy method)
  typedef std::function<void(uint8_t character, int pixel_offset, uint8_t brightness, bool use_alt_font)> RenderCallback;
  typedef std::function<void()> ClearCallback;
  typedef std::function<void()> DrawCallback;
  typedef std::function<uint8_t(char c, String text, int char_pos, bool is_time_display)> BrightnessCallback;
  
  void setRenderCallback(RenderCallback callback);
  void setClearCallback(ClearCallback callback);
  void setDrawCallback(DrawCallback callback);
  void setBrightnessCallback(BrightnessCallback callback);

private:
  // Helper methods
  int calculateTotalScrollPixels() const;
  int getEffectiveCharWidth() const;  // Character width + spacing for current scroll style
  
  // Display parameters
  int display_width_chars_;
  int char_width_pixels_;
  int display_width_pixels_;
  int char_spacing_pixels_;  // Gap between characters for smooth scrolling
  
  // Configuration
  Font current_font_;
  ScrollStyle scroll_style_;
  int scroll_speed_ms_;
  uint8_t default_brightness_;
  
  // Message data
  String message_;
  
  // Scroll state
  int scroll_char_position_;
  int scroll_pixel_offset_;
  unsigned long last_update_time_;
  bool scroll_complete_;
  
  // Highlighting
  static const int MAX_HIGHLIGHTS = 4;
  HighlightSpan highlights_[MAX_HIGHLIGHTS];
  
  // Display integration
  ::DisplayManager* display_manager_;
  
  // Callbacks (legacy)
  RenderCallback render_callback_;
  ClearCallback clear_callback_;
  DrawCallback draw_callback_;
  BrightnessCallback brightness_callback_;
  
  // Internal methods
  void updateSmoothScroll();
  void updateCharacterScroll();
  void updateStaticDisplay();
  void renderMessage();
  uint8_t getCharacterBrightness(char c, int char_index);
  bool isCharacterHighlighted(int char_index, uint8_t& highlight_brightness);
  bool shouldCharacterBeVisible(int char_index, int char_pixel_pos) const;
};

} // namespace RetroText

#endif // SIGN_TEXT_CONTROLLER_H
