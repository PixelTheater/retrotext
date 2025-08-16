#ifndef METEOR_ANIMATION_H
#define METEOR_ANIMATION_H

#include <Arduino.h>
#include "DisplayManager.h"

class MeteorAnimation {
public:
  // Constructor
  MeteorAnimation(DisplayManager* display_manager);
  
  // Destructor
  ~MeteorAnimation();
  
  // Animation control
  void initialize();
  void update();
  void reset();
  
  // Configuration
  void setNumMeteors(int num_meteors);
  void setNumStars(int num_stars);
  void setFrameRate(int fps);
  void setBrightness(uint8_t meteor_brightness, uint8_t star_brightness_fast, uint8_t star_brightness_slow);
  void setSpeed(float meteor_speed_multiplier, float star_speed_multiplier);
  
  // Animation state
  bool isRunning() const { return running_; }
  void start() { running_ = true; }
  void stop() { running_ = false; }
  unsigned long getFrameCount() const { return frame_count_; }
  
private:
  DisplayManager* display_manager_;
  
  // Configuration
  int num_meteors_;
  int num_stars_;
  unsigned long frame_interval_;  // ms between frames
  uint8_t meteor_brightness_;
  uint8_t star_brightness_fast_;
  uint8_t star_brightness_slow_;
  float meteor_speed_multiplier_;
  float star_speed_multiplier_;
  
  // Animation state
  bool initialized_;
  bool running_;
  unsigned long last_update_;
  unsigned long frame_count_;
  
  // Animation data
  float* meteor_positions_;
  float* star_positions_;
  
  // Internal methods
  void initializePositions();
  void updateMeteors();
  void updateStars();
  void drawMeteors();
  void drawStars();
  void cleanup();
};

#endif // METEOR_ANIMATION_H
