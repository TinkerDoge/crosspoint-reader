# 🧠 Brainstorming: New Apps for DogeReader

## Context

**Device**: ESP32-C3 based e-paper reader (Xteink X4), 480×800 display, ~380KB RAM, SD card storage, 4 front buttons + 2 side buttons, WiFi, no touch on all models.

**Existing Apps**: Tarot Card Deck, Reading Stats (with Heatmap + Radar Profile)

**Current Scope**: The [SCOPE.md](file:///d:/CODE/DogeReader/SCOPE.md) explicitly says *"No notepads, calculators, or games"* and *"No RSS readers, news aggregators, or web browsers"* — these are **out of scope** for the upstream project. However, since this is your fork, you have full freedom.

> [!IMPORTANT]
> The ideas below are organized into **3 tiers** based on how well they fit the device's strengths (e-ink display, low power, reading-centric) and the technical constraints (380KB RAM, ESP32-C3, button-only input on most models).

---

## 🟢 Tier 1 — Natural Fit (Reading-Adjacent, Low Resource)

These apps complement the reading experience and feel native on an e-reader.

### 1. 📖 Reading Goals & Challenges
**Concept**: A gamified goal system beyond the current daily-minutes tracker.
- Set goals like *"Read 12 books this year"* or *"Read 30 min/day for 30 days"*
- Visual milestone badges rendered as e-ink art
- Monthly/yearly challenges with progress tracking
- "Current Challenge" widget on the home screen

**Effort**: Medium — builds on existing `ReadingStatsStore` infrastructure
**RAM**: Minimal — just stats aggregation + static rendering

---

### 2. 📝 Reading Journal / Book Notes
**Concept**: Simple text log per book — jot down thoughts at key moments.
- At end-of-chapter or via long-press, open a "thought prompt" with predefined options: *"Key idea"*, *"Favorite quote"*, *"Reaction"*, *"Question"*
- No free-text input (no keyboard!) — use curated templates or star ratings
- Exportable as JSON/text to SD card
- Browse past notes per book

**Effort**: Medium — needs a simple data store + list UI
**RAM**: Low — text only, paginated

---

### 3. 🕐 Pomodoro / Focus Timer
**Concept**: A reading-focused timer that helps build reading habits.
- Set a focus reading session (e.g., 25 min read / 5 min break)
- Timer runs in background while reading; subtle status bar indicator
- End-of-session vibration/screen flash
- Integrates with reading stats — *"This Pomodoro session: 12 pages read"*

**Effort**: Low-Medium — timer logic is simple, the integration with the reader is the challenge
**RAM**: Negligible

---

### 4. 📅 Daily Quote / "Word of the Day"
**Concept**: Load a quotes collection from a JSON file on SD card, show one per day.
- Beautiful e-ink typography rendering of the daily quote
- Can be set as sleep screen or home widget
- Users curate their own quote libraries (JSON file on SD)
- Optional: "Vocabulary of the Day" for language learners

**Effort**: Low — just a JSON reader + text renderer
**RAM**: Negligible

---

### 5. 📊 Library Analytics
**Concept**: Visualize your entire library at a glance.
- Total books count, books by genre/folder, storage usage
- Completion pie chart (% finished, % started, % unread)
- "Estimated reading time remaining" across unfinished books
- "Average book length" stats

**Effort**: Medium — needs filesystem scanning + aggregation
**RAM**: Moderate during scan, low during display

---

## 🟡 Tier 2 — Creative / Fun (Stretching the Device)

These are more experimental — they use the e-ink display in novel ways.

### 6. 🎲 Decision Maker / Random Picker
**Concept**: Can't decide what to read next? Let the device choose!
- "Random Book" — picks a random unread book from your library
- "Yes / No / Maybe" — simple decision randomizer
- Dice roller, coin flip — with nice e-ink animations
- "Magic 8-Ball" style responses

**Effort**: Low — very simple logic, fun e-ink animations
**RAM**: Negligible

---

### 7. 🌙 Sleep Stories / Ambient Text
**Concept**: Slowly scrolling calming text designed for winding down.
- Load relaxation/meditation scripts from SD card
- Very slow auto-scroll or timed page turns
- Pairs well with the auto-sleep timer
- Could include breathing exercise prompts (inhale/hold/exhale with timing)

**Effort**: Low — essentially a specialized text reader with auto-advance
**RAM**: Low

---

### 8. 🧮 Unit Converter / Quick Reference
**Concept**: Handy offline reference cards.
- Unit conversions (metric ↔ imperial), timezone helper
- Morse code chart, ASCII table, periodic table
- Static reference — load from BMP/JSON on SD
- Useful for travelers reading on the go

**Effort**: Low — mostly static rendering
**RAM**: Low

---

### 9. 📆 Calendar / Agenda Viewer
**Concept**: A simple monthly calendar view (offline).
- View the current month's calendar grid (reusing heatmap grid logic!)
- Mark dates manually (reading deadlines, book club dates)
- Persisted to SD card as JSON
- Navigate months with left/right buttons

**Effort**: Low-Medium — the grid rendering already exists in [ReadingHeatmapActivity.cpp](file:///d:/CODE/DogeReader/src/activities/apps/ReadingHeatmapActivity.cpp)
**RAM**: Low

---

### 10. 🃏 Flashcards / Spaced Repetition
**Concept**: Study flashcards loaded from SD card.
- Load decks as JSON: `{ "front": "...", "back": "..." }`
- Show front → press button → reveal back → rate difficulty (1-4)
- Simple spaced repetition scheduling (SM-2 algorithm)
- Great for vocabulary building alongside reading
- Reuses the Tarot-style card flip paradigm

**Effort**: Medium — needs scheduling algorithm + deck management
**RAM**: Low-Medium

---

## 🔴 Tier 3 — Ambitious (Significant Engineering)

These are feasible but require more careful engineering given the constraints.

### 11. 🌐 OPDS "What's New" Feed
**Concept**: Show new additions from your configured OPDS servers as a feed.
- Pull latest entries from OPDS catalog when WiFi is on
- Cache results on SD card for offline browsing
- One-tap download of new books
- *Note: New network connectors are currently frozen in upstream scope*

**Effort**: High — network code, caching, OPDS parsing already exists but extending it is complex
**RAM**: Moderate

---

### 12. ✍️ Handwriting / Doodle Pad (Touch models only)
**Concept**: Simple drawing canvas using the touch screen.
- Free-draw with finger on touch-enabled models
- Save doodles as BMP to SD card
- Could be used for quick margin notes
- Very limited by e-ink refresh rate

**Effort**: High — touch input handling, BMP export
**RAM**: Moderate (framebuffer)

---

## Summary Matrix

| App | Effort | RAM Impact | Scope Fit | Fun Factor |
|-----|--------|-----------|-----------|------------|
| Reading Goals & Challenges | Medium | Low | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ |
| Reading Journal / Book Notes | Medium | Low | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ |
| Pomodoro / Focus Timer | Low-Med | Negligible | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ |
| Daily Quote / WOTD | Low | Negligible | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ |
| Library Analytics | Medium | Moderate | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ |
| Decision Maker / Random Picker | Low | Negligible | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| Sleep Stories / Ambient Text | Low | Low | ⭐⭐⭐⭐ | ⭐⭐⭐ |
| Unit Converter / Quick Ref | Low | Low | ⭐⭐ | ⭐⭐ |
| Calendar / Agenda | Low-Med | Low | ⭐⭐⭐ | ⭐⭐⭐ |
| Flashcards / Spaced Repetition | Medium | Low-Med | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| OPDS "What's New" Feed | High | Moderate | ⭐⭐⭐ | ⭐⭐⭐ |
| Handwriting / Doodle Pad | High | Moderate | ⭐⭐ | ⭐⭐⭐ |

---

## 💡 My Top Picks

If I had to recommend **3 to start with**, based on the sweet spot of effort vs. impact:

1. **Daily Quote / WOTD** — Easiest win. Beautiful e-ink typography, minimal code, users love it. Can be a sleep screen too.
2. **Flashcards** — Killer feature for a reading device. Leverages existing Tarot card UI patterns. Great for language learners.
3. **Reading Goals & Challenges** — Builds on existing stats infrastructure. Adds gamification that keeps users engaged with the device.

> [!TIP]
> Which ideas excite you? Pick one (or several) and I can draft an implementation plan with the full architecture, file structure, and estimated code changes.
