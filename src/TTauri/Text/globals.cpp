// Copyright 2020 Pokitec
// All rights reserved.

#include "TTauri/Text/globals.hpp"
#include "TTauri/Text/ElusiveIcons.hpp"
#include "TTauri/Text/TTauriIcons.hpp"
#include "TTauri/Foundation/globals.hpp"
#include "data/UnicodeData.bin.inl"
#include "data/elusiveicons-webfont.ttf.inl"
#include "data/TTauriIcons.ttf.inl"

namespace tt {

/** Reference counter to determine the amount of startup/shutdowns.
*/
static std::atomic<uint64_t> startupCount = 0;


void text_startup()
{
    if (startupCount.fetch_add(1) != 0) {
        // The library has already been initialized.
        return;
    }

    foundation_startup();
    LOG_INFO("Text startup");

    addStaticResource(UnicodeData_bin_filename, UnicodeData_bin_bytes);
    addStaticResource(elusiveicons_webfont_ttf_filename, elusiveicons_webfont_ttf_bytes);
    addStaticResource(TTauriIcons_ttf_filename, TTauriIcons_ttf_bytes);

    unicodeData = parseResource<UnicodeData>(URL("resource:UnicodeData.bin"));

    fontBook = new FontBook(std::vector<URL>{
        URL::urlFromSystemFontDirectory()
    });
    ElusiveIcons_font_id = fontBook->register_font(URL("resource:elusiveicons-webfont.ttf"));
    TTauriIcons_font_id = fontBook->register_font(URL("resource:TTauriIcons.ttf"));
}

void text_shutdown()
{
    if (startupCount.fetch_sub(1) != 1) {
        // This is not the last instantiation.
        return;
    }
    LOG_INFO("Text shutdown");

    ElusiveIcons_font_id = FontID{};
    delete fontBook;
    unicodeData.release();

    foundation_shutdown();
}

}