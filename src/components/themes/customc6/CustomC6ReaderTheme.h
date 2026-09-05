#pragma once

#include "components/themes/lyra/LyraTheme.h"

namespace CustomC6ReaderMetrics {
constexpr ThemeMetrics values = [] {
  ThemeMetrics metrics = LyraMetrics::values;
  metrics.buttonHintsHeight = 0;
  return metrics;
}();
}  // namespace CustomC6ReaderMetrics

class CustomC6ReaderTheme : public LyraTheme {
 public:
  void drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                       const char* btn4) const override;
};
