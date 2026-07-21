#include "TarotActivity.h"
#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include "components/UITheme.h"
#include "fontIds.h"
#include <algorithm>

TarotActivity::TarotActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Tarot", renderer, mappedInput), viewState(ViewState::DrawPrompt), 
      showMeaning(false), firstRenderDone(false), currentCardId(-1), gridPageIndex(0), isDrawing(false) {}

void TarotActivity::onEnter() {
    Activity::onEnter();
    deck.shuffle();
    assets.loadMeanings();
    viewState = ViewState::DrawPrompt;
    history.clear();
    requestUpdate();
}

void TarotActivity::onExit() {
    Activity::onExit();
}

void TarotActivity::loop() {
    Activity::loop();

    if (viewState == ViewState::DrawPrompt) {
        if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
            // Consistent with main view, Right button (btn4) is Draw
            drawNextCard();
        }
        if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
            finish();
        }
    } else if (viewState == ViewState::Main) {
        if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
            // Button 4 (Right) is Draw
            if (showMeaning) {
                showMeaning = false;
            }
            drawNextCard();
        }

        if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
            // Button 3 (Left) is Meaning
            toggleMeaning();
        }

        if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
            // Button 2 (Confirm) is History
            if (!history.empty()) {
                viewState = ViewState::HistoryGrid;
                gridPageIndex = 0;
                requestUpdate();
            }
        }

        if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
            if (showMeaning) {
                showMeaning = false;
                requestUpdate();
            } else {
                finish();
            }
        }
    } else if (viewState == ViewState::HistoryGrid) {
        if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
            viewState = ViewState::Main;
            requestUpdate();
        }
        if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
            if (gridPageIndex > 0) {
                gridPageIndex--;
                requestUpdate();
            }
        }
        if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
            int maxPage = (history.size() - 1) / 12;
            if (gridPageIndex < maxPage) {
                gridPageIndex++;
                requestUpdate();
            }
        }
    }
}

void TarotActivity::drawNextCard() {
    isDrawing = true;
    requestUpdateAndWait(); // Render the toast

    currentCardId = deck.drawNext();
    if (currentCardId != -1) {
        history.insert(history.begin(), currentCardId);
        viewState = ViewState::Main;
    }
    showMeaning = false;
    isDrawing = false;
    requestUpdate();
}

void TarotActivity::toggleMeaning() {
    if (currentCardId != -1) {
        showMeaning = !showMeaning;
        requestUpdate();
    }
}

void TarotActivity::render(RenderLock&& lock) {
    const auto pageWidth = renderer.getScreenWidth();
    const auto pageHeight = renderer.getScreenHeight();
    const auto& metrics = UITheme::getInstance().getMetrics();

    if (isDrawing) {
        GUI.drawPopup(renderer, tr(STR_TAROT_DRAWING));
        renderer.displayBuffer(HalDisplay::FAST_REFRESH);
        return;
    }

    renderer.clearScreen();

    switch (viewState) {
        case ViewState::DrawPrompt:
            renderPrompt(metrics, pageWidth, pageHeight);
            break;
        case ViewState::Main:
            renderMain(metrics, pageWidth, pageHeight);
            break;
        case ViewState::HistoryGrid:
            renderGrid(metrics, pageWidth, pageHeight);
            break;
    }

    // Use FAST_REFRESH even on entry to avoid the "shuffle animation" (full refresh blink)
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    firstRenderDone = true;
}

void TarotActivity::renderPrompt(const ThemeMetrics& metrics, int pageWidth, int pageHeight) {
    renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 3, tr(STR_TAROT_TITLE), true, EpdFontFamily::BOLD);
    
    HalFile file;
    if (Storage.openFileForRead("TAROT", assets.getBackImagePath(), file)) {
        Bitmap bitmap(file, true);
        if (bitmap.parseHeaders() == BmpReaderError::Ok) {
            int x = (pageWidth - bitmap.getWidth()) / 2;
            int y = (pageHeight - bitmap.getHeight()) / 2 - 20;
            renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight);
        }
        file.close();
    }

    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight * 3 / 4, tr(STR_TAROT_SHUFFLE));
    
    // Consistent with Main view: btn1=Back, btn2="", btn3="", btn4=Draw
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", tr(STR_TAROT_DRAW));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void TarotActivity::renderMain(const ThemeMetrics& metrics, int pageWidth, int pageHeight) {
    // 1. Draw History Row
    int thumbW = 80;
    int thumbSpacing = 12;
    int startX = (pageWidth - (thumbW * 5 + thumbSpacing * 4)) / 2;
    int thumbY = metrics.topPadding + 10;

    for (int i = 0; i < 5 && (i + 1) < history.size(); i++) {
        int x = startX + i * (thumbW + thumbSpacing);
        int8_t cardId = history[i + 1];
        
        HalFile file;
        if (Storage.openFileForRead("TAROT", assets.getCardThumbPath(cardId), file)) {
            Bitmap bitmap(file, true);
            if (bitmap.parseHeaders() == BmpReaderError::Ok) {
                renderer.drawBitmap(bitmap, x, thumbY, thumbW, thumbW * 1.7);
            }
            file.close();
        }
        
        if (i == 4 && history.size() > 6) {
            renderer.drawRect(x - 4, thumbY - 4, thumbW, thumbW * 1.7, 1, true);
            renderer.drawRect(x - 8, thumbY - 8, thumbW, thumbW * 1.7, 1, true);
        }
    }

    // 2. Draw Main Card
    if (currentCardId != -1) {
        HalFile file;
        if (Storage.openFileForRead("TAROT", assets.getCardImagePath(currentCardId), file)) {
            Bitmap bitmap(file, true);
            if (bitmap.parseHeaders() == BmpReaderError::Ok) {
                int drawW = 320;
                int drawH = 533; 
                int x = (pageWidth - drawW) / 2;
                int y = thumbY + 160;

                renderer.drawBitmap(bitmap, x, y, drawW, drawH);
                renderer.drawRect(x - 2, y - 2, drawW + 4, drawH + 4, 2, true); // Frame
                
                auto m = assets.getMeaning(currentCardId);
                char label[128];
                snprintf(label, sizeof(label), "%s  |  Card %d of 78", m.name.c_str(), currentCardId + 1);
                renderer.drawCenteredText(UI_10_FONT_ID, y + drawH + 20, label, true, EpdFontFamily::BOLD);
            }
            file.close();
        }
    }

    // 3. Meaning Overlay (Custom Box)
    if (showMeaning && currentCardId != -1) {
        auto m = assets.getMeaning(currentCardId);
        
        int boxW = pageWidth - 60;
        int boxH = 280;
        int boxX = (pageWidth - boxW) / 2;
        int boxY = (pageHeight - boxH) / 2;

        // Shadow/Border
        renderer.fillRoundedRect(boxX - 2, boxY - 2, boxW + 4, boxH + 4, 10, Color::Black);
        renderer.fillRoundedRect(boxX, boxY, boxW, boxH, 8, Color::White);

        renderer.drawCenteredText(UI_12_FONT_ID, boxY + 20, m.name.c_str(), true, EpdFontFamily::BOLD);
        renderer.drawLine(boxX + 30, boxY + 55, boxX + boxW - 30, boxY + 55, 1, true);

        // Correctly wrap the meaning text
        auto lines = renderer.wrappedText(NOTOSERIF_14_FONT_ID, m.meaning.c_str(), boxW - 50, 6);
        int currentY = boxY + 80;
        int lineHeight = renderer.getLineHeight(NOTOSERIF_14_FONT_ID) + 4;
        for (const auto& line : lines) {
            renderer.drawText(NOTOSERIF_14_FONT_ID, boxX + 25, currentY, line.c_str());
            currentY += lineHeight;
        }
    }

    // 4. Hints - [Back] [History] [Meaning] [Draw]
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_TAROT_HISTORY), tr(STR_TAROT_MEANING), tr(STR_TAROT_DRAW));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void TarotActivity::renderGrid(const ThemeMetrics& metrics, int pageWidth, int pageHeight) {
    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_TAROT_HISTORY));

    int thumbW = 80;
    int thumbH = 133;
    int cols = 4;
    int rows = 5;
    int spacingX = 20;
    int spacingY = 12;
    int startX = (pageWidth - (thumbW * cols + spacingX * (cols - 1))) / 2;
    int startY = metrics.headerHeight + 20;

    int itemsPerPage = cols * rows;
    int startIdx = gridPageIndex * itemsPerPage;

    for (int i = 0; i < itemsPerPage && (startIdx + i) < history.size(); i++) {
        int8_t cardId = history[startIdx + i];
        int col = i % cols;
        int row = i / cols;
        int x = startX + col * (thumbW + spacingX);
        int y = startY + row * (thumbH + spacingY);

        HalFile file;
        if (Storage.openFileForRead("TAROT", assets.getCardThumbPath(cardId), file)) {
            Bitmap bitmap(file, true);
            if (bitmap.parseHeaders() == BmpReaderError::Ok) {
                renderer.drawBitmap(bitmap, x, y, thumbW, thumbH);
                renderer.drawRect(x, y, thumbW, thumbH, 1, true);
            }
            file.close();
        }
    }

    int totalPages = (history.size() + itemsPerPage - 1) / itemsPerPage;
    char pageStr[32];
    snprintf(pageStr, sizeof(pageStr), "Page %d / %d", gridPageIndex + 1, totalPages);
    renderer.drawCenteredText(SMALL_FONT_ID, pageHeight - metrics.buttonHintsHeight - 15, pageStr);

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", tr(STR_PREV), tr(STR_NEXT));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
