#include "MeteorAnimation.h"

MeteorAnimation::MeteorAnimation(DisplayManager* display_manager)
  : display_manager_(display_manager)
  , num_meteors_(9)
  , num_stars_(24)
  , frame_interval_(50)  // 20 FPS
  , meteor_brightness_(150)
  , star_brightness_fast_(20)
  , star_brightness_slow_(8)
  , meteor_speed_multiplier_(1.0f)
  , star_speed_multiplier_(1.0f)
  , initialized_(false)
  , running_(false)
  , last_update_(0)
  , frame_count_(0)
  , meteor_positions_(nullptr)
  , star_positions_(nullptr)
{
}

MeteorAnimation::~MeteorAnimation() {
  cleanup();
}

void MeteorAnimation::initialize() {
  if (initialized_) {
    cleanup();
  }
  
  // Allocate memory for positions
  meteor_positions_ = new float[num_meteors_];
  star_positions_ = new float[num_stars_];
  
  initializePositions();
  
  initialized_ = true;
  running_ = true;
  frame_count_ = 0;
  
  Serial.printf("MeteorAnimation initialized with %d meteors and %d stars\n", num_meteors_, num_stars_);
}

void MeteorAnimation::update() {
  if (!initialized_ || !running_ || !display_manager_) {
    return;
  }
  
  unsigned long current_time = millis();
  if (current_time - last_update_ < frame_interval_) {
    return;
  }
  
  // Clear display
  display_manager_->clearBuffer();
  
  // Update and draw stars (background)
  updateStars();
  drawStars();
  
  // Update and draw meteors (foreground)
  updateMeteors();
  drawMeteors();
  
  // Update display
  display_manager_->updateDisplay();
  
  last_update_ = current_time;
  frame_count_++;
}

void MeteorAnimation::reset() {
  if (initialized_) {
    initializePositions();
    frame_count_ = 0;
  }
}

void MeteorAnimation::setNumMeteors(int num_meteors) {
  if (num_meteors != num_meteors_) {
    num_meteors_ = num_meteors;
    if (initialized_) {
      initialize(); // Reinitialize with new count
    }
  }
}

void MeteorAnimation::setNumStars(int num_stars) {
  if (num_stars != num_stars_) {
    num_stars_ = num_stars;
    if (initialized_) {
      initialize(); // Reinitialize with new count
    }
  }
}

void MeteorAnimation::setFrameRate(int fps) {
  if (fps > 0) {
    frame_interval_ = 1000 / fps;
  }
}

void MeteorAnimation::setBrightness(uint8_t meteor_brightness, uint8_t star_brightness_fast, uint8_t star_brightness_slow) {
  meteor_brightness_ = meteor_brightness;
  star_brightness_fast_ = star_brightness_fast;
  star_brightness_slow_ = star_brightness_slow;
}

void MeteorAnimation::setSpeed(float meteor_speed_multiplier, float star_speed_multiplier) {
  meteor_speed_multiplier_ = meteor_speed_multiplier;
  star_speed_multiplier_ = star_speed_multiplier;
}

void MeteorAnimation::initializePositions() {
  if (!meteor_positions_ || !star_positions_) return;
  
  int display_width = display_manager_->getWidth();
  
  // Initialize meteors at random negative positions to stagger their entry
  for (int i = 0; i < num_meteors_; i++) {
    meteor_positions_[i] = -10.0f - (rand() % 40);
  }
  
  // Initialize stars at random positions across the display width
  for (int i = 0; i < num_stars_; i++) {
    star_positions_[i] = (float)(rand() % (display_width + 20));
  }
}

void MeteorAnimation::updateMeteors() {
  if (!meteor_positions_) return;
  
  int display_width = display_manager_->getWidth();
  
  for (int m = 0; m < num_meteors_; m++) {
    // Meteor speed based on index - higher index = faster
    float meteor_speed = (1.0f + (m * 0.2f)) * meteor_speed_multiplier_;
    
    // Move meteor
    meteor_positions_[m] += meteor_speed;
    
    // Reset meteor when it goes off screen
    if (meteor_positions_[m] > display_width + 10) {
      meteor_positions_[m] = -15.0f - (m * 5); // Reset with staggered timing
    }
  }
}

void MeteorAnimation::updateStars() {
  if (!star_positions_) return;
  
  int display_width = display_manager_->getWidth();
  
  for (int i = 0; i < num_stars_; i++) {
    // Movement speed: odd stars faster, even stars slower
    float move_speed = ((i % 2 == 0) ? 0.2f : 0.5f) * star_speed_multiplier_;
    star_positions_[i] -= move_speed;
    
    // Reset star when it goes off screen
    if (star_positions_[i] < -10) {
      star_positions_[i] = display_width + 10;
    }
  }
}

void MeteorAnimation::drawMeteors() {
  if (!meteor_positions_) return;
  
  int display_width = display_manager_->getWidth();
  int display_height = display_manager_->getHeight();
  
  for (int m = 0; m < num_meteors_; m++) {
    int meteor_x = (int)meteor_positions_[m];
    int meteor_y = m % display_height; // Distribute meteors across all rows
    
    // Trail length based on speed - faster meteors have longer trails
    int trail_length = 3 + (m / 2); // 3-7 trails based on meteor index
    
    // Draw meteor trail (fading brightness)
    for (int trail = 0; trail < trail_length; trail++) {
      int trail_x = meteor_x - trail;
      if (trail_x >= 0 && trail_x < display_width) {
        uint8_t trail_brightness = meteor_brightness_ - (trail * 15);
        if (trail_brightness > star_brightness_slow_) {
          display_manager_->setPixel(trail_x, meteor_y, trail_brightness);
        }
      }
    }
  }
}

void MeteorAnimation::drawStars() {
  if (!star_positions_) return;
  
  int display_width = display_manager_->getWidth();
  int display_height = display_manager_->getHeight();
  
  for (int i = 0; i < num_stars_; i++) {
    int star_x = (int)star_positions_[i] % (display_width + 10);
    int star_y = (i % display_height); // Distribute stars across all rows
    
    if (star_x >= 0 && star_x < display_width) {
      // Odd stars move faster and are brighter, even stars move slower and are dimmer
      uint8_t star_brightness = (i % 2 == 0) ? star_brightness_slow_ : star_brightness_fast_;
      display_manager_->setPixel(star_x, star_y, star_brightness);
    }
  }
}

void MeteorAnimation::cleanup() {
  if (meteor_positions_) {
    delete[] meteor_positions_;
    meteor_positions_ = nullptr;
  }
  
  if (star_positions_) {
    delete[] star_positions_;
    star_positions_ = nullptr;
  }
  
  initialized_ = false;
}
