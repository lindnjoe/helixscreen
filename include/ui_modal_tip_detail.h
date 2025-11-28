// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ui_modal_base.h"

#include <string>

/**
 * @file ui_modal_tip_detail.h
 * @brief Modal dialog for displaying printing tip details
 *
 * Shows a tip title and scrollable content with an Ok button to close.
 * Used by HomePanel when user clicks on the tip-of-the-day text.
 *
 * ## Usage:
 *
 * @code
 * TipDetailModal tip_modal;
 * tip_modal.show(lv_screen_active(), "Bed Adhesion", "First layer adhesion is...");
 * // Modal closes when Ok clicked, backdrop clicked, or ESC pressed
 * @endcode
 *
 * @see ModalBase for base class documentation
 */
class TipDetailModal : public ModalBase {
  public:
    TipDetailModal() = default;
    ~TipDetailModal() override = default;

    /**
     * @brief Show the tip detail modal
     *
     * @param parent Parent object (usually lv_screen_active())
     * @param title Tip title text
     * @param content Tip content text (can be long, will scroll)
     * @return true if modal was created successfully
     */
    bool show(lv_obj_t* parent, const std::string& title, const std::string& content);

    //
    // === ModalBase Implementation ===
    //

    const char* get_name() const override {
        return "Tip Detail Modal";
    }
    const char* get_xml_component_name() const override {
        return "tip_detail_dialog";
    }

  protected:
    void on_show() override;

  private:
    // Store strings so they remain valid during modal lifetime
    std::string title_;
    std::string content_;
};
