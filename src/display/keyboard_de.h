#pragma once
// =====================================================================
//  keyboard_de.h  —  German QWERTZ keyboard layout for lv_keyboard,
//                    plus a visible-cursor style for lv_textarea.
//
//  The custom ui_font_ms14m font (range 0x20..0xFF) carries the German
//  Latin-1 glyphs (ä ö ü ß Ä Ö Ü) that stock LV_FONT_MONTSERRAT_14 is
//  missing. To still render the LV_SYMBOL_* (Font-Awesome) keys for
//  backspace / arrows / OK / close we install Montserrat as a fallback
//  font on ui_font_ms14m. The fallback link is set up lazily on first
//  use so we don't have to touch global init code.
// =====================================================================
#include <lvgl.h>

extern "C" const lv_font_t ui_font_ms14m;

namespace kb_de {

// A RAM-resident copy of ui_font_ms14m with a Montserrat fallback so the
// LV_SYMBOL_* (Font-Awesome) glyphs used as keyboard control keys render.
//
// IMPORTANT: ui_font_ms14m itself lives in flash (.rodata), so writing to
// its `fallback` field via const_cast triggers a load/store fault on ESP32-S3
// and the device reboots silently. We MUST copy the struct into RAM first
// and patch the copy.
inline const lv_font_t* fontWithFallback() {
    static lv_font_t f;            // RAM copy, zero-initialised
    static bool inited = false;
    if (!inited) {
        f = ui_font_ms14m;                       // shallow-copy from flash
        f.fallback = &lv_font_montserrat_14;     // safe: writing to RAM
        inited = true;
    }
    return &f;
}

// QWERTZ + German umlauts. Keep LV_SYMBOL_* values intact so the stock
// lv_keyboard event handler still recognises the control keys.
inline const char* const* lowerMap() {
    static const char* const m[] = {
        "1#",  "q","w","e","r","t","z","u","i","o","p", "\xc3\xbc",  LV_SYMBOL_BACKSPACE, "\n",
        "ABC", "a","s","d","f","g","h","j","k","l", "\xc3\xb6", "\xc3\xa4",                "\n",
        "_",   "-","y","x","c","v","b","n","m", "\xc3\x9f", ",", ".",                       "\n",
        LV_SYMBOL_CLOSE, LV_SYMBOL_LEFT, " ", LV_SYMBOL_RIGHT, LV_SYMBOL_OK, ""
    };
    return m;
}
inline const char* const* upperMap() {
    static const char* const m[] = {
        "1#",  "Q","W","E","R","T","Z","U","I","O","P", "\xc3\x9c", LV_SYMBOL_BACKSPACE, "\n",
        "abc", "A","S","D","F","G","H","J","K","L", "\xc3\x96", "\xc3\x84",                "\n",
        "_",   "-","Y","X","C","V","B","N","M", "\xc3\x9f", ",", ".",                       "\n",
        LV_SYMBOL_CLOSE, LV_SYMBOL_LEFT, " ", LV_SYMBOL_RIGHT, LV_SYMBOL_OK, ""
    };
    return m;
}

// Control map matched to the layouts above. LV_KEYBOARD_CTRL_BUTTON_FLAGS
// marks buttons the keyboard handles itself (mode-switch, OK, close, etc.).
inline const lv_buttonmatrix_ctrl_t* ctrlMap() {
    using C = lv_buttonmatrix_ctrl_t;
    // Row counts MUST match the maps in lowerMap()/upperMap():
    //   row 1 = 13 keys  (1#  q w e r t z u i o p  ü  BSPC)
    //   row 2 = 12 keys  (ABC a s d f g h j k l  ö  ä)
    //   row 3 = 12 keys  (_   - y x c v b n m  ß  , .)
    //   row 4 =  5 keys  (CLOSE  LEFT  SPACE  RIGHT  OK)
    // A mismatch makes lv_buttonmatrix walk past the ctrl array and crash.
    static const lv_buttonmatrix_ctrl_t c[] = {
        C(LV_KEYBOARD_CTRL_BUTTON_FLAGS | 6),
            C(6),C(6),C(6),C(6),C(6),C(6),C(6),C(6),C(6),C(6),C(6),
        C(LV_KEYBOARD_CTRL_BUTTON_FLAGS | 6),

        C(LV_KEYBOARD_CTRL_BUTTON_FLAGS | 7),
            C(6),C(6),C(6),C(6),C(6),C(6),C(6),C(6),C(6),C(6),C(6),

        C(LV_KEYBOARD_CTRL_BUTTON_FLAGS | 6),
            C(6),C(6),C(6),C(6),C(6),C(6),C(6),C(6),C(6),C(6),C(6),

        C(LV_KEYBOARD_CTRL_BUTTON_FLAGS | 4),
        C(LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2),
        C(12),
        C(LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2),
        C(LV_KEYBOARD_CTRL_BUTTON_FLAGS | 4)
    };
    return c;
}

// Install the German maps and the font that can render them (with the
// FA-fallback chain) onto an existing lv_keyboard.
inline void applyGermanLayout(lv_obj_t* kb) {
    const lv_font_t* font = fontWithFallback();
    lv_obj_set_style_text_font(kb, font, 0);
    lv_obj_set_style_text_font(kb, font, LV_PART_ITEMS);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_LOWER,
                        lowerMap(), ctrlMap());
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_UPPER,
                        upperMap(), ctrlMap());
}

// Make the textarea cursor a bright, blinking vertical bar so the user
// can always see where typing will go. Works on dark and light themes.
inline void applyVisibleCursor(lv_obj_t* ta) {
    const lv_color_t cursor = lv_color_hex(0x1E90FF);   // dodger blue
    lv_obj_set_style_bg_color(ta, cursor, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(ta, LV_OPA_70, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(ta, lv_color_white(),
                                LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(ta, cursor,
                                  LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(ta, 0,
                                  LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_anim_duration(ta, 500,
                                   LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_textarea_set_cursor_click_pos(ta, true);
    lv_obj_add_state(ta, LV_STATE_FOCUSED);
}

} // namespace kb_de
