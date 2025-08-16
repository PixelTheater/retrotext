#include "messages.h"

// Array of demonstration messages
String messages[NUM_MESSAGES] = {
  "Did you know? The speed of light is approximately 299,792 kilometers per second. The Earth revolves around the Sun at a speed of about 30 kilometers per second. A single teaspoon of honey represents the life work of 12 bees. The human brain contains approximately 86 billion neurons.",
  
  "Fascinating facts: Water expands by about 9% when it freezes. The Eiffel Tower can be 15 cm taller during the summer due to thermal expansion. Bananas are berries, but strawberries aren't. Octopuses have three hearts and blue blood. A day on Venus is longer than a year on Venus.",
  
  "Science wonders: There are more stars in the universe than grains of sand on all the Earth's beaches. The human body contains enough iron to make a 3-inch nail. Honey never spoils - archaeologists have found pots of honey in ancient Egyptian tombs that are over 3,000 years old and still edible.",
  
  "Amazing nature: A group of flamingos is called a flamboyance. Butterflies taste with their feet. A day on Mars is only 37 minutes longer than a day on Earth. The shortest war in history was between Britain and Zanzibar in 1896 - it lasted only 38 minutes.",
  
  "Space facts: The Sun makes up 99.86% of the mass of our solar system. If you could fold a piece of paper 42 times, it would reach the Moon. The Great Wall of China is not visible from space with the naked eye, contrary to popular belief. The first photograph of a black hole was taken in 2019.",
  
  "Human body marvels: Your heart beats about 100,000 times every day. The human body sheds about 600,000 particles of skin every hour. Your brain uses 20% of your body's total energy. The average person spends 6 months of their lifetime waiting for red lights to turn green.",
  
  "Technology insights: The first computer mouse was made of wood in 1964. The average smartphone today has more computing power than NASA had for the entire Apollo 11 mission. The first webcam was invented to monitor a coffee pot at Cambridge University. The average person checks their phone 150 times per day.",
  
  "Historical curiosities: The shortest war in history was between Britain and Zanzibar in 1896 - it lasted only 38 minutes. Cleopatra lived closer in time to the Moon landing than to the building of the Great Pyramid. The ancient Egyptians used honey as an antibiotic. The first oranges weren't orange - they were green."
};

void initializeMessages() {
  // Initialize random seed for message selection
  randomSeed(analogRead(0));
}

String getRandomMessage() {
  static int last_index = -1;
  int new_index;
  
  // Avoid repeating the same message immediately
  do {
    new_index = random(NUM_MESSAGES);
  } while (new_index == last_index && NUM_MESSAGES > 1);
  
  last_index = new_index;
  return messages[new_index];
}

String getRandomMessage(int& message_index) {
  static int last_index = -1;
  int new_index;
  
  // Avoid repeating the same message immediately
  do {
    new_index = random(NUM_MESSAGES);
  } while (new_index == last_index && NUM_MESSAGES > 1);
  
  last_index = new_index;
  message_index = new_index;
  return messages[new_index];
}

String getMessage(int index) {
  if (index >= 0 && index < NUM_MESSAGES) {
    return messages[index];
  }
  return "";
}

int getMessageCount() {
  return NUM_MESSAGES;
}
