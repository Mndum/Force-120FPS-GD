#include <Geode/Geode.hpp>

#ifdef GEODE_IS_MOBILE

#include <Geode/modify/OptionsLayer.hpp>
#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;

// GD's PC builds have a Settings > Graphics screen with an FPS box and a
// VSync toggle. Phone builds strip both the button and those rows, so this
// file adds them back as a mod-made page that drives the mod's settings.

namespace {
    constexpr int64_t MIN_FPS = 30;
    constexpr int64_t MAX_FPS = 1000;

    bool vsyncOn() {
        // VSync on = wait for the screen = "run past screen refresh" off
        return !Mod::get()->getSettingValue<bool>("uncap-mobile");
    }
}

class GraphicsPopup : public geode::Popup {
protected:
    TextInput* m_fpsInput = nullptr;
    CCMenuItemToggler* m_vsyncToggle = nullptr;

    bool init() {
        if (!Popup::init(300.f, 210.f)) return false;
        this->setTitle("Graphics");

        auto menu = CCMenu::create();
        menu->setContentSize(m_size);
        menu->ignoreAnchorPointForPosition(false);
        m_mainLayer->addChildAtPosition(menu, Anchor::Center);

        float cx = m_size.width / 2;

        // --- FPS row -------------------------------------------------------
        auto fpsLabel = CCLabelBMFont::create("FPS", "bigFont.fnt");
        fpsLabel->setScale(.6f);
        fpsLabel->setPosition({ cx - 95.f, 140.f });
        menu->addChild(fpsLabel);

        m_fpsInput = TextInput::create(80.f, "120");
        m_fpsInput->setCommonFilter(CommonFilter::Uint);
        m_fpsInput->setMaxCharCount(4);
        m_fpsInput->setString(
            fmt::format("{}", Mod::get()->getSettingValue<int64_t>("target-fps"))
        );
        m_fpsInput->setPosition({ cx - 10.f, 140.f });
        menu->addChild(m_fpsInput);

        auto applySpr = ButtonSprite::create("Apply", "goldFont.fnt", "GJ_button_01.png", .7f);
        auto applyBtn = CCMenuItemSpriteExtra::create(
            applySpr, this, menu_selector(GraphicsPopup::onApplyFPS)
        );
        applyBtn->setPosition({ cx + 85.f, 140.f });
        menu->addChild(applyBtn);

        // --- VSync row -----------------------------------------------------
        m_vsyncToggle = CCMenuItemToggler::createWithStandardSprites(
            this, menu_selector(GraphicsPopup::onVSync), .8f
        );
        m_vsyncToggle->toggle(vsyncOn());
        m_vsyncToggle->setPosition({ cx - 95.f, 95.f });
        menu->addChild(m_vsyncToggle);

        auto vsyncLabel = CCLabelBMFont::create("VSync", "bigFont.fnt");
        vsyncLabel->setScale(.55f);
        vsyncLabel->setAnchorPoint({ 0.f, .5f });
        vsyncLabel->setPosition({ cx - 75.f, 95.f });
        menu->addChild(vsyncLabel);

        auto info = InfoAlertButton::create(
            "VSync",
#ifdef GEODE_IS_IOS
            "<cy>On iOS</c> the system always matches frames to the screen, so the game "
            "can't run faster than your display (120 Hz on ProMotion devices, 60 Hz on "
            "others).\n\nFor 120 Hz on ProMotion, turn on <cg>Force ANGLE</c> and "
            "<cg>Enable 120hz</c> in the Geode app's settings.",
#else
            "<cg>On</c>: the game waits for your screen each frame, so it can't go "
            "above your screen's refresh rate.\n\n<cr>Off</c>: the game runs at the FPS "
            "above even on a slower screen. Taps and timing run at the full rate; the "
            "screen shows the newest frame. Uses more battery.",
#endif
            .6f
        );
        info->setPosition({ cx + 85.f, 95.f });
        menu->addChild(info);

        // --- GD's own advanced graphics page --------------------------------
        auto advSpr = ButtonSprite::create("Advanced", "goldFont.fnt", "GJ_button_04.png", .7f);
        auto advBtn = CCMenuItemSpriteExtra::create(
            advSpr, this, menu_selector(GraphicsPopup::onAdvanced)
        );
        advBtn->setPosition({ cx, 40.f });
        menu->addChild(advBtn);

        return true;
    }

    void onApplyFPS(CCObject*) {
        auto text = std::string(m_fpsInput->getString());
        int64_t fps = 0;
        for (char c : text) {
            if (c < '0' || c > '9') break;
            fps = fps * 10 + (c - '0');
            if (fps > MAX_FPS) break;
        }
        if (fps <= 0) {
            Notification::create("Enter a frame rate", NotificationIcon::Warning)->show();
            return;
        }
        fps = std::clamp(fps, MIN_FPS, MAX_FPS);
        m_fpsInput->setString(fmt::format("{}", fps));

        // Triggers the mod's setting listener, which applies it immediately
        Mod::get()->setSettingValue<int64_t>("target-fps", fps);
        Notification::create(fmt::format("FPS set to {}", fps), NotificationIcon::Success)->show();
    }

    void onVSync(CCObject*) {
        // The toggler flips after this callback runs, so read the old state
        bool turningOn = !m_vsyncToggle->isToggled();
        Mod::get()->setSettingValue<bool>("uncap-mobile", !turningOn);

#ifdef GEODE_IS_IOS
        if (!turningOn) {
            Notification::create(
                "iOS always syncs to the screen; tap the i for how to get 120 Hz",
                NotificationIcon::Info
            )->show();
        }
#endif
    }

    void onAdvanced(CCObject*) {
        if (auto layer = MoreVideoOptionsLayer::create()) {
            layer->show();
        }
    }

public:
    static GraphicsPopup* create() {
        auto ret = new GraphicsPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// Add a "Graphics" button to Settings, under the options list.
class $modify(Force120OptionsLayer, OptionsLayer) {
    void customSetup() {
        OptionsLayer::customSetup();

        if (!m_mainLayer) return;

        auto spr = ButtonSprite::create("Graphics", "goldFont.fnt", "GJ_button_01.png", .7f);
        auto btn = CCMenuItemSpriteExtra::create(
            spr, this, menu_selector(Force120OptionsLayer::onModGraphics)
        );
        btn->setID("graphics-button"_spr);

        auto menu = CCMenu::create();
        menu->setID("graphics-menu"_spr);
        menu->addChild(btn);

        // Centered just below the options list, kept on screen
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        CCPoint pos { winSize.width / 2, 28.f };
        if (m_listLayer) {
            auto listPos = m_listLayer->getPosition();
            auto listSize = m_listLayer->getContentSize();
            pos.x = listPos.x + listSize.width / 2;
            pos.y = std::max(listPos.y - 22.f, 22.f);
        }
        menu->setPosition(pos);
        m_mainLayer->addChild(menu, 10);
    }

    void onModGraphics(CCObject*) {
        if (auto popup = GraphicsPopup::create()) {
            popup->show();
        }
    }
};

#endif
