#pragma once

#define ImTextureID ImU64

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <deps/imgui/imgui.h>
#include <frozen/map.h>
#include <include/reshade.hpp>

#include "./bitwise.hpp"
#include "./host_graphics.hpp"
#include "./icons.hpp"
#include "./mutex.hpp"
#include "./settings/segmentedbuttons.hpp"

namespace renodx::utils::settings {

extern "C" __declspec(dllexport) const char* const NAME;

template <typename T = float>
using SettingChangeCallback = std::conditional_t<
    std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, std::string>
        || std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, std::string_view>
        || std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, const char*>,
    std::function<void(std::string_view previous, std::string_view current)>,
    std::function<void(T previous, T current)>>;

enum class SettingStyle : uint8_t {
  DEFAULT = 0,
  SEGMENTED = 1 << 0,
  MULTILINE = 1 << 1,
};

constexpr SettingStyle operator|(SettingStyle lhs, SettingStyle rhs) {
  return static_cast<SettingStyle>(
      static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

constexpr SettingStyle operator&(SettingStyle lhs, SettingStyle rhs) {
  return static_cast<SettingStyle>(
      static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}

static bool use_presets = true;
static SettingStyle preset_style = SettingStyle::DEFAULT;
static bool open_sections_by_default = true;
static std::vector<std::string> default_open_sections;
static std::string overlay_title = NAME;
static std::string global_name = "renodx";
static int preset_index = 1;
static std::vector<std::string> preset_strings = {
    "Off",
    "Preset #1",
    "Preset #2",
    "Preset #3",
};
static const std::vector<std::string> BOOLEAN_STRINGS = {
    "Off",
    "On",
};

static std::vector<std::function<void()>> on_preset_off_callbacks;
static std::vector<std::function<void()>> on_preset_changed_callbacks;

static ImVec4 ImVec4FromHex(uint32_t hex) {
  return {
      static_cast<float>((hex >> (8 * 2)) & 0xFF) / 255.f,
      static_cast<float>((hex >> (8 * 1)) & 0xFF) / 255.f,
      static_cast<float>((hex >> (8 * 0)) & 0xFF) / 255.f,
      1.f,
  };
};

enum class SettingValueType : uint8_t {
  FLOAT = 0,
  INTEGER = 1,
  BOOLEAN = 2,
  BUTTON = 3,
  LABEL = 4,
  BULLET = 5,
  TEXT = 6,
  TEXT_NOWRAP = 7,
  CUSTOM = 8,
  INPUT_TEXT = 9,
};

struct Setting {
  std::string key;
  float* binding = nullptr;
  char* text_binding = nullptr;
  size_t text_binding_size = 0;
  SettingValueType value_type = SettingValueType::FLOAT;
  float default_value = 0.f;
  std::string default_text;
  std::vector<uint32_t> packed_values;
  bool can_reset = true;
  std::string label = key;
  std::string section;
  std::string group;
  std::string tooltip;
  std::string placeholder;
  std::vector<std::string> labels;
  SettingStyle style = SettingStyle::DEFAULT;
  std::optional<uint32_t> tint;  // HEX notation
  float min = 0.f;
  float max = 100.f;
  std::string format = "%.0f";
  ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_None;

  std::function<bool()> is_enabled = [] {
    return true;
  };

  std::function<float(float value)> parse = [](float value) {
    return value;
  };

  std::function<void()> on_change = [] {};

  SettingChangeCallback<> on_change_value = [](float previous, float current) {};
  SettingChangeCallback<std::string_view> on_change_text = [](std::string_view previous, std::string_view current) {};

  // Return true to save settings
  std::function<bool()> on_click = [] { return true; };

  // Return true if value is changed
  std::function<bool()> on_draw = [] { return false; };

  bool is_global = false;

  std::function<bool()> is_visible = [] {
    return true;
  };

  bool is_sticky = false;

  bool is_logarithmic = false;

  float value = default_value;
  int value_as_int = static_cast<int>(default_value);
  std::string value_as_string;

  [[nodiscard]]
  bool HasTextBinding() const {
    return this->text_binding != nullptr && this->text_binding_size > 0;
  }

  [[nodiscard]]
  bool HasNumericValue() const {
    switch (this->value_type) {
      case SettingValueType::FLOAT:
      case SettingValueType::INTEGER:
      case SettingValueType::BOOLEAN:
        return true;
      default:
        return false;
    }
  }

  [[nodiscard]]
  bool SupportsReset() const {
    switch (this->value_type) {
      case SettingValueType::FLOAT:
      case SettingValueType::INTEGER:
      case SettingValueType::BOOLEAN:
      case SettingValueType::INPUT_TEXT:
        return true;
      default:
        return false;
    }
  }

  [[nodiscard]]
  std::string GetTextValue() const {
    if (this->HasTextBinding()) {
      return std::string(this->text_binding);
    }
    return this->value_as_string;
  }

  [[nodiscard]]
  bool IsUsingDefault() const {
    if (this->value_type == SettingValueType::INPUT_TEXT) {
      return this->GetTextValue() == this->default_text;
    }
    return this->GetValue() == this->default_value;
  }

  [[nodiscard]]
  float GetMax() const {
    switch (this->value_type) {
      case SettingValueType::BOOLEAN:
        return 1.f;
      case SettingValueType::INTEGER:
        return this->labels.empty()
                   ? this->max
                   : (this->labels.size() - 1);
      case SettingValueType::FLOAT:
      default:
        return this->max;
    }
  }

  [[nodiscard]]
  float GetValue() const {
    switch (this->value_type) {
      default:
      case SettingValueType::FLOAT:
        return this->value;
        break;
      case SettingValueType::INTEGER:
        return static_cast<float>(this->value_as_int);
        break;
      case SettingValueType::BOOLEAN:
        return ((this->value_as_int == 0) ? 0.f : 1.f);
        break;
      case SettingValueType::INPUT_TEXT:
      case SettingValueType::BUTTON:
      case SettingValueType::LABEL:
      case SettingValueType::BULLET:
      case SettingValueType::TEXT:
      case SettingValueType::TEXT_NOWRAP:
      case SettingValueType::CUSTOM:
        return 0.f;
        break;
    }
  }

  Setting* Set(float value) {
    this->value = value;
    this->value_as_int = static_cast<int>(value);
    return this;
  }

  Setting* Set(const std::string& value) {
    this->value_as_string = value;
    return this;
  }

  Setting* ReadFromConfig(const std::string& config_section) {
    switch (this->value_type) {
      case SettingValueType::FLOAT:
        if (!reshade::get_config_value(nullptr, config_section.c_str(), this->key.c_str(), this->value)) {
          this->value = this->default_value;
        }
        if (this->value > this->GetMax()) {
          this->value = this->GetMax();
        } else if (this->value < this->min) {
          this->value = this->min;
        }
        break;
      case SettingValueType::BOOLEAN:
      case SettingValueType::INTEGER:
        if (!reshade::get_config_value(nullptr, config_section.c_str(), this->key.c_str(), this->value_as_int)) {
          this->value_as_int = static_cast<int>(this->default_value);
        }
        if (this->value_as_int > this->GetMax()) {
          this->value_as_int = this->GetMax();
        } else if (this->value_as_int < static_cast<int>(this->min)) {
          this->value_as_int = static_cast<int>(this->min);
        }
        break;
      case SettingValueType::INPUT_TEXT: {
        size_t size = 0;
        if (!reshade::get_config_value(nullptr, config_section.c_str(), this->key.c_str(), nullptr, &size)
            || size == 0) {
          this->value_as_string = this->default_text;
          break;
        }

        std::vector<char> temp(size + 1, '\0');
        if (!reshade::get_config_value(nullptr, config_section.c_str(), this->key.c_str(), temp.data(), &size)) {
          this->value_as_string = this->default_text;
          break;
        }
        temp.back() = '\0';
        this->value_as_string = temp.data();
        break;
      }
      default:
        break;
    }
    return this;
  }

  Setting* WriteToConfig(const std::string& config_section) {
    switch (this->value_type) {
      case SettingValueType::FLOAT:
        reshade::set_config_value(nullptr, config_section.c_str(), this->key.c_str(), this->value);
        break;
      case SettingValueType::INTEGER:
      case SettingValueType::BOOLEAN:
        reshade::set_config_value(nullptr, config_section.c_str(), this->key.c_str(), this->value_as_int);
        break;
      case SettingValueType::INPUT_TEXT:
        reshade::set_config_value(nullptr, config_section.c_str(), this->key.c_str(), this->GetTextValue().c_str());
        break;
      default:
        break;
    }
    return this;
  }

  Setting* Write() {
    if (this->value_type == SettingValueType::INPUT_TEXT) {
      if (this->HasTextBinding()) {
        size_t copy_size = std::min(this->value_as_string.size(), this->text_binding_size - 1);
        std::memcpy(this->text_binding, this->value_as_string.data(), copy_size);
        this->text_binding[copy_size] = '\0';
      }
      return this;
    }

    if (this->binding != nullptr) {
      if (!this->packed_values.empty()) {
        float packed_binding = *this->binding;
        uint32_t clear_mask = 0u;
        uint32_t selected_value = this->packed_values[this->default_value];
        const int len = static_cast<int>(this->packed_values.size());

        for (int i = 0; i < len; ++i) {
          if (i == this->value_as_int) {
            selected_value = this->packed_values[i];
          } else {
            clear_mask |= this->packed_values[i];
          }
        }

        packed_binding = renodx::utils::bitwise::UnsetFlag(packed_binding, clear_mask);
        packed_binding = renodx::utils::bitwise::SetFlag(packed_binding, selected_value);
        *this->binding = packed_binding;
      } else {
        *this->binding = this->parse(this->GetValue());
      }
    }
    return this;
  }
};

using Settings = std::vector<Setting*>;
static Settings* settings = nullptr;

#define RENODX_JOIN_MACRO(x, y) x##y

#define AddDebugSetting(injection, name)                  \
  new renodx::utils::settings::Setting {                  \
    .key = "debug" #name,                                 \
    .binding = &RENODX_JOIN_MACRO(injection.debug, name), \
    .default_value = 1.f,                                 \
    .label = "Debug" #name,                               \
    .section = "Debug",                                   \
    .max = 2.f,                                           \
    .format = "%.2f",                                     \
  }

static Setting* FindSetting(const std::string& key) {
  for (auto* setting : *settings) {
    if (setting->key == key) {
      return setting;
    }
  }
  return nullptr;
}

static bool UpdateSetting(const std::string& key, float value) {
  auto* setting = FindSetting(key);
  if (setting == nullptr) return false;
  if (!setting->HasNumericValue()) return false;
  const std::unique_lock lock(renodx::utils::mutex::global_mutex);
  setting->Set(value)->Write();
  return true;
}

static bool UpdateSetting(const std::string& key, const std::string& value) {
  auto* setting = FindSetting(key);
  if (setting == nullptr) return false;
  if (setting->value_type != SettingValueType::INPUT_TEXT) return false;
  const std::unique_lock lock(renodx::utils::mutex::global_mutex);
  setting->Set(value)->Write();
  return true;
}

static void ResetSettings(bool reset_global = false) {
  const std::unique_lock lock(renodx::utils::mutex::global_mutex);
  for (auto* setting : *settings) {
    if (setting->key.empty()) continue;
    if (setting->is_global && !reset_global) continue;
    if (!setting->can_reset) continue;
    if (setting->value_type == SettingValueType::INPUT_TEXT) {
      setting->Set(setting->default_text)->Write();
    } else {
      setting->Set(setting->default_value)->Write();
    }
  }
}

static bool UpdateSettings(const std::vector<std::pair<std::string, float>>& pairs) {
  bool missing_key = false;
  const std::unique_lock lock(renodx::utils::mutex::global_mutex);
  for (const auto& [key, value] : pairs) {
    auto* setting = FindSetting(key);
    if (setting == nullptr) {
      missing_key = true;
    } else {
      setting->Set(value)->Write();
    }
  }
  return !missing_key;
}

[[deprecated("Use Setting::ReadFromConfig directly")]]
static void LoadSetting(const std::string& config_section, Setting* setting) {
  setting->ReadFromConfig(config_section);
}

static void LoadSettings(const std::string& config_section) {
  for (auto* setting : *settings) {
    if (setting->is_global) continue;
    const std::unique_lock lock(renodx::utils::mutex::global_mutex);
    setting->ReadFromConfig(config_section)->Write();
  }
}

static void LoadGlobalSettings() {
  for (auto* setting : *settings) {
    if (!setting->is_global) continue;
    const std::unique_lock lock(renodx::utils::mutex::global_mutex);
    setting->ReadFromConfig(global_name)->Write();
  }
}

static std::string GetCurrentPresetName() {
  switch (preset_index) {
    case 1:
      return global_name + "-preset1";
      break;
    case 2:
      return global_name + "-preset2";
      break;
    case 3:
      return global_name + "-preset3";
      break;
  }
  return "";
}

static void SaveSettings(const std::string& config_section = GetCurrentPresetName()) {
  for (auto* setting : *settings) {
    if (setting->key.empty()) continue;
    if (setting->is_global) continue;
    setting->WriteToConfig(config_section);
  }
}

static void SaveGlobalSettings() {
  for (auto* setting : *settings) {
    if (setting->key.empty()) continue;
    if (!setting->is_global) continue;
    setting->WriteToConfig(global_name);
  }
}

static std::string ReadGlobalString(const std::string& key) {
  char temp[256] = "";
  size_t size = 256;
  if (reshade::get_config_value(nullptr, global_name.c_str(), key.c_str(), temp, &size)) {
    std::string temp_string = std::string(temp);
    auto pos = temp_string.find_last_not_of("\t\n\v\f\r ");
    if (pos != std::string_view::npos) {
      temp_string = {temp_string.data(), temp_string.data() + pos + 1};
    }
    return temp_string;
  }
  return "";
}

static void WriteGlobalString(const std::string& key, const std::string& value) {
  reshade::set_config_value(nullptr, global_name.c_str(), key.c_str(), value.c_str());
}

static bool IsPresetOff() {
  return use_presets && preset_index == 0;
}

// Runs first
// https://pthom.github.io/imgui_manual_online/manual/imgui_manual.html
static void OnRegisterOverlay(reshade::api::effect_runtime* runtime) {
  bool changed_preset = false;
  bool has_drawn_presets = !use_presets;

  auto draw_presets = [&]() {
    if (use_presets) {
      if ((preset_style & SettingStyle::SEGMENTED)
          == SettingStyle::SEGMENTED) {
        changed_preset = SegmentedButtons(
            "Preset",
            &preset_index,
            preset_strings,
            (preset_style & SettingStyle::MULTILINE)
                == SettingStyle::MULTILINE);
      } else {
        changed_preset = ImGui::SliderInt(
            "Preset",
            &preset_index,
            0,
            preset_strings.size() - 1,
            preset_strings[preset_index].c_str(),
            ImGuiSliderFlags_NoInput);
      }
    }

    if (changed_preset) {
      switch (preset_index) {
        case 0:
          for (auto& callback : on_preset_off_callbacks) {
            callback();
          }
          break;
        case 1:
          LoadSettings(global_name + "-preset1");
          break;
        case 2:
          LoadSettings(global_name + "-preset2");
          break;
        case 3:
          LoadSettings(global_name + "-preset3");
          break;
      }
      for (auto& callback : on_preset_changed_callbacks) {
        callback();
      }
    }
    has_drawn_presets = true;
  };

  bool any_change = false;
  std::string last_section;
  std::string last_group;
  bool open_section = true;
  bool open_node = false;
  bool has_indent = false;
  for (auto* setting : *settings) {
    if (setting->is_visible != nullptr && !setting->is_visible()) continue;

    if (!setting->is_sticky) {
      if (!has_drawn_presets) {
        draw_presets();
      }
    }

    int styles_pushed = 0;
    if (setting->tint.has_value()) {
      auto target_rgb = ImVec4FromHex(setting->tint.value());
      float target_hsv[3] = {};
      ImGui::ColorConvertRGBtoHSV(target_rgb.x, target_rgb.y, target_rgb.z, target_hsv[0], target_hsv[1], target_hsv[2]);

      static const auto COMMON_STYLES =
          {
              ImGuiCol_FrameBg,
              ImGuiCol_FrameBgHovered,
              ImGuiCol_FrameBgActive,
              ImGuiCol_SliderGrab,
              ImGuiCol_SliderGrabActive,
              ImGuiCol_Button,
              ImGuiCol_ButtonActive,
              ImGuiCol_ButtonHovered,
              ImGuiCol_TextSelectedBg,
              ImGuiCol_Header,
              ImGuiCol_HeaderHovered,
              ImGuiCol_HeaderActive,
          };

      static const auto TEXT_STYLES = {
          ImGuiCol_Text,
          ImGuiCol_TextDisabled,
      };
      if (setting->value_type == SettingValueType::TEXT
          || setting->value_type == SettingValueType::TEXT_NOWRAP) {
        for (const auto style : TEXT_STYLES) {
          auto style_rgb = ImGui::GetStyleColorVec4(style);
          style_rgb.x = target_rgb.x;
          style_rgb.y = target_rgb.y;
          style_rgb.z = target_rgb.z;
          ImGui::PushStyleColor(style, style_rgb);
        }
        styles_pushed = TEXT_STYLES.size();
      } else {
        for (const auto style : COMMON_STYLES) {
          auto style_rgb = ImGui::GetStyleColorVec4(style);
          float style_hsv[3] = {};
          ImGui::ColorConvertRGBtoHSV(style_rgb.x, style_rgb.y, style_rgb.z, style_hsv[0], style_hsv[1], style_hsv[2]);

          ImGui::ColorConvertHSVtoRGB(target_hsv[0], style_hsv[1], style_hsv[2], style_rgb.x, style_rgb.y, style_rgb.z);
          ImGui::PushStyleColor(style, style_rgb);
        }
        styles_pushed = COMMON_STYLES.size();
      }
    }

    if (last_section != setting->section) {
      last_section.assign(setting->section);

      if (open_node) {
        // TreePop will call unindent
        ImGui::Indent();
        ImGui::TreePop();
      }
      open_node = ImGui::TreeNodeEx(
          setting->section.c_str(),
          ImGuiTreeNodeFlags_SpanFullWidth
              | (open_sections_by_default
                         || std::ranges::find(
                                default_open_sections,
                                setting->section)
                                != default_open_sections.end()
                     ? ImGuiTreeNodeFlags_DefaultOpen
                     : ImGuiTreeNodeFlags_None));
      if (open_node) {
        ImGui::Unindent();
      }

      open_section = open_node;
    }

    if (open_section) {
      if (!last_group.empty() && !setting->group.empty() && last_group == setting->group) {
        ImGui::SameLine();
      }

      last_group = setting->group;

      const bool is_disabled = preset_index == 0
                               || (setting->is_enabled != nullptr
                                   && !setting->is_enabled());
      if (is_disabled) {
        ImGui::BeginDisabled();
      }
      bool changed = false;
      float previous_value = setting->HasNumericValue()
                                 ? setting->GetValue()
                                 : 0.f;
      std::string previous_text = setting->value_type == SettingValueType::INPUT_TEXT
                                      ? setting->GetTextValue()
                                      : std::string();
      const auto identifier = (setting->key.empty() ? setting->label : setting->key);
      ImGui::PushID(("##Key" + identifier).c_str());
      ImGuiSliderFlags slider_flags = ImGuiSliderFlags_None;
      if (setting->is_logarithmic) {
        slider_flags |= ImGuiSliderFlags_Logarithmic;
      }
      const bool is_multiline = renodx::utils::bitwise::HasFlag(
          setting->style,
          SettingStyle::MULTILINE);
      switch (setting->value_type) {
        case SettingValueType::FLOAT:
          changed |= ImGui::SliderFloat(
              setting->label.c_str(),
              &setting->value,
              setting->min,
              setting->max,
              setting->format.c_str(),
              slider_flags);
          break;
        case SettingValueType::INTEGER:
          if (renodx::utils::bitwise::HasFlag(
                  setting->style,
                  SettingStyle::SEGMENTED)
              && !setting->labels.empty()) {
            changed |= SegmentedButtons(
                setting->label.c_str(),
                &setting->value_as_int,
                setting->labels,
                is_multiline);
          } else {
            changed |= ImGui::SliderInt(
                setting->label.c_str(),
                &setting->value_as_int,
                setting->min,
                setting->GetMax(),
                setting->labels.empty()
                    ? setting->format.c_str()
                    : setting->labels.at(setting->value_as_int).c_str(),
                slider_flags | ImGuiSliderFlags_NoInput);
          }
          break;
        case SettingValueType::BOOLEAN:
          if (renodx::utils::bitwise::HasFlag(
                  setting->style,
                  SettingStyle::SEGMENTED)) {
            IM_ASSERT(setting->labels.empty() || setting->labels.size() >= 2);
            if (setting->labels.size() < 2) {
              changed |= SegmentedButtons(
                  setting->label.c_str(),
                  &setting->value_as_int,
                  BOOLEAN_STRINGS,
                  is_multiline);
            } else {
              changed |= SegmentedButtons(
                  setting->label.c_str(),
                  &setting->value_as_int,
                  std::span<const std::string>(setting->labels).first(2),
                  is_multiline);
            }
          } else {
            changed |= ImGui::SliderInt(
                setting->label.c_str(),
                &setting->value_as_int,
                0,
                1,
                setting->labels.empty()
                    ? ((setting->value_as_int == 0) ? "Off" : "On")  // NOLINT(readability-avoid-nested-conditional-operator)
                    : setting->labels.at(setting->value_as_int).c_str(),
                slider_flags | ImGuiSliderFlags_NoInput);
          }
          break;
        case SettingValueType::BUTTON:
          if (ImGui::Button(setting->label.c_str())) {
            changed = setting->on_click();
          }
          break;
        case SettingValueType::LABEL:
          ImGui::LabelText(setting->label.c_str(), "%s", setting->labels[0].c_str());
          break;
        case SettingValueType::BULLET:
          ImGui::BulletText(setting->label.c_str(), "");
          break;
        case SettingValueType::TEXT:
          ImGui::TextWrapped("%s", setting->label.c_str());
          break;
        case SettingValueType::TEXT_NOWRAP:
          ImGui::Text(setting->label.c_str(), "");
          break;
        case SettingValueType::CUSTOM:
          changed |= setting->on_draw();
          break;
        case SettingValueType::INPUT_TEXT:
          if (setting->HasTextBinding()) {
            changed |= ImGui::InputTextWithHint(
                setting->label.c_str(),
                setting->placeholder.c_str(),
                setting->text_binding,
                setting->text_binding_size,
                setting->input_text_flags);
            if (changed) {
              setting->value_as_string = setting->GetTextValue();
            }
          } else {
            changed |= ImGui::InputTextWithHint(
                setting->label.c_str(),
                setting->placeholder.c_str(),
                setting->value_as_string.data(),
                setting->value_as_string.capacity() + 1,
                setting->input_text_flags | ImGuiInputTextFlags_CallbackResize,
                [](ImGuiInputTextCallbackData* data) {
                  if (data->EventFlag != ImGuiInputTextFlags_CallbackResize) return 0;
                  auto* value = static_cast<std::string*>(data->UserData);
                  value->resize(static_cast<size_t>(data->BufTextLen));
                  data->Buf = value->data();
                  return 0;
                },
                &setting->value_as_string);
          }
          break;
      }
      ImGui::PopID();
      if (changed) {
        setting->on_change();
      }
      if (!setting->tooltip.empty()) {
        ImGui::SetItemTooltip("%s", setting->tooltip.c_str());
      }

      if (preset_index != 0
          && setting->can_reset
          && setting->SupportsReset()) {
        ImGui::SameLine();
        const bool is_using_default = setting->IsUsingDefault();
        ImGui::BeginDisabled(is_using_default);
        if (is_using_default) {
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor::HSV(0, 0, 0.6f)));
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor::HSV(0, 0, 0.7f)));
          ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor::HSV(0, 0, 0.8f)));
        }
        auto* font = ImGui::GetFont();
        auto old_scale = font->Scale;
        auto previous_font_size = ImGui::GetFontSize();
        font->Scale *= 0.75f;
        ImGui::PushFont(font);
        auto current_font_size = ImGui::GetFontSize();

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, current_font_size * 2);

        ImVec2 cursor_pos = ImGui::GetCursorPos();
        cursor_pos.y += (previous_font_size / 2.f) - (current_font_size / 2.f);
        ImGui::SetCursorPos(cursor_pos);

        ImGui::PushID(("##Reset" + identifier).c_str());
        if (ImGui::Button(renodx::utils::icons::View(renodx::utils::icons::UNDO))) {
          if (setting->value_type == SettingValueType::INPUT_TEXT) {
            setting->Set(setting->default_text);
          } else {
            setting->Set(setting->default_value);
          }
          changed = true;
        }
        ImGui::PopID();

        if (is_using_default) {
          ImGui::PopStyleColor(3);
        }
        font->Scale = old_scale;
        ImGui::PopFont();
        ImGui::PopStyleVar();
        ImGui::EndDisabled();
      }

      if (changed) {
        const std::unique_lock lock(renodx::utils::mutex::global_mutex);
        setting->Write();
        any_change = true;
        if (setting->HasNumericValue()) {
          setting->on_change_value(previous_value, setting->GetValue());
        } else if (setting->value_type == SettingValueType::INPUT_TEXT) {
          setting->on_change_text(previous_text, setting->GetTextValue());
        }
      }
      if (is_disabled) {
        ImGui::EndDisabled();
      }
    }
    ImGui::PopStyleColor(styles_pushed);
  }
  if (open_node) {
    ImGui::Indent();
    ImGui::TreePop();
  }

  if (!has_drawn_presets) {
    draw_presets();
  }
  if (!changed_preset && any_change) {
    switch (preset_index) {
      case 1:
        SaveSettings(global_name + "-preset1");
        break;
      case 2:
        SaveSettings(global_name + "-preset2");
        break;
      case 3:
        SaveSettings(global_name + "-preset3");
        break;
    }
    SaveGlobalSettings();
  }
}

static bool attached = false;

static void Use(
    DWORD fdw_reason,
    Settings* new_settings,
    void (*new_on_preset_off)() = nullptr,
    void (*new_on_preset_changed)() = nullptr) {
  switch (fdw_reason) {
    case DLL_PROCESS_ATTACH:
      if (attached) return;
      attached = true;

      settings = new_settings;
      if (new_on_preset_off != nullptr) {
        on_preset_off_callbacks.emplace_back(new_on_preset_off);
      }
      if (new_on_preset_changed != nullptr) {
        on_preset_changed_callbacks.emplace_back(new_on_preset_changed);
      }
      LoadGlobalSettings();
      LoadSettings(global_name + "-preset1");
      reshade::register_overlay(overlay_title.c_str(), OnRegisterOverlay);

      break;
    case DLL_PROCESS_DETACH:
      if (!attached) return;
      attached = false;
      reshade::unregister_overlay(overlay_title.c_str(), OnRegisterOverlay);
      on_preset_off_callbacks.clear();
      on_preset_changed_callbacks.clear();
      settings = nullptr;
      break;
  }
}

// ---- host API -------------------------------------------------------------------------------
//
// A C ABI another module in the same process can use to read and drive this addon's settings.
// ReShade's overlay is the only way in today, which is right for a person at a keyboard and no use
// to a tool: an external panel, a launcher, or a second addon that wants to present these controls
// in its own UI cannot reach them at all.
//
// It is a facade, not a layer. Every entry point below is FindSetting, UpdateSetting or
// SaveSettings -- the functions the overlay itself calls -- so there is no second copy of the
// locking, the write path or the preset rules to keep in step.
//
// Exported from settings.hpp on purpose: every addon compiles this header into its single
// translation unit, so one definition here gives every addon the export without touching 271
// addon.cpp files or any build file. It sits beside the NAME export above, which already
// establishes that this header owns an exported symbol.
//
// Compatibility: `struct_size` on both structs and `api_version` are the whole contract. A caller
// passes the version it was built against and gets nullptr if this build cannot serve it; it then
// checks `struct_size` before reading a field, so adding members later does not break an older
// caller. Do not reorder or remove members -- append only.

// 2: resolve_clone and encode_for_swapchain, for a host that hands this addon's images to NVIDIA
//    Streamline DLSS Frame Generation.
inline constexpr uint32_t HOST_API_VERSION = 2u;

enum RenoDxHostValueType : uint32_t {
  RENODX_HOST_VALUE_FLOAT = 0,
  RENODX_HOST_VALUE_INTEGER = 1,
  RENODX_HOST_VALUE_BOOLEAN = 2,
  // An integer whose values are named: `label_count` is non-zero and RenoDxHostLabelAt names each.
  RENODX_HOST_VALUE_COMBO = 3,
  RENODX_HOST_VALUE_TEXT = 4,
};

struct RenoDxHostSetting {
  uint32_t struct_size;
  // Borrowed from the Setting itself and valid only until the next call into this API on this
  // thread. A caller that keeps a description copies the strings.
  const char* key;
  const char* label;
  const char* section;
  const char* group;
  const char* tooltip;
  const char* format;
  uint32_t value_type;
  float min_value;
  float max_value;
  uint32_t label_count;
  // The addon's own answer to "should this be shown / is it live right now", so a host UI can grey
  // a control the overlay would also grey rather than offering a value the mod will ignore.
  int32_t is_visible;
  int32_t is_enabled;
  int32_t is_global;
};

struct RenoDxHostApi {
  uint32_t struct_size;
  uint32_t api_version;
  // The ReShade config section this addon reads and writes -- `global_name`. A host that wants to
  // find the same values on disk needs this; guessing "renodx" is wrong for any mod that sets it.
  const char* (*addon_name)();
  uint32_t (*setting_count)();
  // False when `index` is out of range or `out->struct_size` is smaller than this build's struct.
  bool (*describe_setting)(uint32_t index, RenoDxHostSetting* out);
  // One label of a COMBO setting. Separate from describe_setting because the labels are
  // std::strings owned by the Setting: handing out an array of pointers would mean caching one,
  // and a cache is a lifetime bug waiting to happen for a caller that holds it too long.
  const char* (*label_at)(uint32_t index, uint32_t label_index);
  bool (*get_number)(const char* key, float* out);
  bool (*set_number)(const char* key, float value);
  // Writes at most `buf_size` bytes including the terminator; false if the value does not fit, and
  // the buffer is then left empty rather than truncated into something that reads as a real value.
  bool (*get_text)(const char* key, char* buf, uint32_t buf_size);
  bool (*set_text)(const char* key, const char* value);
  // Persist to the current preset's config section, as the overlay does after a change.
  void (*save)();

  // Version 2. For a host that tags this addon's resources for NVIDIA Streamline (DLSS Frame
  // Generation reads them when the frame is presented). Both take and return native D3D12 resources
  // (ID3D12Resource*) and D3D12_RESOURCE_STATES, and return false, leaving the outputs untouched,
  // when the addon has nothing to substitute -- the host then tags the original.
  //
  // The resource the game's writes actually land in when this addon redirects them to a clone; the
  // same lookup RenoDX's dlssfix does in its slSetTag hook.
  bool (*resolve_clone)(void* native_resource, void** out_native_resource);
  // For a colour image Streamline compares with the presented frame (HUD-less colour, back buffer):
  // a texture of the swap chain's format that receives, at each present and before the frame leaves
  // ReShade, this addon's swap chain proxy pass run over the image (over its clone when it has one),
  // so it is encoded exactly like the frame it is compared with. `d3d12_state` is the state the host
  // tagged the image in; `out_d3d12_state` is the state the returned texture is left in.
  bool (*encode_for_swapchain)(void* native_resource, uint32_t d3d12_state, void** out_native_resource,
                               uint32_t* out_d3d12_state);
};

// The C ABI is only an ABI if the layout is one C understands, and that is a property a later
// edit can quietly remove -- a std::string field, a virtual, a base class. Asserted here rather
// than in test/api_compatibility because every addon compiles this header, so every addon build
// is the check.
static_assert(std::is_standard_layout_v<RenoDxHostSetting>);
static_assert(std::is_standard_layout_v<RenoDxHostApi>);

namespace host_api_detail {

inline const char* AddonName() {
  return global_name.c_str();
}

inline uint32_t SettingCount() {
  if (settings == nullptr) return 0u;
  const std::shared_lock lock(renodx::utils::mutex::global_mutex);
  return static_cast<uint32_t>(settings->size());
}

inline Setting* At(uint32_t index) {
  if (settings == nullptr || index >= settings->size()) return nullptr;
  return settings->at(index);
}

inline uint32_t ValueTypeOf(const Setting* setting) {
  switch (setting->value_type) {
    case SettingValueType::INTEGER:
      return setting->labels.empty() ? RENODX_HOST_VALUE_INTEGER : RENODX_HOST_VALUE_COMBO;
    case SettingValueType::BOOLEAN:
      return RENODX_HOST_VALUE_BOOLEAN;
    case SettingValueType::INPUT_TEXT:
      return RENODX_HOST_VALUE_TEXT;
    default:
      return RENODX_HOST_VALUE_FLOAT;
  }
}

inline bool DescribeSetting(uint32_t index, RenoDxHostSetting* out) {
  if (out == nullptr || out->struct_size < sizeof(RenoDxHostSetting)) return false;
  Setting* setting = nullptr;
  {
    const std::shared_lock lock(renodx::utils::mutex::global_mutex);
    setting = At(index);
    if (setting == nullptr) return false;
    out->key = setting->key.c_str();
    out->label = setting->label.c_str();
    out->section = setting->section.c_str();
    out->group = setting->group.c_str();
    out->tooltip = setting->tooltip.c_str();
    out->format = setting->format.c_str();
    out->value_type = ValueTypeOf(setting);
    out->min_value = setting->min;
    out->max_value = setting->GetMax();
    out->label_count = static_cast<uint32_t>(setting->labels.size());
    out->is_global = setting->is_global ? 1 : 0;
  }
  // Outside the lock, because these two are the mod author's own lambdas and one of them calling
  // UpdateSetting would deadlock on a shared_mutex we already hold. The overlay's own draw path
  // calls them unlocked for the same reason, so this matches it rather than inventing a stricter
  // rule that only this entry point obeys.
  out->is_visible = setting->is_visible == nullptr || setting->is_visible() ? 1 : 0;
  out->is_enabled = setting->is_enabled == nullptr || setting->is_enabled() ? 1 : 0;
  return true;
}

inline const char* LabelAt(uint32_t index, uint32_t label_index) {
  const std::shared_lock lock(renodx::utils::mutex::global_mutex);
  auto* setting = At(index);
  if (setting == nullptr || label_index >= setting->labels.size()) return nullptr;
  return setting->labels.at(label_index).c_str();
}

inline bool GetNumber(const char* key, float* out) {
  if (key == nullptr || out == nullptr) return false;
  const std::shared_lock lock(renodx::utils::mutex::global_mutex);
  auto* setting = FindSetting(key);
  if (setting == nullptr || !setting->HasNumericValue()) return false;
  *out = setting->GetValue();
  return true;
}

inline bool GetText(const char* key, char* buf, uint32_t buf_size) {
  if (key == nullptr || buf == nullptr || buf_size == 0u) return false;
  buf[0] = '\0';
  const std::shared_lock lock(renodx::utils::mutex::global_mutex);
  auto* setting = FindSetting(key);
  if (setting == nullptr) return false;
  const auto text = setting->GetTextValue();
  if (text.size() + 1u > buf_size) return false;
  std::memcpy(buf, text.c_str(), text.size() + 1u);
  return true;
}

inline bool SetNumber(const char* key, float value) {
  // UpdateSetting takes the write lock and calls Set()->Write(), which is what runs the setting's
  // on_change callbacks. Reaching past it into Setting::Set would skip them.
  return key != nullptr && UpdateSetting(std::string(key), value);
}

inline bool SetText(const char* key, const char* value) {
  return key != nullptr && value != nullptr && UpdateSetting(std::string(key), std::string(value));
}

inline void Save() {
  SaveSettings();
  SaveGlobalSettings();
}

inline bool ResolveClone(void* native_resource, void** out_native_resource) {
  return host_graphics::resolve_clone != nullptr
         && host_graphics::resolve_clone(native_resource, out_native_resource);
}

inline bool EncodeForSwapchain(void* native_resource, uint32_t d3d12_state, void** out_native_resource,
                               uint32_t* out_d3d12_state) {
  return host_graphics::encode_for_swapchain != nullptr
         && host_graphics::encode_for_swapchain(native_resource, d3d12_state, out_native_resource, out_d3d12_state);
}

inline const RenoDxHostApi API = {
    .struct_size = sizeof(RenoDxHostApi),
    .api_version = HOST_API_VERSION,
    .addon_name = AddonName,
    .setting_count = SettingCount,
    .describe_setting = DescribeSetting,
    .label_at = LabelAt,
    .get_number = GetNumber,
    .set_number = SetNumber,
    .get_text = GetText,
    .set_text = SetText,
    .save = Save,
    .resolve_clone = ResolveClone,
    .encode_for_swapchain = EncodeForSwapchain,
};

}  // namespace host_api_detail

}  // namespace renodx::utils::settings

// Outside the namespace so the exported name is exactly `RenoDxGetHostApi`, which is what a caller
// resolves with GetProcAddress. Returns nullptr when the caller wants a version this build predates.
extern "C" __declspec(dllexport) inline const renodx::utils::settings::RenoDxHostApi* RenoDxGetHostApi(
    uint32_t requested_version) {
  if (requested_version > renodx::utils::settings::HOST_API_VERSION) return nullptr;
  return &renodx::utils::settings::host_api_detail::API;
}
