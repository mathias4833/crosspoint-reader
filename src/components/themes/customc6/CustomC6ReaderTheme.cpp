#include "CustomC6ReaderTheme.h"

#include <GfxRenderer.h>

#include <string>

#include "fontIds.h"

namespace {
constexpr int sideActionHintWidth = 24;
constexpr int sideActionHintHeight = 72;
constexpr int sideActionHintTextPadding = 6;
constexpr int sideActionHintRadius = 3;

enum class HintSide { Left, Right };

void drawSideActionHint(const GfxRenderer& renderer, HintSide side, int y, const char* label) {
  if (label == nullptr || label[0] == '\0') {
    return;
  }

  const int screenWidth = renderer.getScreenWidth();
  const int x = side == HintSide::Left ? 0 : screenWidth - sideActionHintWidth;

  const int right = x + sideActionHintWidth - 1;
  const int bottom = y + sideActionHintHeight - 1;

  if (side == HintSide::Left) {
    renderer.fillRoundedRect(x, y, sideActionHintWidth, sideActionHintHeight, sideActionHintRadius, false, true, false,
                             true, Color::White);
    renderer.drawLine(x, y, right - sideActionHintRadius, y);
    renderer.drawLine(x, bottom, right - sideActionHintRadius, bottom);
    renderer.drawLine(right, y + sideActionHintRadius, right, bottom - sideActionHintRadius);
    renderer.drawArc(sideActionHintRadius, right - sideActionHintRadius, y + sideActionHintRadius, 1, -1, 1, true);
    renderer.drawArc(sideActionHintRadius, right - sideActionHintRadius, bottom - sideActionHintRadius, 1, 1, 1, true);
  } else {
    renderer.fillRoundedRect(x, y, sideActionHintWidth, sideActionHintHeight, sideActionHintRadius, true, false, true,
                             false, Color::White);
    renderer.drawLine(x + sideActionHintRadius, y, right, y);
    renderer.drawLine(x + sideActionHintRadius, bottom, right, bottom);
    renderer.drawLine(x, y + sideActionHintRadius, x, bottom - sideActionHintRadius);
    renderer.drawArc(sideActionHintRadius, x + sideActionHintRadius, y + sideActionHintRadius, -1, -1, 1, true);
    renderer.drawArc(sideActionHintRadius, x + sideActionHintRadius, bottom - sideActionHintRadius, -1, 1, 1, true);
  }

  const int maxTextWidth = sideActionHintHeight - sideActionHintTextPadding * 2;
  const std::string text = renderer.truncatedText(SMALL_FONT_ID, label, maxTextWidth, EpdFontFamily::REGULAR);
  const int textWidth = renderer.getTextWidth(SMALL_FONT_ID, text.c_str(), EpdFontFamily::REGULAR);
  const int textHeight = renderer.getTextHeight(SMALL_FONT_ID);
  const int textX = side == HintSide::Left ? x + 1 : x + (sideActionHintWidth - textHeight) / 2;
  const int textY = y + (sideActionHintHeight + textWidth) / 2;
  renderer.drawTextRotated90CW(SMALL_FONT_ID, textX, textY, text.c_str(), true, EpdFontFamily::REGULAR);
}
}  // namespace

void CustomC6ReaderTheme::drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                                          const char* btn4) const {
  const GfxRenderer::Orientation orig_orientation = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  constexpr int topHintY = 161;
  constexpr int bottomHintY = 240;

  drawSideActionHint(renderer, HintSide::Left, topHintY, btn1);
  drawSideActionHint(renderer, HintSide::Left, bottomHintY, btn2);
  drawSideActionHint(renderer, HintSide::Right, topHintY, btn3);
  drawSideActionHint(renderer, HintSide::Right, bottomHintY, btn4);

  renderer.setOrientation(orig_orientation);
}
