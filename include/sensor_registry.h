// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "hv/json.hpp"

namespace helix::sensors {

/// @brief Interface for sensor category managers
///
/// Sensors come from three different sources:
/// - Klipper objects (printer.objects.list) - humidity, probe, switch sensors
/// - Klipper config (configfile.config) - accelerometers (no get_status method)
/// - Moonraker APIs - color sensors (TD-1)
///
/// Managers implement only the discovery methods for their data source.
class ISensorManager {
  public:
    virtual ~ISensorManager() = default;

    /// @brief Get the category name (e.g., "switch", "humidity")
    [[nodiscard]] virtual std::string category_name() const = 0;

    /// @brief Discover sensors from Klipper object list (printer.objects.list)
    /// @note Default implementation is no-op for managers that don't use this source
    virtual void discover(const std::vector<std::string>& klipper_objects) {
        (void)klipper_objects;
    }

    /// @brief Discover sensors from Klipper config (configfile.config keys)
    /// @note Use this for sensors that exist in config but not in objects list
    /// @note Default implementation is no-op for managers that don't use this source
    virtual void discover_from_config(const nlohmann::json& config_keys) {
        (void)config_keys;
    }

    /// @brief Discover sensors from Moonraker API info
    /// @note Use this for sensors that come from Moonraker, not Klipper
    /// @note Default implementation is no-op for managers that don't use this source
    virtual void discover_from_moonraker(const nlohmann::json& moonraker_info) {
        (void)moonraker_info;
    }

    /// @brief Inject mock sensor objects for testing UI
    /// @param objects Klipper objects list to append to
    /// @param config_keys Config keys to append to
    /// @param moonraker_info Moonraker info to append to
    /// @note Called by SensorRegistry when should_mock_sensors() is true
    virtual void inject_mock_sensors(std::vector<std::string>& objects, nlohmann::json& config_keys,
                                     nlohmann::json& moonraker_info) {
        (void)objects;
        (void)config_keys;
        (void)moonraker_info;
    }

    /// @brief Inject mock status data for testing UI
    /// @param status Status JSON to append to
    /// @note Called by SensorRegistry when should_mock_sensors() is true
    virtual void inject_mock_status(nlohmann::json& status) { (void)status; }

    /// @brief Update state from Moonraker status JSON
    virtual void update_from_status(const nlohmann::json& status) = 0;

    /// @brief Load configuration from JSON
    virtual void load_config(const nlohmann::json& config) = 0;

    /// @brief Save configuration to JSON
    [[nodiscard]] virtual nlohmann::json save_config() const = 0;
};

/// @brief Central registry for all sensor managers
class SensorRegistry {
  public:
    SensorRegistry() = default;
    ~SensorRegistry() = default;

    // Non-copyable, non-movable (contains mutex)
    SensorRegistry(const SensorRegistry&) = delete;
    SensorRegistry& operator=(const SensorRegistry&) = delete;
    SensorRegistry(SensorRegistry&&) = delete;
    SensorRegistry& operator=(SensorRegistry&&) = delete;

    /// @brief Register a sensor manager
    void register_manager(std::string category, std::unique_ptr<ISensorManager> manager);

    /// @brief Get a manager by category name
    [[nodiscard]] ISensorManager* get_manager(const std::string& category) const;

    /// @brief Discover sensors in all registered managers from all sources
    /// @param klipper_objects Objects from printer.objects.list
    /// @param config_keys Keys from configfile.config (for accelerometers)
    /// @param moonraker_info Info from Moonraker APIs (for TD-1 color sensors)
    void discover_all(const std::vector<std::string>& klipper_objects,
                      const nlohmann::json& config_keys = nlohmann::json::object(),
                      const nlohmann::json& moonraker_info = nlohmann::json::object());

    /// @brief Route status update to all managers
    void update_all_from_status(const nlohmann::json& status);

    /// @brief Load config for all managers
    void load_config(const nlohmann::json& root_config);

    /// @brief Save config from all managers
    [[nodiscard]] nlohmann::json save_config() const;

  private:
    mutable std::recursive_mutex mutex_;
    std::map<std::string, std::unique_ptr<ISensorManager>> managers_;
};

} // namespace helix::sensors
