#pragma once

#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

#include "Config/Config.hpp"
#include "Core/SystemInfo.hpp"

namespace draconis::ui {
  namespace types   = ::draconis::utils::types;
  namespace logging = ::draconis::utils::logging;
  namespace config  = ::draconis::config;
  namespace system  = ::draconis::core::system;

  struct Theme {
    logging::LogColor icon;
    logging::LogColor label;
    logging::LogColor value;
  };

  extern const Theme DEFAULT_THEME;

  struct Icons {
    types::StringView calendar;
    types::StringView desktopEnvironment;
    types::StringView disk;
    types::StringView host;
    types::StringView kernel;
    types::StringView memory;
    types::StringView cpu;
    types::StringView gpu;
    types::StringView uptime;
    types::StringView os;
#if DRAC_ENABLE_PACKAGECOUNT
    types::StringView package;
#endif
    types::StringView palette;
    types::StringView shell;
    types::StringView user;
    types::StringView windowManager;
  };

  extern const Icons ICON_TYPE;

  /**
   * @brief Text measurement and fitting helpers the responsive layout is built on.
   */
  namespace detail {
    /**
     * @brief Columns a string occupies once ANSI escapes are discounted.
     */
    auto GetVisualWidth(const types::StringView& str) -> types::usize;

    /**
     * @brief Cuts a string down to a visual width, marking the loss with an ellipsis.
     */
    auto TruncateToWidth(const types::StringView& str, types::usize maxWidth) -> types::String;

    /**
     * @brief Word-wraps text to a visual width, breaking tokens too long to fit.
     * @param wrapWidth Maximum visual width per line (0 = no wrap).
     */
    auto WordWrap(const types::StringView& text, types::usize wrapWidth) -> types::Vec<types::String>;
  } // namespace detail

  /**
   * @brief Creates the main UI element based on system data and configuration.
   * @param config The application configuration.
   * @param data The collected system data.
   * @param noAscii Whether to disable ASCII art.
   * @param widthOverride Column budget to lay out for. Defaults to None, which
   *        detects the terminal size; 0 disables width adaptation entirely.
   * @return A string containing the formatted UI.
   *
   * The layout adapts to the terminal it is printed into. The logo is dropped
   * as soon as it no longer fits beside the box at full width; only below that
   * does the box narrow and wrap its values, and very narrow terminals get a
   * borderless list instead of the box.
   */
  auto CreateUI(
    const config::Config&       config,
    const system::SystemInfo&   data,
    bool                        noAscii,
    types::Option<types::usize> widthOverride = types::None
  ) -> types::String;
} // namespace draconis::ui
