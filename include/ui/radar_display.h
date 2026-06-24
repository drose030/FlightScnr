#pragma once

namespace ui {

/** Draw the static sonar/radar grid (black disc, green overlay, labels). */
void radarDisplayDraw();

/** Compose grid + aircraft + sweep in RAM and push one frame to the panel. */
void radarDisplayRefreshSweep();

/** Note ADS-B aircraft changes; panel updates on the next sweep frame. */
void radarDisplayRefreshAircraft();

}  // namespace ui
