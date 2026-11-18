#pragma once

// Everything personal about the graduation gift lives in this file. Edit the
// text here, rebuild, and nothing else needs to change. Save it as UTF-8;
// Greek works everywhere (the title font includes Greek too).
//
// The printable label (print/gift_label.svg and .pdf) is generated from the
// same three lines: run `python3 tools/make_gift_label.py` after editing them.

// --------------------------------------------------------------- who and what --
// The name is the startup screen's title; all three go on the printed label,
// and the name also appears in gifs/congrats.gif.
constexpr char GIFT_NAME[] = "Alex Example";                  // FONT_TITLE, keep it short (~14 characters)
constexpr char GIFT_DEGREE[] = "BSc Food Science";    // one line, about 36 characters at most
constexpr char GIFT_DATE[] = "2026-12-18";              // shown exactly as written

// ------------------------------------------------------ everyday screen text --
// Title of the startup screen ("Connecting to Wi-Fi..." appears below it).
constexpr const char *GIFT_BOOT_TITLE = GIFT_NAME;
// Second line on the empty Messages and Animations screens.
constexpr char GIFT_EMPTY_MESSAGES_HINT[] = "Notes from your people land here";
constexpr char GIFT_EMPTY_ANIMATIONS_HINT[] = "Little surprises will show up here";
// The open hotspot the display creates for Wi-Fi setup. Plain ASCII, no more
// than 32 characters. Also printed on the how-to card.
constexpr char GIFT_SETUP_NETWORK[] = "Petri-Display-Setup";

// ------------------------------------------------------------ culture screen --
// After this many minutes without a button press the display shows the living
// petri dish (one colony per message received). Any button returns to the
// clock. 0 turns this off; the screen is still in the button rotation.
constexpr uint32_t CULTURE_IDLE_MINUTES = 5;

// ------------------------------------------------- preloaded inbox messages --
// Added to the inbox on the very first start only (when /messages.json does
// not exist yet), as unread messages. Up to 10 entries. They arrive in the
// order listed: the first entry is the oldest, so the LAST entry is the one the
// inbox opens on. The sender's name is shown where the received time normally
// goes. Each text fits about four lines on screen (roughly 120 characters).
struct GiftMessage {
  const char *sender;
  const char *text;
};

constexpr GiftMessage GIFT_MESSAGES[] = {
    {"Mum & Dad", "We are so proud of you! Congratulations on your degree."},
    {"Μαρία", "Συγχαρητήρια! Καλή αρχή στο επόμενο κεφάλαιο!"},
    {"The lab crew", "From petri dishes to a diploma. Well done, graduate!"},
};
