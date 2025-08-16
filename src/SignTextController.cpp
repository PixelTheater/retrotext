#include "SignTextController.h"
#include "DisplayManager.h"

namespace RetroText {

SignTextController::SignTextController(int display_width_chars, int char_width_pixels)
  : display_width_chars_(display_width_chars)
  , char_width_pixels_(char_width_pixels)
  , display_width_pixels_(display_width_chars * char_width_pixels)
  , char_spacing_pixels_(1)  // Default 1-pixel spacing for smooth scroll
  , current_font_(MODERN_FONT)
  , scroll_style_(SMOOTH)
  , scroll_speed_ms_(50)
  , default_brightness_(NORMAL)
  , message_("")
  , scroll_char_position_(0)
  , scroll_pixel_offset_(0)
  , last_update_time_(0)
  , scroll_complete_(false)
  , display_manager_(nullptr)
  , render_callback_(nullptr)
  , clear_callback_(nullptr)
  , draw_callback_(nullptr)
  , brightness_callback_(nullptr)
{
  // Initialize highlights as inactive
  for (int i = 0; i < MAX_HIGHLIGHTS; i++) {
    highlights_[i].active = false;
  }
}

SignTextController::~SignTextController() {
  // Nothing to clean up currently
}

void SignTextController::setFont(Font font) {
  current_font_ = font;
}

void SignTextController::setScrollStyle(ScrollStyle style) {
  scroll_style_ = style;
  resetScroll();
}

void SignTextController::setScrollSpeed(int speed_ms) {
  scroll_speed_ms_ = speed_ms;
}

void SignTextController::setCharacterSpacing(int spacing_pixels) {
  char_spacing_pixels_ = spacing_pixels;
}

void SignTextController::setBrightness(uint8_t default_brightness) {
  default_brightness_ = default_brightness;
}

void SignTextController::setMessage(String message) {
  message_ = message;
  resetScroll();
}

String SignTextController::getMessage() const {
  return message_;
}

void SignTextController::setScrollChars(int char_position) {
  scroll_char_position_ = char_position;
  scroll_pixel_offset_ = char_position * char_width_pixels_;
  scroll_complete_ = false;
}

void SignTextController::setScrollPixels(int pixel_offset) {
  scroll_pixel_offset_ = pixel_offset;
  scroll_char_position_ = pixel_offset / char_width_pixels_;
  scroll_complete_ = false;
}

void SignTextController::resetScroll() {
  scroll_char_position_ = 0;
  scroll_pixel_offset_ = 0;
  scroll_complete_ = false;
  last_update_time_ = 0;
}

void SignTextController::highlightText(int start_char, int end_char, uint8_t brightness) {
  // Find an inactive highlight slot
  for (int i = 0; i < MAX_HIGHLIGHTS; i++) {
    if (!highlights_[i].active) {
      highlights_[i].start_char = start_char;
      highlights_[i].end_char = end_char;
      highlights_[i].brightness = brightness;
      highlights_[i].active = true;
      break;
    }
  }
}

void SignTextController::clearHighlights() {
  for (int i = 0; i < MAX_HIGHLIGHTS; i++) {
    highlights_[i].active = false;
  }
}

void SignTextController::update() {
  if (message_.length() == 0) {
    return;
  }
  
  unsigned long current_time = millis();
  
  // Check if enough time has passed for update
  if (current_time - last_update_time_ < scroll_speed_ms_) {
    return;
  }
  
  last_update_time_ = current_time;
  
  switch (scroll_style_) {
    case SMOOTH:
      updateSmoothScroll();
      break;
    case CHARACTER:
      updateCharacterScroll();
      break;
    case STATIC:
      updateStaticDisplay();
      break;
  }
}

bool SignTextController::isComplete() const {
  return scroll_complete_;
}

void SignTextController::reset() {
  resetScroll();
}

int SignTextController::getCurrentCharPosition() const {
  return scroll_char_position_;
}

int SignTextController::getCurrentPixelOffset() const {
  return scroll_pixel_offset_;
}

bool SignTextController::isScrolling() const {
  return !scroll_complete_ && (scroll_style_ == SMOOTH || scroll_style_ == CHARACTER);
}

ScrollStyle SignTextController::getScrollStyle() const {
  return scroll_style_;
}

int SignTextController::calculateTotalScrollPixels() const {
  // Calculate how many pixels we need to scroll to show the entire message
  int effective_char_width = getEffectiveCharWidth();
  
  // Total message width in pixels (including spacing)
  int total_message_pixels = message_.length() * effective_char_width;
  
  // Display width in pixels
  int display_width_pixels = display_width_chars_ * effective_char_width;
  
  // We need to scroll until the end of the message is visible
  // Plus one character width to completely scroll off screen
  return total_message_pixels - display_width_pixels + effective_char_width;
}

int SignTextController::getEffectiveCharWidth() const {
  // For smooth scrolling, include spacing between characters
  // For character and static scrolling, use base character width
  if (scroll_style_ == SMOOTH) {
    return char_width_pixels_ + char_spacing_pixels_;
  }
  return char_width_pixels_;
}

void SignTextController::setDisplayManager(::DisplayManager* display_manager) {
  display_manager_ = display_manager;
}

void SignTextController::setRenderCallback(RenderCallback callback) {
  render_callback_ = callback;
}

void SignTextController::setClearCallback(ClearCallback callback) {
  clear_callback_ = callback;
}

void SignTextController::setDrawCallback(DrawCallback callback) {
  draw_callback_ = callback;
}

void SignTextController::setBrightnessCallback(BrightnessCallback callback) {
  brightness_callback_ = callback;
}

void SignTextController::updateSmoothScroll() {
  // Handle short messages that fit on screen
  if (message_.length() <= display_width_chars_) {
    updateStaticDisplay();
    scroll_complete_ = true;
    return;
  }
  
  // Calculate total scroll distance needed
  int total_scroll_pixels = calculateTotalScrollPixels();
  
  // Check if scrolling is complete
  if (scroll_pixel_offset_ >= total_scroll_pixels) {
    scroll_complete_ = true;
    return;
  }
  
  // Render current frame
  renderMessage();
  
  // Advance scroll position
  scroll_pixel_offset_++;
  scroll_char_position_ = scroll_pixel_offset_ / char_width_pixels_;
}

void SignTextController::updateCharacterScroll() {
  // Handle short messages that fit on screen
  if (message_.length() <= display_width_chars_) {
    updateStaticDisplay();
    scroll_complete_ = true;
    return;
  }
  
  // Calculate total character positions needed
  int total_char_positions = message_.length() - display_width_chars_ + 1;
  
  // Check if scrolling is complete
  if (scroll_char_position_ >= total_char_positions) {
    scroll_complete_ = true;
    return;
  }
  
  // Render current frame
  renderMessage();
  
  // Advance character position
  scroll_char_position_++;
  scroll_pixel_offset_ = scroll_char_position_ * char_width_pixels_;
}

void SignTextController::updateStaticDisplay() {
  renderMessage();
  scroll_complete_ = true;
}

void SignTextController::renderMessage() {
  // Use DisplayManager if available, otherwise fall back to callbacks
  if (display_manager_) {
    // Clear the display
    display_manager_->clearBuffer();
  } else if (!clear_callback_ || !render_callback_ || !draw_callback_) {
    return; // Can't render without callbacks or DisplayManager
  } else {
    // Clear the display using callback
    clear_callback_();
  }
  
  if (scroll_style_ == STATIC) {
    // Static display - show first characters that fit
    for (int i = 0; i < min((int)message_.length(), display_width_chars_); i++) {
      char c = message_.charAt(i);
      uint8_t ascii = c - 32;
      uint8_t brightness = getCharacterBrightness(c, i);
      int pixel_pos = i * char_width_pixels_;
      
      if (display_manager_) {
        // Get character pattern and draw using DisplayManager
        uint8_t pattern[6];
        for (int row = 0; row < 6; row++) {
          pattern[row] = display_manager_->getCharacterPattern(ascii, row, current_font_ == MODERN_FONT);
        }
        display_manager_->drawCharacter(pattern, pixel_pos, brightness);
      } else if (render_callback_) {
        render_callback_(ascii, pixel_pos, brightness, current_font_ == MODERN_FONT);
      }
    }
  } else {
    // Scrolling display - render visible characters
    int effective_char_width = getEffectiveCharWidth();
    for (int char_idx = 0; char_idx < message_.length(); char_idx++) {
      char c = message_.charAt(char_idx);
      uint8_t ascii = c - 32;
      int char_pixel_pos = (char_idx * effective_char_width) - scroll_pixel_offset_;
      
      // Only render characters that are at least partially visible
      if (shouldCharacterBeVisible(char_idx, char_pixel_pos)) {
        uint8_t brightness = getCharacterBrightness(c, char_idx);
        
        if (display_manager_) {
          // Get character pattern and draw using DisplayManager
          uint8_t pattern[6];
          for (int row = 0; row < 6; row++) {
            pattern[row] = display_manager_->getCharacterPattern(ascii, row, current_font_ == MODERN_FONT);
          }
          display_manager_->drawCharacter(pattern, char_pixel_pos, brightness);
        } else if (render_callback_) {
          render_callback_(ascii, char_pixel_pos, brightness, current_font_ == MODERN_FONT);
        }
      }
    }
  }
  
  // Draw the rendered frame
  if (display_manager_) {
    display_manager_->updateDisplay();
  } else if (draw_callback_) {
    draw_callback_();
  }
}

uint8_t SignTextController::getCharacterBrightness(char c, int char_index) {
  // Check for highlights first
  uint8_t highlight_brightness;
  if (isCharacterHighlighted(char_index, highlight_brightness)) {
    return highlight_brightness;
  }
  
  // Use custom brightness callback if available
  if (brightness_callback_) {
    return brightness_callback_(c, message_, char_index, false);
  }
  
  // Default brightness
  return default_brightness_;
}

bool SignTextController::isCharacterHighlighted(int char_index, uint8_t& highlight_brightness) {
  for (int i = 0; i < MAX_HIGHLIGHTS; i++) {
    if (highlights_[i].active && 
        char_index >= highlights_[i].start_char && 
        char_index <= highlights_[i].end_char) {
      highlight_brightness = highlights_[i].brightness;
      return true;
    }
  }
  return false;
}



bool SignTextController::shouldCharacterBeVisible(int char_index, int char_pixel_pos) const {
  // Character is visible if any part of it is on screen
  return char_pixel_pos > -char_width_pixels_ && char_pixel_pos < display_width_pixels_;
}

} // namespace RetroText
