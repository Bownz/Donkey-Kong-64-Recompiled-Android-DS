#include "elements/ui_theme.h"
#include "theme.h"

void recomptheme::set_custom_theme() {
    using namespace recompui;

    theme::set_theme_color(theme::color::Background1, Color{10, 10, 11, 255});
    theme::set_theme_color(theme::color::Background2, Color{19, 20, 21, 255});
    theme::set_theme_color(theme::color::Background3, Color{27, 27, 29, 255});
    theme::set_theme_color(theme::color::BGOverlay, Color{199, 200, 204, 26}); // actually the overlay for the modal
    theme::set_theme_color(theme::color::ModalOverlay, Color{10, 10, 11, 242}); // actually the background color of the modal itself
    theme::set_theme_color(theme::color::BGShadow, Color{0, 0, 0, 89});
    theme::set_theme_color(theme::color::BGShadow2, Color{10, 10, 11, 184});
    theme::set_theme_color(theme::color::Text, Color{242, 242, 242, 255});
    theme::set_theme_color(theme::color::TextActive, Color{245, 245, 245, 255});
    theme::set_theme_color(theme::color::TextDim, Color{204, 204, 204, 255});
    theme::set_theme_color(theme::color::TextInactive, Color{255, 255, 255, 153});
    theme::set_theme_color(theme::color::TextA5, Color{242, 242, 242, 13});
    theme::set_theme_color(theme::color::TextA20, Color{242, 242, 242, 51});
    theme::set_theme_color(theme::color::TextA30, Color{242, 242, 242, 77});
    theme::set_theme_color(theme::color::TextA50, Color{242, 242, 242, 128});
    theme::set_theme_color(theme::color::TextA80, Color{242, 242, 242, 204});
    theme::set_theme_color(theme::color::Primary, Color{29, 93, 226, 255});
    theme::set_theme_color(theme::color::PrimaryL, Color{167, 191, 241, 255});
    theme::set_theme_color(theme::color::PrimaryD, Color{0, 38, 117, 255});
    theme::set_theme_color(theme::color::PrimaryA5, Color{29, 93, 226, 13});
    theme::set_theme_color(theme::color::PrimaryA20, Color{29, 93, 226, 51});
    theme::set_theme_color(theme::color::PrimaryA30, Color{29, 93, 226, 77});
    theme::set_theme_color(theme::color::PrimaryA50, Color{29, 93, 226, 128});
    theme::set_theme_color(theme::color::PrimaryA80, Color{29, 93, 226, 204});
    theme::set_theme_color(theme::color::Secondary, Color{247, 158, 8, 255});
    theme::set_theme_color(theme::color::SecondaryL, Color{255, 215, 148, 255});
    theme::set_theme_color(theme::color::SecondaryD, Color{224, 141, 0, 255});
    theme::set_theme_color(theme::color::SecondaryA5, Color{247, 158, 8, 13});
    theme::set_theme_color(theme::color::SecondaryA20, Color{247, 158, 8, 51});
    theme::set_theme_color(theme::color::SecondaryA30, Color{247, 158, 8, 77});
    theme::set_theme_color(theme::color::SecondaryA50, Color{247, 158, 8, 128});
    theme::set_theme_color(theme::color::SecondaryA80, Color{247, 158, 8, 204});
    theme::set_theme_color(theme::color::Warning, Color{255, 254, 0, 255});
    theme::set_theme_color(theme::color::WarningL, Color{255, 254, 143, 255});
    theme::set_theme_color(theme::color::WarningD, Color{197, 163, 2, 255});
    theme::set_theme_color(theme::color::WarningA5, Color{255, 254, 0, 13});
    theme::set_theme_color(theme::color::WarningA20, Color{255, 254, 0, 51});
    theme::set_theme_color(theme::color::WarningA30, Color{255, 254, 0, 77});
    theme::set_theme_color(theme::color::WarningA50, Color{255, 254, 0, 128});
    theme::set_theme_color(theme::color::WarningA80, Color{255, 254, 0, 204});
    theme::set_theme_color(theme::color::Danger, Color{255, 53, 31, 255});
    theme::set_theme_color(theme::color::DangerL, Color{255, 149, 138, 255});
    theme::set_theme_color(theme::color::DangerD, Color{163, 16, 0, 255});
    theme::set_theme_color(theme::color::DangerA5, Color{255, 53, 31, 13});
    theme::set_theme_color(theme::color::DangerA20, Color{255, 53, 31, 51});
    theme::set_theme_color(theme::color::DangerA30, Color{255, 53, 31, 77});
    theme::set_theme_color(theme::color::DangerA50, Color{255, 53, 31, 128});
    theme::set_theme_color(theme::color::DangerA80, Color{255, 53, 31, 204});
    theme::set_theme_color(theme::color::Success, Color{40, 238, 32, 255});
    theme::set_theme_color(theme::color::SuccessL, Color{155, 247, 151, 255});
    theme::set_theme_color(theme::color::SuccessD, Color{18, 157, 12, 255});
    theme::set_theme_color(theme::color::SuccessA5, Color{40, 238, 32, 13});
    theme::set_theme_color(theme::color::SuccessA20, Color{40, 238, 32, 51});
    theme::set_theme_color(theme::color::SuccessA30, Color{40, 238, 32, 77});
    theme::set_theme_color(theme::color::SuccessA50, Color{40, 238, 32, 128});
    theme::set_theme_color(theme::color::SuccessA80, Color{40, 238, 32, 204});
    theme::set_theme_color(theme::color::Border, Color{255, 255, 255, 51});
    theme::set_theme_color(theme::color::BorderSoft, Color{255, 255, 255, 26});
    theme::set_theme_color(theme::color::BorderHard, Color{255, 255, 255, 77});
    theme::set_theme_color(theme::color::BorderSolid, Color{255, 255, 255, 153});
    theme::set_theme_color(theme::color::Transparent, Color{0, 0, 0, 0});
    theme::set_theme_color(theme::color::A, Color{51, 51, 255, 255});
    theme::set_theme_color(theme::color::AL, Color{178, 178, 255, 255});
    theme::set_theme_color(theme::color::AD, Color{32, 32, 172, 255});
    theme::set_theme_color(theme::color::AA5, Color{51, 51, 255, 13});
    theme::set_theme_color(theme::color::AA20, Color{51, 51, 255, 51});
    theme::set_theme_color(theme::color::AA30, Color{51, 51, 255, 77});
    theme::set_theme_color(theme::color::AA50, Color{51, 51, 255, 128});
    theme::set_theme_color(theme::color::AA80, Color{51, 51, 255, 204});
    theme::set_theme_color(theme::color::White, Color{255, 255, 255, 255});
    theme::set_theme_color(theme::color::WhiteA5, Color{255, 255, 255, 13});
    theme::set_theme_color(theme::color::WhiteA20, Color{255, 255, 255, 51});
    theme::set_theme_color(theme::color::WhiteA30, Color{255, 255, 255, 77});
    theme::set_theme_color(theme::color::WhiteA50, Color{255, 255, 255, 128});
    theme::set_theme_color(theme::color::WhiteA80, Color{255, 255, 255, 204});
    theme::set_theme_color(theme::color::BW05, Color{13, 13, 13, 255});
    theme::set_theme_color(theme::color::BW10, Color{26, 26, 26, 255});
    theme::set_theme_color(theme::color::BW25, Color{64, 64, 64, 255});
    theme::set_theme_color(theme::color::BW50, Color{128, 128, 128, 255});
    theme::set_theme_color(theme::color::BW75, Color{191, 191, 191, 255});
    theme::set_theme_color(theme::color::BW90, Color{229, 229, 229, 255});

#ifdef __ANDROID__
    // Android: the bottom screen's DK look - jungle-green glass panels (the frame, wooden tab
    // bar and option rows are set in the RecompFrontend patch), banana-yellow accent, cream text.
    {
        const Color banana{249, 214, 72, 255}, bananaL{255, 236, 150, 255}, bananaD{176, 128, 20, 255};
        auto alpha = [](Color c, uint8_t a) { c.a = a; return c; };
        theme::set_theme_color(theme::color::Background1, Color{8, 20, 5, 255});
        theme::set_theme_color(theme::color::Background2, Color{19, 40, 14, 255});
        theme::set_theme_color(theme::color::Background3, Color{30, 58, 22, 255});
        theme::set_theme_color(theme::color::BGOverlay, Color{0, 0, 0, 90});
        theme::set_theme_color(theme::color::ModalOverlay, Color{14, 26, 10, 226});
        theme::set_theme_color(theme::color::BGShadow, Color{42, 20, 4, 110});
        theme::set_theme_color(theme::color::BGShadow2, Color{8, 20, 5, 184});
        theme::set_theme_color(theme::color::Text, Color{255, 244, 214, 255});
        theme::set_theme_color(theme::color::TextActive, Color{255, 250, 232, 255});
        theme::set_theme_color(theme::color::TextDim, Color{232, 213, 168, 255});
        theme::set_theme_color(theme::color::TextInactive, Color{232, 213, 168, 170});
        theme::set_theme_color(theme::color::TextA5, Color{255, 244, 214, 13});
        theme::set_theme_color(theme::color::TextA20, Color{255, 244, 214, 51});
        theme::set_theme_color(theme::color::TextA30, Color{255, 244, 214, 77});
        theme::set_theme_color(theme::color::TextA50, Color{255, 244, 214, 128});
        theme::set_theme_color(theme::color::TextA80, Color{255, 244, 214, 204});
        theme::set_theme_color(theme::color::Primary, banana);
        theme::set_theme_color(theme::color::PrimaryL, bananaL);
        theme::set_theme_color(theme::color::PrimaryD, bananaD);
        theme::set_theme_color(theme::color::PrimaryA5, alpha(banana, 13));
        theme::set_theme_color(theme::color::PrimaryA20, alpha(banana, 51));
        theme::set_theme_color(theme::color::PrimaryA30, alpha(banana, 77));
        theme::set_theme_color(theme::color::PrimaryA50, alpha(banana, 128));
        theme::set_theme_color(theme::color::PrimaryA80, alpha(banana, 204));
        const Color gold{217, 165, 32, 255};
        theme::set_theme_color(theme::color::Border, alpha(gold, 90));
        theme::set_theme_color(theme::color::BorderSoft, alpha(gold, 50));
        theme::set_theme_color(theme::color::BorderHard, alpha(gold, 140));
        theme::set_theme_color(theme::color::BorderSolid, alpha(gold, 210));
        const Color wood{122, 78, 38, 255};
        theme::set_theme_color(theme::color::Elevated, alpha(wood, 150));
        theme::set_theme_color(theme::color::ElevatedSoft, alpha(wood, 80));
        theme::set_theme_color(theme::color::ElevatedBorder, alpha(gold, 170));
        theme::set_theme_color(theme::color::ElevatedBorderHard, gold);
        theme::set_theme_color(theme::color::A, banana);
        theme::set_theme_color(theme::color::AL, bananaL);
        theme::set_theme_color(theme::color::AD, bananaD);
        theme::set_theme_color(theme::color::AA5, alpha(banana, 13));
        theme::set_theme_color(theme::color::AA20, alpha(banana, 51));
        theme::set_theme_color(theme::color::AA30, alpha(banana, 77));
        theme::set_theme_color(theme::color::AA50, alpha(banana, 128));
        theme::set_theme_color(theme::color::AA80, alpha(banana, 204));
    }
#endif

    theme::border::radius_sm = 12.0f;
    theme::border::radius_md = 18.0f;
    theme::border::radius_lg = 24.0f;

    const uint32_t header_weight = 700;
#ifdef __ANDROID__
    const uint32_t label_weight = 700;   // bold condensed labels, as in Dusklight
#else
    const uint32_t label_weight = 500;
#endif
    const uint32_t base_weight = 400;
    theme::set_typography_preset(theme::Typography::Header1, 68.0f, 0.01f, header_weight);
    theme::set_typography_preset(theme::Typography::Header2, 52.0f, 0.01f, header_weight);
    theme::set_typography_preset(theme::Typography::Header3, 36.0f, 0.01f, header_weight);
    theme::set_typography_preset(theme::Typography::LabelLG, 36.0f, 0.05f, label_weight);
    theme::set_typography_preset(theme::Typography::LabelMD, 28.0f, 0.05f, label_weight);
    theme::set_typography_preset(theme::Typography::LabelSM, 20.0f, 0.07f, label_weight);
    theme::set_typography_preset(theme::Typography::LabelXS, 18.0f, 0.07f, base_weight);
    theme::set_typography_preset(theme::Typography::Body,    20.0f, 0,     base_weight);
};


