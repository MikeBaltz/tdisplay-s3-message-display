#include "ui.h"

#include <WiFi.h>

#include "fonts.h"
#include "gift_config.h"

namespace {
constexpr uint16_t rgb(uint8_t red, uint8_t green, uint8_t blue) {
  return static_cast<uint16_t>(((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3));
}

// Palette: deep violet base, with warm, blue and rose cards for accents.
constexpr uint16_t BACKGROUND = rgb(0x0E, 0x0C, 0x18);
constexpr uint16_t CARD = rgb(0x1D, 0x18, 0x30);
constexpr uint16_t CARD_RAISED = rgb(0x2C, 0x24, 0x4A);
constexpr uint16_t CARD_WARM = rgb(0x2A, 0x1F, 0x16);
constexpr uint16_t CARD_BLUE = rgb(0x16, 0x23, 0x3A);
constexpr uint16_t CARD_ROSE = rgb(0x3A, 0x1A, 0x22);
constexpr uint16_t WHITE = rgb(0xFF, 0xFF, 0xFF);
constexpr uint16_t TEXT_SOFT = rgb(0xCF, 0xC7, 0xEA);
constexpr uint16_t TEXT_MUTED = rgb(0x9A, 0x90, 0xBE);
constexpr uint16_t ACCENT = rgb(0xA4, 0x7F, 0xFF);
constexpr uint16_t WARM = rgb(0xFF, 0xC4, 0x6B);
constexpr uint16_t WARM_TEXT = rgb(0xFF, 0xDD, 0xA8);
constexpr uint16_t BLUE_TEXT = rgb(0x8C, 0xB8, 0xFF);
constexpr uint16_t ROSE_TEXT = rgb(0xFF, 0x9C, 0xAE);
constexpr uint16_t ALERT = rgb(0xFF, 0x7D, 0x94);
constexpr uint16_t CLOUD = rgb(0xEE, 0xE8, 0xFB);
constexpr uint16_t RAIN = rgb(0x6F, 0xA8, 0xFF);
constexpr uint16_t MOON = rgb(0xE6, 0xDD, 0xFF);
// Petri-dish logo: glass rim, amber agar, colonies in three soft colours.
constexpr uint16_t DISH_GLASS = rgb(0xD9, 0xD2, 0xF2);
constexpr uint16_t AGAR = rgb(0xE8, 0xA9, 0x4E);
constexpr uint16_t AGAR_SHINE = rgb(0xF6, 0xD3, 0x96);
constexpr uint16_t COLONY_CREAM = rgb(0xFF, 0xF3, 0xDC);
constexpr uint16_t COLONY_ROSE = rgb(0xF2, 0x7E, 0x95);
constexpr uint16_t COLONY_LILAC = rgb(0x9E, 0x84, 0xF5);
// Dimmed colonies for the page indicator's inactive dots.
constexpr uint16_t DOT_CREAM = rgb(0x5E, 0x58, 0x58);
constexpr uint16_t DOT_ROSE = rgb(0x6A, 0x36, 0x48);
constexpr uint16_t DOT_LILAC = rgb(0x46, 0x3C, 0x78);

// Number of bytes in the UTF-8 character that starts with this byte.
uint8_t utf8Length(uint8_t lead) {
  if (lead < 0x80) return 1;
  if ((lead & 0xE0) == 0xC0) return 2;
  if ((lead & 0xF0) == 0xE0) return 3;
  if ((lead & 0xF8) == 0xF0) return 4;
  return 1;
}

String degreeText(float value) {
  String result = String(static_cast<long>(lroundf(value)));
  result += "\xC2\xB0";  // UTF-8 degree sign
  return result;
}
}  // namespace

void Ui::begin(TFT_eSPI *display) {
  display_ = display;
  sprite_ = new TFT_eSprite(display_);
  sprite_->setColorDepth(16);
  sprite_->setAttribute(PSRAM_ENABLE, true);
  if (sprite_->createSprite(SCREEN_WIDTH, SCREEN_HEIGHT) == nullptr) {
    Serial.println("Sprite allocation failed");
  }
  // Second frame for slide transitions. If it cannot be allocated the screens
  // simply switch without sliding.
  previous_ = new TFT_eSprite(display_);
  previous_->setColorDepth(16);
  previous_->setAttribute(PSRAM_ENABLE, true);
  if (previous_->createSprite(SCREEN_WIDTH, SCREEN_HEIGHT) == nullptr) {
    Serial.println("Transition sprite allocation failed; transitions disabled");
  }
  dirty_ = true;
}

void Ui::update(const TimeService &time, const WeatherService &weather,
               const MessageStore &messages, const GifMessageStore &gifMessages) {
  if (booting_) return;
  const String currentClock = time.timeText();
  const String currentDate = time.dateText();
  const bool clockChanged = currentClock != lastClockText_ || currentDate != lastDateText_;
  const uint8_t unreadCount = messages.unreadCount() + gifMessages.unreadCount();
  const bool unreadChanged = unreadCount != lastUnreadCount_;
  const bool messageCountChanged = messages.count() != lastMessageCount_;
  const bool messageChanged = messages.revision() != lastMessageRevision_;
  const bool gifChanged = gifMessages.revision() != lastGifRevision_;
  const bool weatherChanged = weather.data().updatedAt != lastWeatherUpdatedAt_;
  // The colon between hours and minutes blinks once a second.
  const bool colonOn = (millis() / 1000) % 2 == 0;
  const bool colonChanged = screen_ == Screen::Clock && colonOn != lastColonOn_;
  if (!dirty_ && !clockChanged && !unreadChanged && !messageCountChanged &&
      !messageChanged && !gifChanged && !weatherChanged && !colonChanged) {
    return;
  }
  if (screen_ == Screen::Clock && !dirty_ && !clockChanged && !unreadChanged &&
      !weatherChanged && !colonChanged) {
    return;
  }
  if (screen_ == Screen::Messages && !dirty_ && !messageCountChanged &&
      !messageChanged && !unreadChanged) {
    return;
  }
  if (screen_ == Screen::Animations && !dirty_ && !gifChanged && !unreadChanged) {
    return;
  }
  // The Culture screen is animated: about 12 frames a second while it is open.
  if (screen_ == Screen::Culture && !dirty_ && !unreadChanged && !clockChanged &&
      millis() - lastCultureFrameAt_ < 80) {
    return;
  }
  lastColonOn_ = colonOn;
  render(time, weather, messages, gifMessages);
}

void Ui::invalidate() {
  dirty_ = true;
  slidePending_ = false;
  bootStatus_ = "";  // a notice shown again later must be redrawn
}

bool Ui::screenEnabled(Screen screen) const {
  if (screen == Screen::Dashboard) return showWeather_;
  if (screen == Screen::Messages) return showMessages_;
  if (screen == Screen::Animations) return showAnimations_;
  return true;
}

void Ui::configure(bool showWeather, bool showMessages, bool showAnimations, const char *city) {
  if (showWeather == showWeather_ && showMessages == showMessages_ &&
      showAnimations == showAnimations_ && city_ == city) {
    return;
  }
  showWeather_ = showWeather;
  showMessages_ = showMessages;
  showAnimations_ = showAnimations;
  city_ = city;
  if (!screenEnabled(screen_)) screen_ = Screen::Clock;  // the open screen was switched off
  dirty_ = true;
}

void Ui::nextScreen() {
  const Screen before = screen_;
  do {
    screen_ = static_cast<Screen>((static_cast<uint8_t>(screen_) + 1) % SCREEN_COUNT);
  } while (!screenEnabled(screen_));
  if (screen_ == before) return;  // the clock is the only screen
  dirty_ = true;
  slidePending_ = true;
}

void Ui::openScreen(Screen screen) {
  if (!screenEnabled(screen) || screen == screen_) return;
  screen_ = screen;
  dirty_ = true;
  slidePending_ = true;
}

void Ui::setColonies(uint32_t count) {
  if (coloniesKnown_ && count == colonies_) return;
  if (coloniesKnown_ && count > colonies_) colonyBornAt_ = millis() | 1;  // the newest one pops in
  colonies_ = count;
  coloniesKnown_ = true;
  dirty_ = true;
}

void Ui::openMessages() {
  if (!showMessages_) return;
  if (screen_ != Screen::Messages) slidePending_ = true;
  screen_ = Screen::Messages;
  selectedMessage_ = 0;
  dirty_ = true;
}

bool Ui::booting() const { return booting_; }

void Ui::finishBoot() {
  if (!booting_) return;
  booting_ = false;
  dirty_ = true;
  slidePending_ = true;  // the clock slides in over the startup screen
}

void Ui::setConnectivity(bool wifiOk, bool brokerOk) {
  if (wifiOk == wifiOk_ && brokerOk == brokerOk_) return;
  wifiOk_ = wifiOk;
  brokerOk_ = brokerOk;
  dirty_ = true;
}

void Ui::showBoot(const char *status) { showNotice(GIFT_BOOT_TITLE, status, "", true); }

void Ui::showNotice(const char *title, const char *line1, const char *line2, bool animated,
                    const char *corner) {
  if (sprite_ == nullptr || !sprite_->created()) return;
  // Three dots take turns lighting up while the device is busy.
  const uint8_t dot = animated ? (millis() / 350) % 3 : 3;
  const String key = String(title) + '|' + line1 + '|' + line2 + '|' + corner;
  if (bootStatus_ == key && dot == bootDot_) return;
  bootStatus_ = key;
  bootDot_ = dot;
  const bool twoLines = line2[0] != '\0';

  sprite_->fillSprite(BACKGROUND);
  // The mark: a petri dish, 52 px across, where the old tile was.
  drawPetriDish(160, 44, 26, BACKGROUND);
  if (corner[0] != '\0') {
    useFont(FONT_TEXT);
    const int16_t pillWidth = sprite_->textWidth(corner) + 22;
    card(SCREEN_WIDTH - 10 - pillWidth, 8, pillWidth, 24, 12, CARD_ROSE);
    text(corner, SCREEN_WIDTH - 10 - pillWidth / 2, 20, MC_DATUM, ROSE_TEXT, CARD_ROSE);
  }
  useFont(FONT_TITLE);
  // A title too long for the title font (a long name) uses the text font.
  if (sprite_->textWidth(title) > SCREEN_WIDTH - 16) useFont(FONT_TEXT);
  text(title, SCREEN_WIDTH / 2, 94, MC_DATUM, WHITE, BACKGROUND);
  useFont(FONT_TEXT);
  text(line1, SCREEN_WIDTH / 2, 121, MC_DATUM, twoLines ? TEXT_SOFT : TEXT_MUTED, BACKGROUND);
  if (twoLines) {
    text(line2, SCREEN_WIDTH / 2, 142, MC_DATUM, TEXT_MUTED, BACKGROUND);
  } else if (animated) {
    for (uint8_t index = 0; index < 3; ++index) {
      sprite_->fillSmoothCircle(148 + index * 12, 148, 3, index == dot ? ACCENT : CARD_RAISED,
                                BACKGROUND);
    }
  }
  sprite_->pushSprite(0, 0);
}

void Ui::selectNewest(const GifMessageStore &gifMessages) {
  (void)gifMessages;
  selectedGif_ = false;
  selectedMessage_ = 0;
  dirty_ = true;
}

void Ui::nextMessage(const MessageStore &messages, const GifMessageStore &gifMessages) {
  if (screen_ != Screen::Messages) {
    openMessages();
    selectNewest(gifMessages);
    return;
  }
  if (messages.count() > 0) {
    selectedMessage_ = (selectedMessage_ + 1) % messages.count();
  }
  dirty_ = true;
}

void Ui::markCurrentRead(MessageStore &messages) {
  messages.markRead(selectedMessage_);
  dirty_ = true;
}

Screen Ui::screen() const { return screen_; }

uint8_t Ui::selectedMessage() const { return selectedMessage_; }

bool Ui::selectedGif() const { return selectedGif_; }

void Ui::render(const TimeService &time, const WeatherService &weather,
                const MessageStore &messages, const GifMessageStore &gifMessages) {
  if (sprite_ == nullptr || !sprite_->created()) return;
  dirty_ = false;
  const uint8_t unread = messages.unreadCount() + gifMessages.unreadCount();
  lastUnreadCount_ = unread;
  lastMessageCount_ = messages.count();
  lastMessageRevision_ = messages.revision();
  lastGifRevision_ = gifMessages.revision();
  lastWeatherUpdatedAt_ = weather.data().updatedAt;
  lastClockText_ = time.timeText();
  lastDateText_ = time.dateText();
  // Keep a copy of the frame that is on screen now, so it can slide out.
  const bool slide = slidePending_ && previous_ != nullptr && previous_->created();
  slidePending_ = false;
  if (slide) {
    memcpy(previous_->getPointer(), sprite_->getPointer(),
           static_cast<size_t>(SCREEN_WIDTH) * SCREEN_HEIGHT * 2);
  }
  sprite_->fillSprite(BACKGROUND);
  if (screen_ == Screen::Clock) drawClock(time, weather, unread);
  else if (screen_ == Screen::Dashboard) drawDashboard(time, weather, unread);
  else if (screen_ == Screen::Messages) drawMessages(messages);
  else if (screen_ == Screen::Animations) drawAnimations(gifMessages);
  else drawCulture(time, unread);
  drawPageDots();
  if (slide) slideIn();
  else sprite_->pushSprite(0, 0);
}

// Slides the new frame in from the right while the old one leaves to the left.
// Takes roughly a fifth of a second and ends on the complete new frame.
void Ui::slideIn() {
  constexpr uint8_t STEPS = 9;
  for (uint8_t step = 1; step <= STEPS; ++step) {
    const float progress = static_cast<float>(step) / STEPS;
    const float eased = 1.0f - (1.0f - progress) * (1.0f - progress) * (1.0f - progress);
    const int16_t offset = static_cast<int16_t>(SCREEN_WIDTH * eased + 0.5f);
    if (offset < SCREEN_WIDTH) previous_->pushSprite(-offset, 0);
    sprite_->pushSprite(SCREEN_WIDTH - offset, 0);
  }
}

// Four small dots along the bottom edge show which screen is active. A rose
// dot in the corner appears only while Wi-Fi or the message server is down.
void Ui::drawPageDots() {
  constexpr int16_t Y = 165;
  uint8_t enabled = 0;
  for (uint8_t index = 0; index < SCREEN_COUNT; ++index) {
    if (screenEnabled(static_cast<Screen>(index))) ++enabled;
  }
  // One wide amber "agar" dot for the open screen, and a small dimmed colony
  // (cream, rose or lilac) for each of the others.
  static const uint16_t colonyDots[SCREEN_COUNT] = {DOT_CREAM, DOT_ROSE, DOT_LILAC, DOT_CREAM,
                                                    DOT_ROSE};
  int16_t x = SCREEN_WIDTH / 2 - (14 + (enabled - 1) * 13) / 2;
  for (uint8_t index = 0; enabled > 1 && index < SCREEN_COUNT; ++index) {
    if (!screenEnabled(static_cast<Screen>(index))) continue;
    if (index == static_cast<uint8_t>(screen_)) {
      sprite_->fillSmoothRoundRect(x, Y - 2, 14, 5, 2, AGAR, BACKGROUND);
      x += 14 + 8;
    } else {
      sprite_->fillSmoothCircle(x + 2, Y, 2, colonyDots[index], BACKGROUND);
      x += 5 + 8;
    }
  }
  if (!wifiOk_ || !brokerOk_) sprite_->fillSmoothCircle(16, Y, 3, ALERT, BACKGROUND);
}

// ---------------------------------------------------------------- helpers --

void Ui::useFont(const uint8_t *font) {
  if (font == currentFont_) return;
  if (currentFont_ != nullptr) sprite_->unloadFont();
  sprite_->loadFont(font);
  currentFont_ = font;
}

void Ui::text(const String &value, int16_t x, int16_t y, uint8_t datum, uint16_t color,
              uint16_t background) {
  sprite_->setTextDatum(datum);
  sprite_->setTextColor(color, background);
  sprite_->drawString(value, x, y);
}

void Ui::card(int16_t x, int16_t y, int16_t width, int16_t height, int16_t radius,
              uint16_t color) {
  sprite_->fillSmoothRoundRect(x, y, width, height, radius, color, BACKGROUND);
}

void Ui::drawBadge(int16_t x, int16_t y, uint8_t unread) {
  if (unread == 0) return;
  // No background colour is given, so the edge blends with whatever is underneath.
  sprite_->fillSmoothCircle(x, y, 10, ALERT);
  useFont(FONT_TEXT);
  text(unread > 9 ? String("9+") : String(unread), x, y, MC_DATUM, BACKGROUND, ALERT);
}

void Ui::drawCloud(float x, float y, float scale, uint16_t color) {
  sprite_->fillSmoothCircle(x - 10 * scale, y + 3 * scale, 9 * scale, color);
  sprite_->fillSmoothCircle(x + 2 * scale, y - 4 * scale, 13 * scale, color);
  sprite_->fillSmoothCircle(x + 13 * scale, y + 4 * scale, 8 * scale, color);
  sprite_->fillSmoothRoundRect(x - 19 * scale, y + 1 * scale, 40 * scale, 12 * scale,
                               6 * scale, color);
}

void Ui::drawPetriDish(float x, float y, float radius, uint16_t background, bool colonies) {
  const float unit = radius / 26.0f;  // all sizes below are for the 52 px mark
  // Glass rim, a thin dark gap for the dish wall, then the agar.
  sprite_->fillSmoothCircle(x, y, radius, DISH_GLASS, background);
  sprite_->fillSmoothCircle(x, y, radius - 2.5f * unit, background, DISH_GLASS);
  const float agar = radius - 4.5f * unit;
  sprite_->fillSmoothCircle(x, y, agar, AGAR, background);
  // A soft reflection along the upper left of the agar.
  if (radius >= 14) {
    sprite_->drawSmoothArc(x, y, agar - 2.5f * unit, agar - 4.5f * unit, 105, 165, AGAR_SHINE,
                           AGAR, true);
  }
  // Colonies: offset from the centre and radius, as fractions of the dish radius.
  struct Colony {
    float dx, dy, r;
    uint16_t color;
  };
  if (!colonies) return;
  static const Colony colonyList[] = {
      {-0.30f, -0.22f, 0.17f, COLONY_CREAM}, {0.30f, -0.36f, 0.10f, COLONY_ROSE},
      {0.40f, 0.12f, 0.14f, COLONY_LILAC},   {-0.08f, 0.40f, 0.12f, COLONY_CREAM},
      {-0.47f, 0.22f, 0.08f, COLONY_ROSE},   {0.08f, 0.00f, 0.07f, COLONY_LILAC},
      {0.30f, 0.46f, 0.06f, COLONY_ROSE},
  };
  // Very small dishes (inline icons) keep only the three largest colonies.
  const uint8_t count = radius < 14 ? 3 : sizeof(colonyList) / sizeof(colonyList[0]);
  for (uint8_t index = 0; index < count; ++index) {
    const Colony &colony = colonyList[index];
    sprite_->fillSmoothCircle(x + colony.dx * radius, y + colony.dy * radius,
                              max(1.5f, colony.r * radius), colony.color, AGAR);
  }
}

void Ui::drawSunOrMoon(bool isDay, float x, float y, float scale, uint16_t background) {
  if (!isDay) {
    // A crescent: a disc with a second disc in the card colour cut out of it.
    sprite_->fillSmoothCircle(x, y, 13 * scale, MOON);
    sprite_->fillSmoothCircle(x + 7 * scale, y - 5 * scale, 11 * scale, background);
    return;
  }
  sprite_->fillSmoothCircle(x, y, 10 * scale, WARM);
  for (uint8_t ray = 0; ray < 8; ++ray) {
    const float angle = ray * 0.7853982f;
    sprite_->drawWideLine(x + cosf(angle) * 15 * scale, y + sinf(angle) * 15 * scale,
                          x + cosf(angle) * 20 * scale, y + sinf(angle) * 20 * scale,
                          max(2.0f, 3.5f * scale), WARM);
  }
}

// Draws a weather icon centred on x,y. Scale 1 is roughly 46 px wide.
void Ui::drawWeatherIcon(int weatherCode, bool isDay, int16_t x, int16_t y, float scale,
                         uint16_t background) {
  const float stroke = max(2.0f, 3.5f * scale);
  if (weatherCode < 0) return;
  if (weatherCode <= 1) {  // clear, mostly clear
    drawSunOrMoon(isDay, x, y, scale * 1.1f, background);
    return;
  }
  if (weatherCode == 2) {  // partly cloudy
    drawSunOrMoon(isDay, x + 8 * scale, y - 8 * scale, scale * 0.75f, background);
    drawCloud(x - 3 * scale, y + 5 * scale, scale * 0.85f, CLOUD);
    return;
  }
  if (weatherCode == 3) {  // overcast
    drawCloud(x, y, scale, CLOUD);
    return;
  }
  if (weatherCode <= 48) {  // fog
    for (int8_t row = -1; row <= 1; ++row) {
      const float inset = row == 0 ? 0 : 5 * scale;
      sprite_->drawWideLine(x - 18 * scale + inset, y + row * 10 * scale,
                            x + 18 * scale - inset, y + row * 10 * scale, stroke, CLOUD);
    }
    return;
  }
  const bool snow = (weatherCode >= 71 && weatherCode <= 77) || weatherCode == 85 ||
                    weatherCode == 86;
  const bool storm = weatherCode >= 95;
  drawCloud(x, y - 6 * scale, scale * 0.9f, CLOUD);
  if (storm) {
    sprite_->fillTriangle(x + 3 * scale, y + 4 * scale, x - 7 * scale, y + 15 * scale,
                          x + 1 * scale, y + 15 * scale, WARM);
    sprite_->fillTriangle(x - 2 * scale, y + 12 * scale, x + 7 * scale, y + 12 * scale,
                          x - 4 * scale, y + 24 * scale, WARM);
    return;
  }
  for (int8_t drop = -1; drop <= 1; ++drop) {
    const float dropX = x + drop * 11 * scale;
    if (snow) {
      sprite_->fillSmoothCircle(dropX, y + 15 * scale, max(1.5f, 2.6f * scale), WHITE);
    } else {
      sprite_->drawWideLine(dropX + 2 * scale, y + 11 * scale, dropX - 2 * scale,
                            y + 19 * scale, stroke * 0.85f, RAIN);
    }
  }
}

// Word-wraps UTF-8 text. Returns the number of lines drawn. Text that does not
// fit ends with an ellipsis.
uint8_t Ui::drawWrapped(const String &value, int16_t x, int16_t y, uint16_t width,
                        uint8_t maxLines, uint16_t lineHeight, uint16_t color,
                        uint16_t background, bool draw, bool *truncated) {
  constexpr uint8_t LINE_LIMIT = 8;
  if (maxLines > LINE_LIMIT) maxLines = LINE_LIMIT;
  String lines[LINE_LIMIT];
  uint8_t lineCount = 0;
  String line;
  bool cut = false;
  const uint16_t length = value.length();
  uint16_t index = 0;

  auto flush = [&]() {
    line.trim();
    lines[lineCount++] = line;
    line = "";
  };

  while (index < length) {
    if (lineCount >= maxLines) {
      cut = true;
      break;
    }
    const char current = value[index];
    if (current == '\n') {
      flush();
      ++index;
      continue;
    }
    if (current == ' ') {
      if (line.length() > 0) line += ' ';
      ++index;
      continue;
    }
    // Collect the next word, one whole UTF-8 character at a time.
    uint16_t wordEnd = index;
    while (wordEnd < length && value[wordEnd] != ' ' && value[wordEnd] != '\n') {
      wordEnd += utf8Length(static_cast<uint8_t>(value[wordEnd]));
    }
    if (wordEnd > length) wordEnd = length;
    const String word = value.substring(index, wordEnd);
    if (sprite_->textWidth(line + word) <= width) {
      line += word;
      index = wordEnd;
      continue;
    }
    if (line.length() > 0) {
      flush();
      continue;  // retry the same word on a fresh line
    }
    // A single word wider than the card: break it between characters.
    while (index < wordEnd) {
      const uint8_t step = utf8Length(static_cast<uint8_t>(value[index]));
      const String piece = value.substring(index, min(static_cast<uint16_t>(index + step), length));
      if (line.length() > 0 && sprite_->textWidth(line + piece) > width) break;
      line += piece;
      index += step;
    }
    if (index < wordEnd) flush();
  }
  if (line.length() > 0) {
    if (lineCount < maxLines) flush();
    else cut = true;
  }
  if (truncated != nullptr) *truncated = cut;
  if (!draw) return lineCount;

  if (cut && lineCount > 0) {
    // Shorten the last line, whole characters at a time, until "..." fits.
    const String ellipsis = "\xE2\x80\xA6";
    String &last = lines[lineCount - 1];
    while (last.length() > 0 && sprite_->textWidth(last + ellipsis) > width) {
      uint16_t cut = last.length() - 1;
      while (cut > 0 && (static_cast<uint8_t>(last[cut]) & 0xC0) == 0x80) --cut;
      last.remove(cut);
    }
    last.trim();
    last += ellipsis;
  }
  for (uint8_t row = 0; row < lineCount; ++row) {
    text(lines[row], x, y + row * lineHeight, TL_DATUM, color, background);
  }
  return lineCount;
}

// ---------------------------------------------------------------- screens --

void Ui::drawClock(const TimeService &time, const WeatherService &weather, uint8_t unread) {
  card(10, 10, 300, 108, 22, CARD);

  // Each digit sits in a fixed-width cell so the time does not shift sideways
  // when a narrow "1" replaces a wide "0".
  useFont(FONT_CLOCK);
  const String current = time.timeText();  // "HH:MM" or "--:--"
  const int16_t cell = sprite_->textWidth("0") + 2;
  const int16_t colonWidth = sprite_->textWidth(":") + 6;
  int16_t x = (SCREEN_WIDTH - (4 * cell + colonWidth)) / 2;
  const int16_t centerY = 64;
  const bool twelveHour = !time.use24Hour() && current[0] != '-';
  for (uint8_t index = 0; index < 5 && index < current.length(); ++index) {
    if (index == 2) {
      if (lastColonOn_) text(":", x + colonWidth / 2, centerY, MC_DATUM, ACCENT, CARD);
      x += colonWidth;
      continue;
    }
    // In 12-hour mode "08:37" is shown as " 8:37".
    if (!(twelveHour && index == 0 && current[0] == '0')) {
      text(String(current[index]), x + cell / 2, centerY, MC_DATUM, WHITE, CARD);
    }
    x += cell;
  }
  if (twelveHour) {
    useFont(FONT_TEXT);
    text(time.isPm() ? "PM" : "AM", 26, 18, TL_DATUM, TEXT_MUTED, CARD);
  }
  drawBadge(300, 20, unread);

  useFont(FONT_TEXT);
  if (!wifiOk_ || !brokerOk_) {
    // Explain a stale screen instead of showing the date.
    card(10, 128, 146, 32, 16, CARD_ROSE);
    text(!wifiOk_ ? "No Wi-Fi" : "Server offline", 83, 144, MC_DATUM, ROSE_TEXT, CARD_ROSE);
  } else {
    card(10, 128, 146, 32, 16, CARD);
    text(time.dateText(), 83, 144, MC_DATUM, TEXT_SOFT, CARD);
  }

  card(164, 128, 146, 32, 16, CARD_WARM);
  const WeatherData &data = weather.data();
  if (!data.valid) {
    text("Weather loading", 237, 144, MC_DATUM, WARM_TEXT, CARD_WARM);
    return;
  }
  String summary = degreeText(data.currentTemperature) + " " + weather.condition();
  constexpr int16_t ICON_SPACE = 30;
  if (sprite_->textWidth(summary) > 146 - ICON_SPACE - 20) summary = degreeText(data.currentTemperature);
  const int16_t groupWidth = ICON_SPACE + sprite_->textWidth(summary);
  const int16_t groupX = 237 - groupWidth / 2;
  drawWeatherIcon(data.weatherCode, data.isDay, groupX + 11, 144, 0.42f, CARD_WARM);
  useFont(FONT_TEXT);
  text(summary, groupX + ICON_SPACE, 144, ML_DATUM, WARM_TEXT, CARD_WARM);
}

void Ui::drawDashboard(const TimeService &time, const WeatherService &weather, uint8_t unread) {
  const WeatherData &data = weather.data();

  card(10, 10, 186, 150, 22, CARD);
  useFont(FONT_TEXT);
  const String now = time.shortTime();
  text(now, 180, 22, TR_DATUM, TEXT_SOFT, CARD);
  // The place name gets the room left of the time; long names end with "...".
  const int16_t nameRoom = 180 - 26 - sprite_->textWidth(now) - 10;
  String name = city_;
  if (sprite_->textWidth(name) > nameRoom) {
    const String ellipsis = "\xE2\x80\xA6";
    while (name.length() > 0 && sprite_->textWidth(name + ellipsis) > nameRoom) {
      uint16_t cut = name.length() - 1;
      while (cut > 0 && (static_cast<uint8_t>(name[cut]) & 0xC0) == 0x80) --cut;
      name.remove(cut);
    }
    name.trim();
    name += ellipsis;
  }
  text(name, 26, 22, TL_DATUM, TEXT_MUTED, CARD);
  text(data.valid ? weather.condition() : String("Weather unavailable"), 26, 133, TL_DATUM,
       TEXT_SOFT, CARD);
  useFont(FONT_TEMP);
  text(data.valid ? degreeText(data.currentTemperature) : String("--"), 24, 88, ML_DATUM,
       data.valid ? WHITE : TEXT_MUTED, CARD);

  card(204, 10, 106, 72, 22, CARD_WARM);
  if (data.valid) drawWeatherIcon(data.weatherCode, data.isDay, 257, 46, 1.15f, CARD_WARM);
  drawBadge(300, 20, unread);

  card(204, 90, 50, 70, 18, CARD_BLUE);
  card(260, 90, 50, 70, 18, CARD_ROSE);
  useFont(FONT_TEXT);
  text("Low", 229, 108, MC_DATUM, BLUE_TEXT, CARD_BLUE);
  text("High", 285, 108, MC_DATUM, ROSE_TEXT, CARD_ROSE);
  useFont(FONT_VALUE);
  text(data.valid ? degreeText(data.minimumTemperature) : String("--"), 229, 136, MC_DATUM, WHITE,
       CARD_BLUE);
  text(data.valid ? degreeText(data.maximumTemperature) : String("--"), 285, 136, MC_DATUM, WHITE,
       CARD_ROSE);
}

void Ui::drawMessages(const MessageStore &messages) {
  const uint8_t count = messages.count();
  const RadioMessage *message = messages.at(selectedMessage_);
  useFont(FONT_TEXT);

  // Header: a small petri dish, then who sent it (or when it arrived), and a
  // pill with the position, rose while the message is unread.
  drawPetriDish(22, 20, 9, BACKGROUND);
  int16_t headerRoom = SCREEN_WIDTH - 38 - 10;
  if (message != nullptr) {
    const String page = (message->unread ? String("New \xC2\xB7 ") : String("")) +
                        String(selectedMessage_ + 1) + " / " + String(count);
    const int16_t pillWidth = sprite_->textWidth(page) + 22;
    const uint16_t pill = message->unread ? CARD_ROSE : CARD_RAISED;
    card(SCREEN_WIDTH - 10 - pillWidth, 8, pillWidth, 24, 12, pill);
    text(page, SCREEN_WIDTH - 10 - pillWidth / 2, 20, MC_DATUM,
         message->unread ? ROSE_TEXT : TEXT_SOFT, pill);
    headerRoom -= pillWidth + 8;
  }
  // A friend's message shows their name with the time ("Anna · 20:31").
  String heading = String("Inbox");
  if (message != nullptr) {
    heading = message->sender;
    if (heading.length() > 0 && message->receivedTime.length() > 0) heading += " \xC2\xB7 ";
    heading += message->receivedTime;
  }
  if (sprite_->textWidth(heading) > headerRoom) {
    const String ellipsis = "\xE2\x80\xA6";
    while (heading.length() > 0 && sprite_->textWidth(heading + ellipsis) > headerRoom) {
      uint16_t cut = heading.length() - 1;
      while (cut > 0 && (static_cast<uint8_t>(heading[cut]) & 0xC0) == 0x80) --cut;
      heading.remove(cut);
    }
    heading.trim();
    heading += ellipsis;
  }
  text(heading, 38, 20, ML_DATUM,
       message != nullptr && message->unread ? ROSE_TEXT : TEXT_MUTED, BACKGROUND);

  constexpr int16_t CARD_Y = 38;
  constexpr int16_t CARD_H = 122;
  card(10, CARD_Y, 300, CARD_H, 22, CARD);
  if (message == nullptr) {
    drawPetriDish(SCREEN_WIDTH / 2, 76, 17, CARD);
    text("No messages yet", SCREEN_WIDTH / 2, 113, MC_DATUM, WHITE, CARD);
    text(GIFT_EMPTY_MESSAGES_HINT, SCREEN_WIDTH / 2, 135, MC_DATUM, TEXT_MUTED, CARD);
    return;
  }
  // The text gets the whole card: large type when it fits in four lines,
  // otherwise the regular size with room for five. Centred vertically.
  bool truncated = false;
  useFont(FONT_MESSAGE);
  uint8_t lines = drawWrapped(message->content, 26, 0, 268, 4, 25, WHITE, CARD, false, &truncated);
  uint16_t lineHeight = 25;
  if (truncated) {
    useFont(FONT_TEXT);
    lines = drawWrapped(message->content, 26, 0, 268, 5, 20, WHITE, CARD, false);
    lineHeight = 20;
  }
  const int16_t top = CARD_Y + (CARD_H - lines * lineHeight) / 2 + 2;
  drawWrapped(message->content, 26, top, 268, lineHeight == 25 ? 4 : 5, lineHeight, WHITE, CARD);
}

void Ui::drawAnimations(const GifMessageStore &gifMessages) {
  card(10, 10, 300, 150, 22, CARD);
  // A play button on a petri dish: the agar is the button, with two colonies
  // tucked beside the triangle.
  drawPetriDish(SCREEN_WIDTH / 2, 56, 26, CARD, false);
  sprite_->fillTriangle(152, 43, 152, 69, 174, 56, CARD);
  sprite_->fillSmoothCircle(147, 64, 3, COLONY_CREAM, AGAR);
  sprite_->fillSmoothCircle(170, 46, 2, COLONY_ROSE, AGAR);
  drawBadge(300, 20, gifMessages.unreadCount());

  useFont(FONT_TEXT);
  if (gifMessages.hasLatest()) {
    text("Loading animation", SCREEN_WIDTH / 2, 104, MC_DATUM, WHITE, CARD);
    return;
  }
  text("Waiting for an animation", SCREEN_WIDTH / 2, 104, MC_DATUM, WHITE, CARD);
  text(GIFT_EMPTY_ANIMATIONS_HINT, SCREEN_WIDTH / 2, 126, MC_DATUM, TEXT_MUTED, CARD);
  if (WiFi.status() != WL_CONNECTED) {
    text("Wi-Fi not connected", SCREEN_WIDTH / 2, 146, MC_DATUM, ROSE_TEXT, CARD);
  }
}

// The living petri dish: one colony for every message ever received, the
// oldest in the middle and newer ones spiralling outwards. Colonies breathe
// gently; the newest pops in with a ripple when a message arrives.
void Ui::drawCulture(const TimeService &time, uint8_t unread) {
  const uint32_t now = millis();
  lastCultureFrameAt_ = now;
  constexpr int16_t CX = 88;
  constexpr int16_t CY = 84;
  constexpr int16_t R = 74;
  constexpr uint16_t MAX_SHOWN = 150;  // the dish is full at this many
  sprite_->fillSmoothCircle(CX, CY, R, DISH_GLASS, BACKGROUND);
  sprite_->fillSmoothCircle(CX, CY, R - 3, BACKGROUND, DISH_GLASS);
  const float agar = R - 6;
  sprite_->fillSmoothCircle(CX, CY, agar, AGAR, BACKGROUND);
  sprite_->drawSmoothArc(CX, CY, agar - 5, agar - 9, 110, 160, AGAR_SHINE, AGAR, true);

  static const uint16_t palette[] = {COLONY_CREAM, COLONY_ROSE, COLONY_LILAC};
  const uint16_t shown = colonies_ < MAX_SHOWN ? colonies_ : MAX_SHOWN;
  const bool popping = colonyBornAt_ != 0 && now - colonyBornAt_ < 1400;
  // Sunflower spiral over the dish. A young culture has a few big colonies
  // spaced out; as more arrive the whole culture packs tighter, like real growth.
  const float spread = shown < 20 ? 20.0f : static_cast<float>(shown);
  const float spacing = (agar - 9) * sqrtf(3.14159f / spread);
  for (uint16_t index = 0; index < shown; ++index) {
    const float distance = (agar - 9) * sqrtf((index + 0.6f) / spread);
    const float angle = index * 2.39996323f;
    const float x = CX + cosf(angle) * distance;
    const float y = CY + sinf(angle) * distance;
    const uint32_t hash = (index + 1) * 2654435761u;
    const float age = shown > 1 ? 1.0f - static_cast<float>(index) / (shown - 1) : 1.0f;
    float radius = spacing * 0.3f * (0.75f + ((hash >> 8) & 0xFF) / 255.0f * 0.5f) *
                   (1.0f + 0.25f * age);
    radius *= 1.0f + 0.09f * sinf(now / 850.0f + index * 1.7f);  // breathing
    if (popping && index == shown - 1 && colonies_ <= MAX_SHOWN) {
      const float t = (now - colonyBornAt_) / 900.0f;
      if (t < 1.0f) {
        const float u = t - 1.0f;
        radius *= 1.0f + 2.7f * u * u * u + 1.7f * u * u;  // ease out with a little bounce
      }
      // A ripple spreading out from the new colony.
      const float ring = (now - colonyBornAt_) / 1400.0f;
      const uint8_t fade = static_cast<uint8_t>(255 * (1.0f - ring));
      const int32_t ringRadius = static_cast<int32_t>(4 + ring * 16);
      sprite_->drawSmoothCircle(static_cast<int32_t>(x), static_cast<int32_t>(y), ringRadius,
                                sprite_->alphaBlend(fade, WHITE, AGAR), AGAR);
    }
    if (radius > 0.3f) sprite_->drawSpot(x, y, radius, palette[hash % 3], AGAR);
  }

  // Side card: the time, the count and what it means.
  card(176, 10, 134, 150, 22, CARD);
  useFont(FONT_TEXT);
  text(time.shortTime(), 243, 26, MC_DATUM, TEXT_SOFT, CARD);
  useFont(FONT_TEMP);
  if (sprite_->textWidth(String(colonies_)) > 120) useFont(FONT_VALUE);  // four digits and up
  text(String(colonies_), 243, 78, MC_DATUM, WARM_TEXT, CARD);
  useFont(FONT_TEXT);
  text(colonies_ == 1 ? "colony" : "colonies", 243, 116, MC_DATUM, WHITE, CARD);
  text("one per message", 243, 138, MC_DATUM, TEXT_MUTED, CARD);
  drawBadge(300, 20, unread);
}
