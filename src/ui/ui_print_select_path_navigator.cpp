// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_print_select_path_navigator.h"

namespace helix::ui {

void PrintSelectPathNavigator::navigate_to(const std::string& dirname) {
    if (current_path_.empty()) {
        current_path_ = dirname;
    } else {
        current_path_ = current_path_ + "/" + dirname;
    }
}

void PrintSelectPathNavigator::navigate_up() {
    if (current_path_.empty()) {
        return; // Already at root
    }

    size_t last_slash = current_path_.rfind('/');
    if (last_slash == std::string::npos) {
        // No slash - one level deep, go to root
        current_path_.clear();
    } else {
        // Truncate at last slash
        current_path_ = current_path_.substr(0, last_slash);
    }
}

} // namespace helix::ui
