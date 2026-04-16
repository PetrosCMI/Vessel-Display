#pragma once
#include "lvgl.h"
#include <vector>
#include <functional>

struct Page {
    const char* tab_label;
    std::function<void(lv_obj_t*)> build_fn;
    std::function<void()>          update_fn;
};

void ui_register_page(const Page& page);
const std::vector<Page>& ui_get_pages();
