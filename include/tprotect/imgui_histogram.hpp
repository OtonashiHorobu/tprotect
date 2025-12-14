// imgui_histogram.hpp: ImGui Histogram Drawing Utilities

#pragma once

#include <algorithm>
#include <format>
#include <vector>

#include <imgui.h>

namespace ImGui
{
/**
 * @brief Draw a horizontal bar histogram
 *
 * @param label The label for the histogram
 * @param values Vector of values to display
 * @param labels Vector of labels for each bar
 * @param max_value Maximum value for scaling (0 = auto)
 * @param size Size of the histogram area
 */
inline void HistogramBars(const char *label, const std::vector<float> &values, const std::vector<const char *> &labels,
                          float max_value = 0.0f, const ImVec2 &size = ImVec2{-1, 200})
{
    if (values.empty())
        return;

    if (max_value <= 0.0f)
    {
        max_value = *std::max_element(values.begin(), values.end());
        if (max_value <= 0.0f)
            max_value = 1.0f;
    }

    if (ImGui::BeginChild(label, size, true))
    {
        const float bar_height{ImGui::GetContentRegionAvail().y / static_cast<float>(values.size())};
        const float bar_width{ImGui::GetContentRegionAvail().x * 0.7f};
        const float label_width{ImGui::GetContentRegionAvail().x * 0.25f};

        ImDrawList *draw_list{ImGui::GetWindowDrawList()};
        ImVec2 canvas_pos{ImGui::GetCursorScreenPos()};

        for (size_t i{}; i < values.size(); ++i)
        {
            const float bar_fraction{values[i] / max_value};
            const float bar_pixel_width{bar_width * bar_fraction};

            // Draw label
            ImGui::SetCursorScreenPos(ImVec2{canvas_pos.x, canvas_pos.y + i * bar_height + bar_height * 0.25f});
            if (i < labels.size())
            {
                ImGui::Text("%s", labels[i]);
            }

            // Draw bar background
            ImVec2 bar_min{canvas_pos.x + label_width, canvas_pos.y + i * bar_height + 2};
            ImVec2 bar_max{canvas_pos.x + label_width + bar_width, canvas_pos.y + (i + 1) * bar_height - 2};
            draw_list->AddRectFilled(bar_min, ImVec2{bar_min.x + bar_pixel_width, bar_max.y},
                                     ImGui::GetColorU32(ImGuiCol_PlotHistogram));

            // Draw bar border
            draw_list->AddRect(bar_min, bar_max, ImGui::GetColorU32(ImGuiCol_Border));

            // Draw value text
            ImGui::SetCursorScreenPos(ImVec2{canvas_pos.x + label_width + bar_width + 10,
                                             canvas_pos.y + i * bar_height + bar_height * 0.25f});
            ImGui::Text("%.1f%%", values[i]);
        }

        ImGui::EndChild();
    }
}

/**
 * @brief Draw a vertical bar histogram
 *
 * @param label The label for the histogram
 * @param values Vector of values to display
 * @param labels Vector of labels for each bar
 * @param max_value Maximum value for scaling (0 = auto)
 * @param size Size of the histogram area
 */
inline void HistogramBarsVertical(const char *label, const std::vector<float> &values,
                                  const std::vector<const char *> &labels, float max_value = 0.0f,
                                  const ImVec2 &size = ImVec2{-1, 250})
{
    if (values.empty())
        return;

    if (max_value <= 0.0f)
    {
        max_value = *std::max_element(values.begin(), values.end());
        if (max_value <= 0.0f)
            max_value = 1.0f;
    }

    if (ImGui::BeginChild(label, size, true))
    {
        ImDrawList *draw_list{ImGui::GetWindowDrawList()};
        ImVec2 canvas_pos{ImGui::GetCursorScreenPos()};
        ImVec2 canvas_size{ImGui::GetContentRegionAvail()};

        const float bar_width{canvas_size.x / static_cast<float>(values.size())};
        const float chart_height{canvas_size.y * 0.85f};
        const float chart_bottom{canvas_pos.y + chart_height};

        for (size_t i{}; i < values.size(); ++i)
        {
            const float bar_fraction{values[i] / max_value};
            const float bar_pixel_height{chart_height * bar_fraction};

            // Draw bar
            ImVec2 bar_min{canvas_pos.x + i * bar_width + 2, chart_bottom - bar_pixel_height};
            ImVec2 bar_max{canvas_pos.x + (i + 1) * bar_width - 2, chart_bottom};
            draw_list->AddRectFilled(bar_min, bar_max, ImGui::GetColorU32(ImGuiCol_PlotHistogram));
            draw_list->AddRect(bar_min, bar_max, ImGui::GetColorU32(ImGuiCol_Border));

            // Draw label
            ImVec2 label_pos{canvas_pos.x + i * bar_width + bar_width * 0.5f - 5, chart_bottom + 5};
            if (i < labels.size())
            {
                draw_list->AddText(label_pos, ImGui::GetColorU32(ImGuiCol_Text), labels[i]);
            }

            // Draw value on top of bar
            ImVec2 value_pos{canvas_pos.x + i * bar_width + bar_width * 0.5f - 10, bar_min.y - 15};
            draw_list->AddText(value_pos, ImGui::GetColorU32(ImGuiCol_Text), std::format("{:.1f}%", values[i]).c_str());
        }

        // Draw axis line
        draw_list->AddLine(ImVec2{canvas_pos.x, chart_bottom}, ImVec2{canvas_pos.x + canvas_size.x, chart_bottom},
                           ImGui::GetColorU32(ImGuiCol_Border));

        ImGui::EndChild();
    }
}
} // namespace ImGui
