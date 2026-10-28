# Force 120 FPS

Runs Geometry Dash at **120 FPS** on Windows, macOS and Android — even on 60 Hz screens.

## What it does

- **PC / Mac:** unlocks the frame limiter to 120 and turns VSync off, so the game runs at 120 FPS on any monitor.
- **Android:**
  - Asks Android to switch the screen to 120 Hz when the phone supports it.
  - **Run past screen refresh** (on by default): stops the game from waiting on the screen each frame and paces it at exactly 120 itself. On a 60 Hz phone the game still updates, reads your taps and draws at 120 FPS — the screen just shows the newest frame each time it refreshes.
  - Lifts the launcher's *Limit Frame Rate* setting if it's lower, and holds 144/165 Hz phones at exactly 120.
- **Phones:** adds a **Graphics** button to Settings (GD only shows it on PC) with an **FPS** box + Apply, a **VSync** toggle, and GD's own Advanced graphics page.
- Measures the real FPS. If the game isn't hitting the target, a one-time notification says why and what to change.
- Keeps re-applying itself when the game or the OS resets the frame rate.

## Good to know

- On a screen slower than 120 Hz you won't *see* every frame, but taps and timing are handled at the full rate.
- Running past the screen uses more battery and can warm up the phone. Turn it off in the mod's settings if you prefer.
- A few phones' graphics drivers refuse to run faster than the screen; the mod will tell you if yours is one of them.
