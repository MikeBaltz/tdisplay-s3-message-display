#pragma once

#include <Arduino.h>

enum class ButtonEvent : uint8_t { None, Button1Short, Button2Short, Button2Long };

class ButtonHandler {
 public:
  void begin();
  ButtonEvent update();

 private:
  struct ButtonState {
    explicit ButtonState(uint8_t buttonPin) : pin(buttonPin) {}
    uint8_t pin;
    bool stablePressed = false;
    bool lastRawPressed = false;
    uint32_t changedAt = 0;
    uint32_t pressedAt = 0;
    bool longSent = false;
  };

  ButtonEvent updateButton(ButtonState &button, ButtonEvent shortEvent,
                           ButtonEvent longEvent);
  ButtonState button1_{0};
  ButtonState button2_{14};
};
