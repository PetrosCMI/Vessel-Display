#pragma once
#include <lvgl.h>
#include <vector>
#include <functional>

// ─────────────────────────────────────────────────────────────
//  PAGE REGISTRY
//
//  To add a new instrument page:
//  1. Create src/page_mypage.cpp
//  2. Define build_fn, update_fn
//  3. Call ui_register_page() from a static constructor or setup
//  4. That's it — the tab appears automatically
//
//  Pages are displayed in registration order.
// ─────────────────────────────────────────────────────────────

struct Page {
    const char* tab_label;                       // e.g. LV_SYMBOL_GPS " NAV"
    std::function<void(lv_obj_t* tab)> build_fn; // called once to populate tab
    std::function<void()>             update_fn; // called every 250ms
};

// Register a page (call before ui_init)
void ui_register_page(const Page& page);

// Get all registered pages
const std::vector<Page>& ui_get_pages();
