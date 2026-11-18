#include "button_handler.h"

namespace {
constexpr uint32_t DEBOUNCE_MS = 35;
constexpr uint32_t LONG_PRESS_MS = 700;
}

void ButtonHandler::begin() {
  pinMode(button1_.pin, INPUT_PULLUP);
  pinMode(button2_.pin, INPUT_PULLUP);
}

ButtonEvent ButtonHandler::updateButton(ButtonState &button, ButtonEvent shortEvent,
                                        ButtonEvent longEvent) {
  const bool rawPressed = digitalRead(button.pin) == LOW;
  const uint32_t now = millis();
  if (rawPressed != button.lastRawPressed) {
    button.lastRawPressed = rawPressed;
    button.changedAt = now;
  }
  if (now - button.changedAt < DEBOUNCE_MS || rawPressed == button.stablePressed) {
    if (button.stablePressed && !button.longSent && now - button.pressedAt >= LONG_PRESS_MS) {
      button.longSent = true;
      return longEvent;
    }
    return ButtonEvent::None;
  }

  button.stablePressed = rawPressed;
  if (rawPressed) {
    button.pressedAt = now;
    button.longSent = false;
    return ButtonEvent::None;
  }
  return button.longSent ? ButtonEvent::None : shortEvent;
}

ButtonEvent ButtonHandler::update() {
  const ButtonEvent first = updateButton(button1_, ButtonEvent::Button1Short,
                                         ButtonEvent::None);
  return first == ButtonEvent::None
             ? updateButton(button2_, ButtonEvent::Button2Short, ButtonEvent::Button2Long)
             : first;
}
