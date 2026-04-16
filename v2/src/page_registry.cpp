#include "page_registry.h"

static std::vector<Page> s_pages;

void ui_register_page(const Page& page) {
    s_pages.push_back(page);
}

const std::vector<Page>& ui_get_pages() {
    return s_pages;
}
