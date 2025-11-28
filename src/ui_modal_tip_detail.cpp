// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_modal_tip_detail.h"

#include <spdlog/spdlog.h>

bool TipDetailModal::show(lv_obj_t* parent, const std::string& title, const std::string& content) {
    // Store strings so they remain valid for XML attribute binding
    title_ = title;
    content_ = content;

    const char* attrs[] = {"title", title_.c_str(), "content", content_.c_str(), nullptr};

    spdlog::debug("[{}] Showing tip: {}", get_name(), title_);

    return ModalBase::show(parent, attrs);
}

void TipDetailModal::on_show() {
    // Wire up the Ok button to close the modal
    wire_ok_button("btn_ok");
}
