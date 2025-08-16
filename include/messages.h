#ifndef MESSAGES_H
#define MESSAGES_H

#include <Arduino.h>

// Number of predefined messages
const int NUM_MESSAGES = 8;

// Array of demonstration messages
extern String messages[NUM_MESSAGES];

// Message management functions
void initializeMessages();
String getRandomMessage();
String getRandomMessage(int& message_index); // Returns index by reference
String getMessage(int index);
int getMessageCount();

#endif // MESSAGES_H
