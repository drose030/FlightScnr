#pragma once

#include <cstddef>
#include <cstdint>

namespace services::clock {

void bootLoad();

/** Apply persisted timezone and start NTP (call when Wi-Fi is up). */
void startNtp();

/** Local-time minute bucket for display refresh; UINT32_MAX while unsynced. */
uint32_t localMinuteStamp();

/** Fixed offset from UTC in seconds (no automatic DST). */
int32_t timezoneOffsetSec();
void stepTimezoneHours(int8_t delta);

bool use24Hour();
void toggleHourFormat();

void formatTimezoneLabel(char* out, size_t len);
void formatTimeOfDay(char* out, size_t len);
/** Format a UTC epoch (seconds) as local "H:MM[ AM/PM]" using the saved offset
 *  and 12/24-hour preference. Writes "--:--" when the epoch is invalid. */
void formatClockFromEpoch(int64_t utc_epoch_sec, char* out, size_t len);
void formatDateLine(char* out, size_t len);
/** "AM"/"PM" when 12-hour mode; empty string in 24-hour mode. */
void formatAmPm(char* out, size_t len);

}  // namespace services::clock
