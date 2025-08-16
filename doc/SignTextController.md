# SignTextController Library

A reusable C++ library for controlling LED matrix message signs with smooth scrolling, multiple fonts, and advanced text effects.

## Overview

The `SignTextController` class provides a clean, object-oriented interface for displaying text on LED matrix hardware. It abstracts away the low-level display operations and provides features like smooth scrolling, character-by-character scrolling, static display, text highlighting, and multiple font support.

## Features

- **Multiple Font Support**: Modern font (smooth) and Arduboy font (retro)
- **Three Scroll Styles**: Smooth pixel-by-pixel, character-by-character, and static display
- **Text Highlighting**: Highlight specific character ranges with custom brightness
- **Configurable Speed**: Adjustable scroll timing
- **Precise Positioning**: Jump to specific character or pixel positions
- **Callback System**: Hardware-agnostic through user-defined callbacks
- **Reusable**: Multiple instances can be created for different display purposes

## Quick Start

```cpp
#include "SignTextController.h"

// Create a controller for 18-character display with 4-pixel wide characters
RetroText::SignTextController* sign = new RetroText::SignTextController(18, 4);

// Configure the sign
sign->setFont(RetroText::MODERN_FONT);
sign->setScrollStyle(RetroText::SMOOTH);
sign->setScrollSpeed(45);

// Set up callbacks (implement these for your hardware)
sign->setRenderCallback(your_render_function);
sign->setClearCallback(your_clear_function);
sign->setDrawCallback(your_draw_function);
sign->setBrightnessCallback(your_brightness_function);

// Set message and start displaying
sign->setMessage("Hello World! This message will scroll smoothly.");

// In your main loop
while (!sign->isComplete()) {
    sign->update();
    delay(50);
}
```

## API Reference

### Constructor

```cpp
SignTextController(int display_width_chars = 18, int char_width_pixels = 4)
```

Creates a new controller instance.

### Configuration Methods

```cpp
void setFont(Font font);                    // MODERN_FONT or ARDUBOY_FONT
void setScrollStyle(ScrollStyle style);     // SMOOTH, CHARACTER, or STATIC
void setScrollSpeed(int speed_ms);          // Milliseconds between updates
void setBrightness(uint8_t default_brightness);
```

### Message Control

```cpp
void setMessage(String message);           // Set the text to display
String getMessage() const;                 // Get current message
```

### Scroll Position Control

```cpp
void setScrollChars(int char_position);    // Jump to character position
void setScrollPixels(int pixel_offset);    // Jump to pixel offset
void resetScroll();                        // Reset to beginning
```

### Text Highlighting

```cpp
void highlightText(int start_char, int end_char, uint8_t brightness);
void clearHighlights();
```

### Display Control

```cpp
void update();                             // Call this in your main loop
bool isComplete() const;                   // Check if scrolling is finished
void reset();                              // Reset position and state
```

### Status Methods

```cpp
int getCurrentCharPosition() const;
int getCurrentPixelOffset() const;
bool isScrolling() const;
```

### Callback Setup

```cpp
void setRenderCallback(RenderCallback callback);
void setClearCallback(ClearCallback callback);
void setDrawCallback(DrawCallback callback);
void setBrightnessCallback(BrightnessCallback callback);
```

## Callback Function Signatures

You need to implement these functions for your specific hardware:

```cpp
// Render a character at a specific pixel position
void render_character(uint8_t character, int pixel_offset, uint8_t brightness, bool use_alt_font);

// Clear the display buffer
void clear_display();

// Update the physical display with the buffer contents
void draw_display();

// Calculate brightness for a character based on context
uint8_t get_brightness(char c, String text, int char_pos, bool is_time_display);
```

## Constants

### Fonts
- `RetroText::MODERN_FONT` - Modern, smooth font
- `RetroText::ARDUBOY_FONT` - Retro, pixel-perfect font

### Scroll Styles
- `RetroText::SMOOTH` - Pixel-by-pixel smooth scrolling
- `RetroText::CHARACTER` - Character-by-character scrolling
- `RetroText::STATIC` - No scrolling, static display

### Brightness Levels
- `RetroText::BRIGHT` (150) - Bright text
- `RetroText::NORMAL` (70) - Normal text
- `RetroText::DIM` (20) - Dim text
- `RetroText::VERY_DIM` (8) - Very dim text

## Examples

### Basic Scrolling Text

```cpp
RetroText::SignTextController* sign = new RetroText::SignTextController(18, 4);
sign->setFont(RetroText::MODERN_FONT);
sign->setScrollStyle(RetroText::SMOOTH);
sign->setMessage("Your message here");
// Set up callbacks...
while (!sign->isComplete()) {
    sign->update();
    delay(50);
}
```

### Text with Highlighting

```cpp
sign->setMessage("The QUICK brown fox");
sign->highlightText(4, 8, RetroText::BRIGHT);  // Highlight "QUICK"
```

### Multiple Display Modes

```cpp
// Smooth scrolling mode
sign->setScrollStyle(RetroText::SMOOTH);
sign->setScrollSpeed(40);

// Character scrolling mode  
sign->setScrollStyle(RetroText::CHARACTER);
sign->setScrollSpeed(120);

// Static display mode
sign->setScrollStyle(RetroText::STATIC);
```

### Precise Positioning

```cpp
sign->setScrollChars(10);     // Start at 10th character
sign->setScrollPixels(25);    // Start at 25 pixel offset
```

## Integration with Existing Code

The library is designed to integrate easily with existing LED matrix code. You simply need to:

1. Implement the four callback functions that interface with your hardware
2. Create SignTextController instances as needed
3. Call `update()` in your main loop

The callbacks allow the library to work with any LED matrix hardware without modification.

## Memory Usage

- Each controller instance uses approximately 150-200 bytes of RAM
- No dynamic allocation during operation (only during construction)
- Supports up to 4 highlight spans per instance
- String message is stored as Arduino String object

## Thread Safety

The library is not inherently thread-safe. If using in a multi-threaded environment, ensure proper synchronization when calling methods.

## See Also

- `examples/SignTextController_Example.cpp` - Complete usage examples
- Your hardware-specific display functions for callback implementation
