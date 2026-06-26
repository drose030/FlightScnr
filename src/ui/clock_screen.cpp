#include "ui/clock_screen.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "hardware/display.h"
#include "hardware/display_font.h"
#include "services/clock_time.h"
#include "services/weather.h"
#include "services/weather_icon.h"
#include "ui/radar_theme.h"

namespace ui {
namespace {

constexpr int kBezelInsetPx = 10;
constexpr int kTextPadPx = 6;
constexpr int kLineGap = 4;
constexpr int kSectionGap = 8;
/** Extra space between the time row and the date line. */
constexpr int kTimeDateGap = 14;
/** Extra space between UTC label and swipe hints. */
constexpr int kHintsTopGap = 22;

const int kCenterX = config::kDisplayWidth / 2;
const int kCenterY = config::kDisplayHeight / 2;
const int kCircleRadius = kCenterX - kBezelInsetPx;

int circleHalfWidthAtRow(int row_y, int row_h) {
  if (kCircleRadius <= 0 || row_h <= 0) {
    return 0;
  }
  const int row_center_y = row_y + row_h / 2;
  const int dy = row_center_y - kCenterY;
  if (std::abs(dy) >= kCircleRadius) {
    return 0;
  }
  const float half =
      std::sqrt(static_cast<float>(kCircleRadius * kCircleRadius - dy * dy));
  const int usable = static_cast<int>(half) - kTextPadPx;
  return usable > 0 ? usable : 0;
}

void fitLineToWidth(char* text, size_t len, UiTextStyle style, int max_width_px) {
  if (max_width_px <= 0 || text[0] == '\0') {
    return;
  }
  displayFontApply(tft, style);
  if (tft.textWidth(text) <= max_width_px) {
    return;
  }
  char original[48];
  strncpy(original, text, sizeof(original) - 1);
  original[sizeof(original) - 1] = '\0';
  const size_t raw_len = strlen(original);
  for (size_t n = raw_len; n > 0; --n) {
    snprintf(text, len, "%.*s…", static_cast<int>(n), original);
    if (tft.textWidth(text) <= max_width_px) {
      return;
    }
  }
  strncpy(text, "…", len);
  text[len - 1] = '\0';
}

void drawCenterLine(const char* text, int* y, UiTextStyle style, uint16_t fg,
                    uint16_t bg) {
  displayFontApply(tft, style);
  const int h = displayFontHeight(tft, style);
  const int max_half_w = circleHalfWidthAtRow(*y, h);
  const int max_w = max_half_w * 2;

  char line[48];
  strncpy(line, text, sizeof(line) - 1);
  line[sizeof(line) - 1] = '\0';
  fitLineToWidth(line, sizeof(line), style, max_w);

  tft.setTextDatum(TextDatum::TopCenter);
  tft.setTextColor(fg, bg);
  tft.drawString(line, kCenterX, *y);
  *y += h + kLineGap;
}

void drawTimeWithAmPm(int* y, const char* time_line, const char* ampm_line, uint16_t time_fg,
                      uint16_t ampm_fg, uint16_t bg) {
  const UiTextStyle time_style = displayFontClockTime();
  const UiTextStyle ampm_style = displayFontClockAmPm();
  const int time_w = displayFontWidth(tft, time_style, time_line);
  const int time_h = displayFontHeight(tft, time_style);

  constexpr int kAmPmGap = 8;
  int ampm_w = 0;
  if (ampm_line[0] != '\0') {
    ampm_w = displayFontWidth(tft, ampm_style, ampm_line);
  }

  const int total_w = time_w + (ampm_w > 0 ? kAmPmGap + ampm_w : 0);
  const int x = kCenterX - total_w / 2;
  const int bottom_y = *y + time_h;

  tft.setTextDatum(TextDatum::BottomLeft);
  displayFontApply(tft, time_style);
  tft.setTextColor(time_fg, bg);
  tft.drawString(time_line, x, bottom_y);

  if (ampm_line[0] != '\0') {
    displayFontApply(tft, ampm_style);
    tft.setTextColor(ampm_fg, bg);
    tft.drawString(ampm_line, x + time_w + kAmPmGap, bottom_y);
  }

  *y += time_h + kLineGap;
}

// Visible width of "<value>°<unit>" with the degree drawn as a small ring.
constexpr int kDegRingR = 3;
constexpr int kDegGap1 = 3;
constexpr int kDegGap2 = 2;

int tempVisibleWidth(UiTextStyle style, int value, char unit) {
  char num[8];
  snprintf(num, sizeof(num), "%d", value);
  char us[2] = {unit, '\0'};
  displayFontApply(tft, style);
  return tft.textWidth(num) + kDegGap1 + (2 * kDegRingR + 1) + kDegGap2 + tft.textWidth(us);
}

// Draw "<value>°<unit>" with top-left at (x, top_y); returns width drawn.
int drawTempAt(int x, int top_y, int value, char unit, UiTextStyle style, uint16_t fg,
               uint16_t bg) {
  char num[8];
  snprintf(num, sizeof(num), "%d", value);
  char us[2] = {unit, '\0'};
  displayFontApply(tft, style);
  const int numw = tft.textWidth(num);
  const int uw = tft.textWidth(us);
  const int ringw = 2 * kDegRingR + 1;
  tft.setTextDatum(TextDatum::TopLeft);
  tft.setTextColor(fg, bg);
  tft.drawString(num, x, top_y);
  tft.drawCircle(x + numw + kDegGap1 + kDegRingR, top_y + kDegRingR + 2, kDegRingR, fg);
  tft.drawString(us, x + numw + kDegGap1 + ringw + kDegGap2, top_y);
  return numw + kDegGap1 + ringw + kDegGap2 + uw;
}

int currentWeatherRowHeight() {
  if (!services::weather::hasData()) {
    return 0;
  }
  const int icon_h =
      services::weather_icon::iconHeight(services::weather::currentIconCode());
  const int temp_h = displayFontHeight(tft, displayFontBody());
  return (icon_h > temp_h ? icon_h : temp_h);
}

void drawCurrentWeatherRow(int* y, uint16_t accent, uint16_t bg) {
  const services::weather::WeatherData& wx = services::weather::data();
  const int code = services::weather::currentIconCode();
  const int icon_w = services::weather_icon::iconWidth(code);
  const int icon_h = services::weather_icon::iconHeight(code);
  const int temp = static_cast<int>(std::lround(wx.current_temp));
  const char unit = wx.imperial ? 'F' : 'C';
  const UiTextStyle ts = displayFontBody();
  const int temp_w = tempVisibleWidth(ts, temp, unit);
  const int temp_h = displayFontHeight(tft, ts);
  const int row_h = icon_h > temp_h ? icon_h : temp_h;
  const int icon_gap = icon_w > 0 ? 8 : 0;
  const int row_w = icon_w + icon_gap + temp_w;
  const int start_x = kCenterX - row_w / 2;

  if (icon_w > 0) {
    services::weather_icon::drawIcon(tft, code, start_x + icon_w / 2,
                                     *y + (row_h - icon_h) / 2, bg);
  }
  drawTempAt(start_x + icon_w + icon_gap, *y + (row_h - temp_h) / 2, temp, unit, ts, accent,
             bg);
  *y += row_h + kLineGap;
}

}  // namespace

void clockScreenDraw() {
  tft.beginOffscreen();
  const uint16_t bg = tft.color565(radar::kBgR, radar::kBgG, radar::kBgB);
  const uint16_t fg = tft.color565(255, 255, 255);
  const uint16_t accent_fg = tft.color565(radar::kSweepR, radar::kSweepG, radar::kSweepB);
  const uint16_t ampm_fg = accent_fg;
  const uint16_t hint_fg = tft.color565(120, 140, 160);

  char time_line[16];
  char ampm_line[8];
  char date_line[24];
  char tz_line[16];
  services::clock::formatTimeOfDay(time_line, sizeof(time_line));
  services::clock::formatAmPm(ampm_line, sizeof(ampm_line));
  services::clock::formatDateLine(date_line, sizeof(date_line));
  services::clock::formatTimezoneLabel(tz_line, sizeof(tz_line));

  const int time_h = displayFontHeight(tft, displayFontClockTime());
  const int date_h = displayFontHeight(tft, displayFontClockDate()) + kLineGap;
  const int tz_h = displayFontHeight(tft, displayFontDetail()) + kLineGap;
  const int detail_h = displayFontHeight(tft, displayFontDetail());
  const int hints_h = detail_h * 3 + kLineGap * 2 + kHintsTopGap;
  const bool show_weather = services::weather::hasData();
  const int weather_h = show_weather ? currentWeatherRowHeight() + kSectionGap : 0;
  const int block_h = time_h + kTimeDateGap + date_h + tz_h + weather_h + hints_h;

  tft.fillScreen(bg);

  int y = kCenterY - block_h / 2;
  if (y < kBezelInsetPx) {
    y = kBezelInsetPx;
  }

  drawTimeWithAmPm(&y, time_line, ampm_line, accent_fg, ampm_fg, bg);

  y += kTimeDateGap;
  drawCenterLine(date_line, &y, displayFontClockDate(), fg, bg);
  y += kSectionGap - kLineGap;
  drawCenterLine(tz_line, &y, displayFontDetail(), hint_fg, bg);

  if (show_weather) {
    y += kSectionGap;
    drawCurrentWeatherRow(&y, accent_fg, bg);
  }

  y += kHintsTopGap;

  drawCenterLine("Swipe up — Radar", &y, displayFontDetail(), hint_fg, bg);
  drawCenterLine("Swipe right — Forecast", &y, displayFontDetail(), hint_fg, bg);
  drawCenterLine("Swipe left — Clock settings", &y, displayFontDetail(), hint_fg, bg);

  tft.setTextDatum(TextDatum::TopLeft);
  tft.endOffscreen();
}

}  // namespace ui
