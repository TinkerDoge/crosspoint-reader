# Improvement Backlog

A curated, evidence-based list of known issues, stubs, fragilities and documentation drift discovered while
indexing the firmware (see [firmware-index.md](firmware-index.md)). Each item cites the source so you can
verify before acting. This is a starting point for improvement work, **not** a commitment or a spec — check
[SCOPE.md](../SCOPE.md) and [GOVERNANCE.md](../GOVERNANCE.md) before building anything new, and open a
Discussion if unsure.

Priorities are a rough triage:
- **P1** — correctness / security / data-loss risk, or user-visibly broken.
- **P2** — misleading behavior, maintainability hazard, or partial feature.
- **P3** — polish, cleanup, docs.

---

## Security (all P1 — audit before any public release)

| Item | Evidence |
|------|----------|
| Web server has **no authentication** — anyone on the LAN/AP can read/write/delete the SD card | [CrossPointWebServer.cpp](../src/network/CrossPointWebServer.cpp); noted in [webserver.md](webserver.md) |
| WiFi **AP is an open network** (`CrossPoint-Reader`) with a captive portal — any nearby device can join and browse files | [CrossPointWebServerActivity.cpp](../src/activities/network/CrossPointWebServerActivity.cpp) `startAccessPoint()` |
| **TLS is never validated** anywhere — `NetworkClientSecure::setInsecure()` for OPDS/kosync downloads; OTA sets `skip_cert_common_name_check=true`. MITM risk on catalog, sync and **firmware updates** | [HttpDownloader.cpp](../src/network/HttpDownloader.cpp), [KOReaderSyncClient.cpp](../lib/KOReaderSync/KOReaderSyncClient.cpp), [OtaUpdater.cpp](../src/network/OtaUpdater.cpp) |
| WebDAV `canHandle()` matches by HTTP method on **any URI** with no auth; LOCK/UNLOCK are stubs | [WebDAVHandler.cpp](../src/network/WebDAVHandler.cpp) |
| Credentials are **obfuscated, not encrypted** (XOR with 6-byte MAC + base64) — recoverable from the SD card | [ObfuscationUtils.h](../lib/Serialization/ObfuscationUtils.h), [WifiCredentialStore.h](../src/WifiCredentialStore.h) |

*Some of these are inherent to a "pull books over local WiFi" design and may be acceptable within scope —
the point is to make the trade-off explicit and, where cheap, add opt-in hardening (e.g. WPA2 on the AP, a
transfer PIN, cert pinning for OTA).*

---

## Correctness & broken/stubbed features

| Pri | Item | Evidence |
|-----|------|----------|
| ~~P1~~ | ~~**Reading streaks are fake.**~~ Fixed — `getCurrentStreak()` and `getBestStreak()` now use real date-aware consecutive-day math in `ReadingStatsAnalytics` (single source of truth), with host unit tests. `ReadingStatsStore` delegates; old inflated `bestStreak` self-heals on load. | [util/ReadingStatsAnalytics.cpp](../src/util/ReadingStatsAnalytics.cpp), [test/reading_stats/](../test/reading_stats/) |
| ~~P2~~ | ~~**Reading Stats book progress bars are always empty.**~~ Fixed — each reader's `onExit` now calls `recordProgress()` with the current book percent; the Stats screen reads it via `getBookPercent()`. Progress is persisted in the stats JSON as `bookProgress`. | [ReadingStatsStore.cpp](../src/ReadingStatsStore.cpp), [apps/ReadingStatsActivity.cpp:44](../src/activities/apps/ReadingStatsActivity.cpp#L44) |
| P2 | **Reading Profile radar is placeholder math** (`// Simplified metric calculation`) with a hardcoded summary string | [apps/ReadingProfileActivity.cpp:27](../src/activities/apps/ReadingProfileActivity.cpp#L27), [:70](../src/activities/apps/ReadingProfileActivity.cpp#L70) |
| P2 | **Home "Sync" shortcut is a no-op** (`// Mock sync`) and its subtitle "Last sync: Just now" is faked | [home/HomeActivity.cpp:213](../src/activities/home/HomeActivity.cpp#L213), [util/ShortcutRegistry.cpp:28](../src/util/ShortcutRegistry.cpp#L28) |
| P2 | **Reading Heatmap is unreachable** — `onReadingHeatmapOpen()` exists but `AppsActivity` lists only 2 apps (Heatmap is reachable only via ReadingStats→Confirm) | [home/AppsActivity.cpp:84](../src/activities/home/AppsActivity.cpp#L84) |
| P2 | Possible **infinite reload loop** if a page keeps failing to load (clears cache + `requestUpdate()` on every failure) | [reader/EpubReaderActivity.cpp:643](../src/activities/reader/EpubReaderActivity.cpp#L643) |
| P3 | Grayscale anti-aliasing pass runs unconditionally (`// TODO: Only do this if font supports it`) — wasted work on fonts without AA data | [reader/EpubReaderActivity.cpp:768](../src/activities/reader/EpubReaderActivity.cpp#L768) |
| P3 | OTA install uses a `delay(100)` polling loop; activity flagged for refactor (`// TODO @ngxson`) | [OtaUpdater.cpp:246](../src/network/OtaUpdater.cpp#L246), [settings/OtaUpdateActivity.cpp:142](../src/activities/settings/OtaUpdateActivity.cpp#L142) |

---

## Internationalization gaps (P2)

Several strings bypass the `tr()` / i18n system that the rest of the app uses — they will not translate:

- `ShortcutRegistry` subtitles are hardcoded English ("WiFi File Sharing", "Jump back in", …) —
  [util/ShortcutRegistry.cpp:29-34](../src/util/ShortcutRegistry.cpp#L29-L34).
- Apps screens (ReadingStats / Heatmap / Profile) use hardcoded English labels ("Heatmap", "Profile",
  "Started Books", box labels) instead of `tr()`.

---

## Fragility & maintainability hazards (P2/P3)

- **Dead/duplicated wake-verification code:** `verifyPowerButtonDuration()` in
  [main.cpp:131-171](../src/main.cpp#L131-L171) appears superseded by `gpio.verifyPowerButtonWakeup()` used in
  `setup()`; the local helper is defined but not called and can drift.
- **Boot-loop guard is a cross-file invariant:** reader-crash detection depends on `readerActivityLoadCount`
  being pre-incremented/saved before opening a book and reset elsewhere (in the reader) —
  [main.cpp:302-306](../src/main.cpp#L302-L306).
- **`progress.bin` has no version/magic** (raw byte packing) — a future layout change risks silent misreads.
  Reader [EpubReaderActivity.cpp:696](../src/activities/reader/EpubReaderActivity.cpp#L696).
- **Wake-debounce edge case:** a double single-tap can still power the device on (`TODO`) —
  [HalGPIO.cpp:243](../lib/hal/HalGPIO.cpp#L243).
- **`SDCardManager::readFile` silently caps at 50 KB** — larger files truncate; callers must use streaming
  variants — [SDCardManager.cpp:74](../open-x4-sdk/libs/hardware/SDCardManager/src/SDCardManager.cpp#L74).
- **Panic/crash reporting depends on linker `--wrap` flags** and hand-rolled IRAM-safe copies; removing the
  flags in [platformio.ini:38](../platformio.ini#L38) silently breaks crash reports —
  [HalSystem.cpp](../lib/hal/HalSystem.cpp).
- **JPEGDEC patch is signature-based** and no-ops with a warning if the upstream line changes; the library is
  SHA-pinned to keep it valid — [patch_jpegdec.py:54-59](../scripts/patch_jpegdec.py#L54-L59),
  [platformio.ini:65](../platformio.ini#L65).
- **Per-pixel `malloc`/`free` of row buffers** in `drawBitmap`/`drawBitmap1Bit`/`fillPolygon` — allocation
  churn per image row on a memory-tight MCU — [GfxRenderer.cpp](../lib/GfxRenderer/GfxRenderer.cpp).
- **Font "hot group" pointer lifetime:** `getGlyphBitmap`/`FontDecompressor::getBitmap` returns a pointer
  valid only until the next call — fragile if a future caller batches glyph lookups —
  [GfxRenderer.cpp:18-20](../lib/GfxRenderer/GfxRenderer.cpp#L18-L20).
- **Anchor lookup is a linear scan** per footnote/cross-ref jump — [Section.cpp:290](../lib/Epub/Epub/Section.cpp#L290).
- **Single global WebSocket upload state** — one upload server-wide, race-guarded only by client number —
  [CrossPointWebServer.cpp:32-44](../src/network/CrossPointWebServer.cpp#L32-L44).
- **Dead code:** unused Calibre smart-device `UDP_PORTS[]` array —
  [CrossPointWebServer.cpp:26](../src/network/CrossPointWebServer.cpp#L26).
- **Unsupported CSS selectors** (descendant, child, sibling, attribute, pseudo, ID, wildcard, multi-class) —
  many `TODO`s in [css/CssParser.cpp](../lib/Epub/Epub/css/CssParser.cpp) (~lines 395-437, 630, 644).
- **Fixed-size buffers truncate** long values: footnote `href[64]`/`number[24]`
  ([ChapterHtmlSlimParser.h:81-83](../lib/Epub/Epub/parsers/ChapterHtmlSlimParser.h#L81-L83)); recent books
  hard-capped at 10 in two places; WiFi at 8.

---

## Documentation drift (P2/P3)

Fix these so the docs match the shipping code:

1. **[file-formats.md](file-formats.md) version numbers are stale.** It documents `book.bin` v3 and
   `section.bin` v8, but the code ships `BOOK_CACHE_VERSION = 5`
   ([BookMetadataCache.cpp:12](../lib/Epub/Epub/BookMetadataCache.cpp#L12)) and `SECTION_FILE_VERSION = 19`
   ([Section.cpp:13](../lib/Epub/Epub/Section.cpp#L13)). The ImHex `section.bin` struct also predates the
   current header fields (`hyphenationEnabled`, `embeddedStyle`, `imageRendering`, alignment, anchor-map
   offset). *A staleness banner has been added to that doc; the ImHex patterns still need a full rewrite.*
2. **[contributing/architecture.md](contributing/architecture.md) describes the old navigation model** —
   free functions `exitActivity()`/`enterNewActivity()` in `main.cpp` and `ActivityWithSubactivity`. Both are
   gone; navigation is now the stack-based `ActivityManager` (`goTo*` / `startActivityForResult` / `finish`).
   See [activity-manager.md](activity-manager.md) for the current model. *A pointer note has been added.*
3. **[webserver.md](webserver.md) / [webserver-endpoints.md](webserver-endpoints.md) are incomplete.** They
   omit the implemented `/download`, `/rename`, `/move`, `/settings`, `/api/settings`, `/js/jszip.min.js`
   routes, the full **WebDAV** surface, and the **UDP :8134** discovery responder. `/delete` also now accepts
   a JSON `paths[]` batch form.
4. `ActivityWithSubactivity` survives only in prose (architecture.md); the class is fully removed from `src/`.

---

## Suggested first steps

Low-risk, high-clarity places to begin (each is self-contained and testable):

1. ~~**Fix reading streaks** — implement date-aware streak counting in `ReadingStatsStore` /
   `ReadingStatsAnalytics`.~~ ✅ Done.
2. ~~**Wire book progress into Reading Stats** — read `progress.bin` per book instead of hardcoding 0.~~ ✅ Done (via per-book `recordProgress()` in each reader's `onExit`).
3. **i18n the hardcoded strings** in `ShortcutRegistry` and the apps screens.
4. **Reconcile the three stale docs** above against the code (small, mechanical, improves everyone's map).
5. **Decide the fate of the Sync shortcut and the unwired Heatmap entry** — either wire them up or remove the
   dead UI.
