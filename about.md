# Force 120 FPS

Runs Geometry Dash at **120 FPS** on Windows, macOS, Android and iOS.

## What it does

- **PC / Mac:** unlocks the frame limiter to 120 and turns VSync off (toggle in settings), so 120 FPS works even on a 60 Hz monitor.
- **Android:** asks Android to switch the screen to 120 Hz for the game, lifts the launcher's *Limit Frame Rate* setting if it's lower, and caps 144/165 Hz screens to exactly 120.
- **iOS:** requests 120 Hz from the display and tells you which Geode launcher option to turn on if it's still stuck at 60.
- Keeps re-applying itself when the game or the OS tries to reset the frame rate (level load, settings menu, coming back from the background).
- If something outside the mod is still holding you back, a one-time notification tells you exactly what to change.

## Things the mod can't change

A screen can't show more frames than it refreshes. You need a 120 Hz (or faster) display:

- **Android:** most flagship and many mid-range phones. Battery Saver and the phone's display refresh setting can force 60 Hz — the mod will tell you if that's happening.
- **iOS:** ProMotion devices only (iPhone 13 Pro and newer Pro models, iPad Pro). Apple's OpenGL is capped at 60, so you also need **Force ANGLE** and **Enable 120hz** on in the Geode app's settings, and *Settings → Accessibility → Motion → Limit Frame Rate* off.
