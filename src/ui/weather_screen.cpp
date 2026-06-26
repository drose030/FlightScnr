#include "ui/weather_screen.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "config.h"
#include "hardware/display.h"
#include "hardware/display_font.h"
#include "services/api_keys.h"
#include "services/clock_time.h"
#include "services/weather.h"
#include "services/weather_icon.h"
#include "ui/radar_theme.h"
#include "ui/temp_label.h"

namespace ui {
namespace {

const int kCenterX = config::kDisplayWidth / 2;
const int kCenterY = config::kDisplayHeight / 2;

void drawCenteredAt(const char* text, int cx, int y, UiTextStyle style, uint16_t fg,
                    uint16_t bg) {
  displayFontApply(tft, style);
  tft.setTextDatum(TextDatum::TopCenter);
  tft.setTextColor(fg, bg);
  tft.drawString(text, cx, y);
}

void drawCentered(const char* text, int y, UiTextStyle style, uint16_t fg, uint16_t bg) {
  drawCenteredAt(text, kCenterX, y, style, fg, bg);
}

// Draw "<value>°<unit>" centered at cx (top at top_y).
void drawTempCentered(int cx, int top_y, int value, char unit, UiTextStyle style,
                      uint16_t fg, uint16_t bg) {
  temp_label::drawCentered(cx, top_y, value, unit, style, fg, bg);
}

void weekdayLabel(int64_t date_epoch, int index, char* out, size_t len) {
  if (index == 0) {
    strncpy(out, "Today", len - 1);
    out[len - 1] = '\0';
    return;
  }
  if (date_epoch <= 0) {
    snprintf(out, len, "Day %d", index + 1);
    return;
  }
  const time_t local = static_cast<time_t>(date_epoch + services::clock::timezoneOffsetSec());
  struct tm t {};
  gmtime_r(&local, &t);
  strftime(out, len, "%a", &t);
}

void drawForecast(const services::weather::WeatherData& wx, uint16_t fg, uint16_t dim,
                  uint16_t accent, uint16_t bg) {
  const char unit = wx.imperial ? 'F' : 'C';
  const int col_centers[services::weather::kForecastDays] = {kCenterX - 110, kCenterX,
                                                             kCenterX + 110};
  constexpr int kLabelY = 86;
  constexpr int kIconY = 108;
  constexpr int kHiY = 212;
  constexpr int kLoY = 240;

  for (int i = 0; i < services::weather::kForecastDays; ++i) {
    const services::weather::DayForecast& d = wx.days[i];
    if (!d.valid) {
      continue;
    }
    const int cx = col_centers[i];
    char label[12];
    weekdayLabel(d.date_epoch, i, label, sizeof(label));
    drawCenteredAt(label, cx, kLabelY, displayFontDetail(), i == 0 ? accent : fg, bg);
    services::weather_icon::drawIcon(tft, services::weather::dayIconCode(i), cx, kIconY, bg);
    drawTempCentered(cx, kHiY, static_cast<int>(lroundf(d.temp_max)), unit,
                     displayFontBody(), fg, bg);
    drawTempCentered(cx, kLoY, static_cast<int>(lroundf(d.temp_min)), unit,
                     displayFontDetail(), dim, bg);
  }
}

}  // namespace

void weatherScreenDraw() {
  tft.beginOffscreen();
  const uint16_t bg = tft.color565(radar::kBgR, radar::kBgG, radar::kBgB);
  const uint16_t fg = tft.color565(255, 255, 255);
  const uint16_t dim = tft.color565(150, 165, 180);
  const uint16_t hint = tft.color565(120, 140, 160);
  const uint16_t accent = tft.color565(radar::kSweepR, radar::kSweepG, radar::kSweepB);

  tft.fillScreen(bg);
  drawCentered("Forecast", 48, displayFontClockDate(), fg, bg);

  if (!services::apikeys::hasWeather()) {
    drawCentered("Add a Tomorrow.io key", kCenterY - 12, displayFontDetail(), dim, bg);
    drawCentered("in web settings", kCenterY + 12, displayFontDetail(), dim, bg);
  } else if (!services::weather::hasData()) {
    drawCentered(services::weather::fetchInProgress() ? "Loading weather…"
                                                      : "No weather data",
                 kCenterY - 8, displayFontBody(), dim, bg);
  } else {
    drawForecast(services::weather::data(), fg, dim, accent, bg);
  }

  drawCentered("Swipe left — Clock", 344, displayFontDetail(), hint, bg);

  tft.setTextDatum(TextDatum::TopLeft);
  tft.endOffscreen();
}

}  // namespace ui
