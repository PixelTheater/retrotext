/*
 * SignTextController Example
 * 
 * This example demonstrates how to use the RetroText::SignTextController class
 * to create reusable text display functionality for LED matrix message signs.
 * 
 * The controller abstracts away the low-level display operations and provides
 * a clean, easy-to-use interface for text scrolling and display.
 */

#include "SignTextController.h"

// Example callback functions (you would implement these for your hardware)
void render_character_example(uint8_t character, int pixel_offset, uint8_t brightness, bool use_alt_font) {
  // Your hardware-specific rendering code here
  // This would typically call write_character_at_offset() or similar
  Serial.printf("Render char %c at pixel %d, brightness %d, alt_font: %s\n", 
                character + 32, pixel_offset, brightness, use_alt_font ? "true" : "false");
}

void clear_display_example() {
  // Your hardware-specific clear display code here
  Serial.println("Clear display");
}

void draw_display_example() {
  // Your hardware-specific draw/update display code here
  Serial.println("Draw display");
}

uint8_t brightness_example(char c, String text, int char_pos, bool is_time_display) {
  // Your custom brightness logic here
  if (is_time_display && char_pos >= text.length() - 8) {
    return RetroText::BRIGHT;  // Time portion bright
  }
  if (c >= 'A' && c <= 'Z') {
    return RetroText::BRIGHT;  // Capitals bright
  }
  return RetroText::NORMAL;    // Everything else normal
}

void example_usage() {
  Serial.println("=== SignTextController Example ===");
  
  // Create a sign controller for an 18-character display with 4-pixel wide characters
  RetroText::SignTextController* message_sign = new RetroText::SignTextController(18, 4);
  
  // Configure the controller
  message_sign->setFont(RetroText::MODERN_FONT);
  message_sign->setScrollStyle(RetroText::SMOOTH);
  message_sign->setScrollSpeed(45);  // 45ms between updates
  message_sign->setBrightness(RetroText::NORMAL);
  
  // Set up callbacks
  message_sign->setRenderCallback(render_character_example);
  message_sign->setClearCallback(clear_display_example);
  message_sign->setDrawCallback(draw_display_example);
  message_sign->setBrightnessCallback(brightness_example);
  
  // Set the message to display
  message_sign->setMessage("Hello World! This is a LONG message that will scroll smoothly across the display.");
  
  // Add highlighting to make certain words stand out
  message_sign->highlightText(0, 5, RetroText::BRIGHT);  // "Hello" bright
  message_sign->highlightText(13, 17, RetroText::BRIGHT); // "This" bright
  message_sign->highlightText(21, 25, RetroText::BRIGHT); // "LONG" bright
  
  Serial.println("Starting message display...");
  Serial.printf("Message: %s\n", message_sign->getMessage().c_str());
  Serial.printf("Scrolling: %s\n", message_sign->isScrolling() ? "true" : "false");
  
  // Main display loop - call this repeatedly in your main loop
  int update_count = 0;
  while (!message_sign->isComplete() && update_count < 100) {  // Limit for example
    message_sign->update();
    
    // Print status every 10 updates
    if (update_count % 10 == 0) {
      Serial.printf("Update %d - Char pos: %d, Pixel offset: %d, Complete: %s\n",
                    update_count,
                    message_sign->getCurrentCharPosition(),
                    message_sign->getCurrentPixelOffset(),
                    message_sign->isComplete() ? "true" : "false");
    }
    
    update_count++;
    delay(50);  // Simulate time between updates
  }
  
  Serial.println("Message display complete!");
  
  // Example of jumping to a specific position
  Serial.println("\n=== Jump to Character Position 10 ===");
  message_sign->setScrollChars(10);
  message_sign->update();
  
  // Example of changing scroll style
  Serial.println("\n=== Switching to Character Scroll ===");
  message_sign->setScrollStyle(RetroText::CHARACTER);
  message_sign->setScrollSpeed(120);  // Slower for character scrolling
  message_sign->reset();  // Reset to beginning
  
  // Run character scrolling for a few updates
  for (int i = 0; i < 10; i++) {
    message_sign->update();
    delay(150);
  }
  
  // Example of static display
  Serial.println("\n=== Static Display Mode ===");
  message_sign->setMessage("SHORT MSG");  // Short message for static display
  message_sign->setScrollStyle(RetroText::STATIC);
  message_sign->reset();
  message_sign->update();
  
  // Clean up
  delete message_sign;
  Serial.println("\n=== Example Complete ===");
}

// Alternative usage example showing different configurations
void example_multiple_controllers() {
  Serial.println("\n=== Multiple Controller Example ===");
  
  // Create two different controllers with different configurations
  RetroText::SignTextController* modern_sign = new RetroText::SignTextController(18, 4);
  RetroText::SignTextController* retro_sign = new RetroText::SignTextController(18, 4);
  
  // Configure modern sign (smooth scrolling)
  modern_sign->setFont(RetroText::MODERN_FONT);
  modern_sign->setScrollStyle(RetroText::SMOOTH);
  modern_sign->setScrollSpeed(40);
  modern_sign->setRenderCallback(render_character_example);
  modern_sign->setClearCallback(clear_display_example);
  modern_sign->setDrawCallback(draw_display_example);
  modern_sign->setBrightnessCallback(brightness_example);
  modern_sign->setMessage("MODERN FONT - Smooth scrolling with advanced typography");
  
  // Configure retro sign (character scrolling)
  retro_sign->setFont(RetroText::ARDUBOY_FONT);
  retro_sign->setScrollStyle(RetroText::CHARACTER);
  retro_sign->setScrollSpeed(120);
  retro_sign->setRenderCallback(render_character_example);
  retro_sign->setClearCallback(clear_display_example);
  retro_sign->setDrawCallback(draw_display_example);
  retro_sign->setBrightnessCallback(brightness_example);
  retro_sign->setMessage("RETRO FONT - Classic character-by-character scrolling");
  
  Serial.println("Modern sign message: " + modern_sign->getMessage());
  Serial.println("Retro sign message: " + retro_sign->getMessage());
  
  // You could alternate between them or use them for different purposes
  Serial.println("Both controllers configured and ready to use!");
  
  // Clean up
  delete modern_sign;
  delete retro_sign;
}

// Example of advanced features
void example_advanced_features() {
  Serial.println("\n=== Advanced Features Example ===");
  
  RetroText::SignTextController* sign = new RetroText::SignTextController(18, 4);
  
  // Set up basic configuration
  sign->setFont(RetroText::MODERN_FONT);
  sign->setScrollStyle(RetroText::SMOOTH);
  sign->setRenderCallback(render_character_example);
  sign->setClearCallback(clear_display_example);
  sign->setDrawCallback(draw_display_example);
  sign->setBrightnessCallback(brightness_example);
  
  // Example of multiple highlights
  String message = "The QUICK brown FOX jumps over the LAZY dog";
  sign->setMessage(message);
  
  // Highlight multiple words
  sign->highlightText(4, 8, RetroText::BRIGHT);   // "QUICK"
  sign->highlightText(16, 18, RetroText::BRIGHT); // "FOX"
  sign->highlightText(35, 38, RetroText::BRIGHT); // "LAZY"
  
  Serial.println("Message with highlights: " + message);
  
  // Example of precise pixel positioning
  Serial.println("\n=== Pixel-level Positioning ===");
  sign->setScrollPixels(25);  // Start at 25 pixels offset
  sign->update();
  Serial.printf("Pixel offset: %d (equivalent to character %d)\n", 
                sign->getCurrentPixelOffset(), sign->getCurrentCharPosition());
  
  // Clear all highlights
  sign->clearHighlights();
  Serial.println("All highlights cleared");
  
  delete sign;
}

// This would be called from your main Arduino setup() or similar
void setup_example() {
  Serial.begin(115200);
  delay(1000);
  
  // Run all examples
  example_usage();
  example_multiple_controllers();
  example_advanced_features();
}
