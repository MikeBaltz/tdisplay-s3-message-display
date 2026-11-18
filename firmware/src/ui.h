#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "app_config.h"
#include "gif_messages.h"
#include "messages.h"
#include "time_service.h"
#include "weather_service.h"

// Culture: the living petri dish, one colony per message ever received.
enum class Screen : uint8_t { Clock, Dashboard, Messages, Animations, Culture };
constexpr uint8_t SCREEN_COUNT = 5;

class Ui {
 public:
  void begin(TFT_eSPI *display);
  void update(const TimeService &time, const WeatherService &weather,
              const MessageStore &messages, const GifMessageStore &gifMessages);
  // Tells the UI that the screen was drawn over by something else (a GIF, an
  // error page), so the next frame is painted directly without a transition.
  void invalidate();
  // Startup screen, shown until the device is connected and knows the time.
  void showBoot(const char *status);
  // Full-screen notice (setup mode, firmware update). Draws straight to the
  // display; call invalidate() afterwards to return to the normal screens.
  // corner: optional short text in a pill at the top right (a countdown).
  void showNotice(const char *title, const char *line1, const char *line2, bool animated,
                  const char *corner = "");
  void finishBoot();
  bool booting() const;
  // Connection state for the quiet "offline" indicator. Pass false only once a
  // problem has lasted long enough to be worth showing.
  void setConnectivity(bool wifiOk, bool brokerOk);
  // Which optional screens take part in the rotation (the clock always does),
  // and the place name for the weather screen.
  void configure(bool showWeather, bool showMessages, bool showAnimations, const char *city);
  void nextScreen();
  // Jumps straight to a screen, sliding it in (used for the idle Culture screen).
  void openScreen(Screen screen);
  // Total colonies on the Culture screen. A rise makes the newest one pop in.
  void setColonies(uint32_t count);
  void openMessages();
  void selectNewest(const GifMessageStore &gifMessages);
  void nextMessage(const MessageStore &messages, const GifMessageStore &gifMessages);
  void markCurrentRead(MessageStore &messages);
  Screen screen() const;
  uint8_t selectedMessage() const;
  bool selectedGif() const;

 private:
  void render(const TimeService &time, const WeatherService &weather,
              const MessageStore &messages, const GifMessageStore &gifMessages);
  void drawClock(const TimeService &time, const WeatherService &weather, uint8_t unread);
  void drawDashboard(const TimeService &time, const WeatherService &weather, uint8_t unread);
  void drawMessages(const MessageStore &messages);
  void drawAnimations(const GifMessageStore &gifMessages);
  void drawCulture(const TimeService &time, uint8_t unread);
  void drawPageDots();
  bool screenEnabled(Screen screen) const;
  void slideIn();

  // Drawing helpers. Every text call names its background so the anti-aliased
  // glyph edges blend into the card they sit on.
  void useFont(const uint8_t *font);
  void text(const String &value, int16_t x, int16_t y, uint8_t datum, uint16_t color,
            uint16_t background);
  void card(int16_t x, int16_t y, int16_t width, int16_t height, int16_t radius, uint16_t color);
  void drawBadge(int16_t x, int16_t y, uint8_t unread);
  void drawWeatherIcon(int weatherCode, bool isDay, int16_t x, int16_t y, float scale,
                       uint16_t background);
  void drawCloud(float x, float y, float scale, uint16_t color);
  // The logo: a petri dish with a few colonies, centred on x,y. A radius of 26
  // gives the 52 px startup mark.
  void drawPetriDish(float x, float y, float radius, uint16_t background, bool colonies = true);
  void drawSunOrMoon(bool isDay, float x, float y, float scale, uint16_t background);
  uint8_t drawWrapped(const String &value, int16_t x, int16_t y, uint16_t width,
                      uint8_t maxLines, uint16_t lineHeight, uint16_t color, uint16_t background,
                      bool draw = true, bool *truncated = nullptr);

  TFT_eSPI *display_ = nullptr;
  TFT_eSprite *sprite_ = nullptr;
  TFT_eSprite *previous_ = nullptr;  // outgoing frame during a slide
  const uint8_t *currentFont_ = nullptr;
  Screen screen_ = Screen::Clock;
  uint8_t selectedMessage_ = 0;
  uint8_t lastUnreadCount_ = 0;
  uint8_t lastMessageCount_ = 0;
  uint32_t lastMessageRevision_ = 0;
  uint32_t lastGifRevision_ = 0;
  uint32_t lastWeatherUpdatedAt_ = 0;
  String lastClockText_;
  String lastDateText_;
  bool lastColonOn_ = true;
  bool dirty_ = true;
  bool selectedGif_ = false;
  bool booting_ = true;
  uint32_t colonies_ = 0;
  bool coloniesKnown_ = false;
  uint32_t colonyBornAt_ = 0;
  uint32_t lastCultureFrameAt_ = 0;
  bool slidePending_ = false;
  bool wifiOk_ = true;
  bool brokerOk_ = true;
  bool showWeather_ = true;
  bool showMessages_ = true;
  bool showAnimations_ = true;
  String city_ = "London";
  String bootStatus_;
  uint8_t bootDot_ = 0xFF;
};
