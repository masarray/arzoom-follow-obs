# ArZoom — Smart Camera Zoom & Follow for OBS Studio

ArZoom turns a Display Capture into a smooth presentation camera for tutorials, training, coding, engineering, and screen recordings.

## What makes ArZoom different

ArZoom is not a raw mouse follower. Its Smart Zone camera follows meaningful presentation-area changes while staying steady during local explanation movement.

Current highlights:

- Smart Zone → Follow / Catch-Up → Coast → SmoothIdle camera behavior;
- minimum-jerk camera motion and straight screen-space zoom trajectories;
- premium GPU dual-vector click feedback;
- Presenter Controls with Hold Zoom, Freeze Camera, Smart Follow On/Off, Zoom ±, Reset / Full Frame, and Overview Peek;
- optional Presentation Cursor with built-in ArZoom cursor styles and Advanced custom cursor support;
- built-in cursor clicks use short tactile micro-interactions: fast press, elastic release, tiny rebound, exact settle;
- Standard OBS and OBS Portable/custom-root Windows installer support.

## Presentation Cursor

The normal workflow does not require importing an image file. In the ArZoom filter, select **Cursor Style**:

- Off — captured system cursor;
- ArZoom Prism — 3D cyan;
- ArZoom Outline — professional monochrome;
- ArZoom Azure — technical blue;
- ArZoom Orchid — pastel blue + violet;
- Custom… — Advanced user-provided GIF/WebP/PNG.

Built-in styles own their hotspot and animation timing automatically. Their click gestures are intentionally brief (~220–280 ms) so they feel like tactile UI feedback rather than decorative animation. Cursor size magnifies with the ArZoom camera.

If a Presentation Cursor style is enabled, disable native cursor capture on the underlying Display Capture to avoid seeing two cursors.

## Installation

Use the Windows installer from the current GitHub release, or extract the manual ZIP into the matching OBS installation root.

Restart OBS after installation or update.

## Project status

ArZoom is developed phase-by-phase with deterministic motion gates, Windows CI, and direct OBS visual trials before visual features are declared stable.

See the GitHub issues and North Star roadmap for current development status.
