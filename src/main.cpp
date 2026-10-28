#include <Geode/Geode.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/AppDelegate.hpp>
#include <cmath>
#include "platform.hpp"

using namespace geode::prelude;

// ---------------------------------------------------------------------------
// Core: push the target frame rate into every place GD / cocos reads it from.
// ---------------------------------------------------------------------------

static double targetFPS() {
    auto fps = Mod::get()->getSettingValue<int64_t>("target-fps");
    return fps > 0 ? static_cast<double>(fps) : 120.0;
}

static void applyFPS() {
    double fps = targetFPS();
    double interval = 1.0 / fps;

    if (auto gm = GameManager::get()) {
        // Keep GD's own idea of the target in sync so its FPS logic agrees.
        gm->m_customFPSTarget = static_cast<float>(fps);

#ifdef GEODE_IS_DESKTOP
        if (Mod::get()->getSettingValue<bool>("disable-vsync")) {
            gm->m_vsyncEnabled = false;
            PlatformToolbox::toggleVerticalSync(false);
        }
#endif
    }

    if (fps_platform::controlsDirectorInterval()) {
        // Director interval drives scheduling/timing.
        CCDirector::sharedDirector()->setAnimationInterval(interval);
        // The application interval is the actual frame limiter on Windows/Mac.
        CCApplication::sharedApplication()->setAnimationInterval(interval);
    }

    // Phones: get the screen itself to refresh at the target rate.
    fps_platform::apply(fps);
}

// ---------------------------------------------------------------------------
// Watchdog: GD and the OS reset the frame rate in a few places (settings
// menu, level load, returning from background, surface recreation). Check
// twice a second; re-request the screen rate every couple of seconds.
// ---------------------------------------------------------------------------

class FPSEnforcer : public CCObject {
    int m_ticks = 0;
    bool m_hintShown = false;

public:
    static FPSEnforcer* get() {
        static FPSEnforcer* instance = [] {
            auto e = new FPSEnforcer();
            e->autorelease();
            e->retain(); // lives for the whole session
            return e;
        }();
        return instance;
    }

    void tick(float) {
        m_ticks++;
        double fps = targetFPS();

        bool drifted = false;
        if (fps_platform::controlsDirectorInterval()) {
            double have = CCDirector::sharedDirector()->getAnimationInterval();
            drifted = std::fabs(have - 1.0 / fps) > 1e-7;
        }

        // Every 2 seconds on phones, re-assert the screen refresh request
        // (Android forgets it whenever the surface is recreated).
        bool periodic = false;
#ifdef GEODE_IS_MOBILE
        periodic = m_ticks % 4 == 0;
#endif
        if (drifted || periodic) {
            applyFPS();
        }

        // A few seconds in, tell the player if something outside the mod is
        // still holding the frame rate back. Once per launch.
        if (!m_hintShown && m_ticks == 6) {
            m_hintShown = true;
            auto hint = fps_platform::blockerHint(fps);
            if (!hint.empty()) {
                log::warn("{}", hint);
                Notification::create(hint, NotificationIcon::Warning, 6.f)->show();
            }
        }
    }

    void start() {
        CCDirector::sharedDirector()->getScheduler()->scheduleSelector(
            schedule_selector(FPSEnforcer::tick), this, 0.5f, false
        );
    }
};

// ---------------------------------------------------------------------------
// Hooks
// ---------------------------------------------------------------------------

// GD's central "apply the FPS setting" function. Let it run, then override.
class $modify(Force120GameManager, GameManager) {
    void updateCustomFPS() {
        GameManager::updateCustomFPS();
        applyFPS();
    }
};

// Phones reset the display when the app comes back from the background.
class $modify(Force120AppDelegate, AppDelegate) {
    void applicationWillEnterForeground() {
        AppDelegate::applicationWillEnterForeground();
        queueInMainThread([] { applyFPS(); });
    }
};

// ---------------------------------------------------------------------------
// Startup + live setting changes
// ---------------------------------------------------------------------------

$on_mod(Loaded) {
    queueInMainThread([] {
        applyFPS();
        FPSEnforcer::get()->start();
        auto max = fps_platform::maxRefreshRate();
        if (max > 0) {
            log::info("Force 120 FPS active: target {} FPS, screen max {} Hz", targetFPS(), max);
        } else {
            log::info("Force 120 FPS active: target {} FPS", targetFPS());
        }
    });

    listenForSettingChanges<int64_t>("target-fps", [](int64_t) { applyFPS(); });
    listenForSettingChanges<bool>("disable-vsync", [](bool enabled) {
#ifdef GEODE_IS_DESKTOP
        if (!enabled) {
            if (auto gm = GameManager::get()) gm->m_vsyncEnabled = true;
            PlatformToolbox::toggleVerticalSync(true);
        }
#endif
        applyFPS();
    });
}
