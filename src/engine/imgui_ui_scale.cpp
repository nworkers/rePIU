#include "repiu/engine/imgui_ui_scale.h"

#include "imgui.h"

#include <algorithm>

namespace repiu::engine
{

float UiScaleForHeight(float height, const UiScaleRule& rule)
{
    if (!(height > 0.0F) || !(rule.reference_height > 0.0F))
    {
        return rule.minimum;
    }
    return std::max(rule.minimum,
                    height / rule.reference_height * rule.factor);
}

void ApplyImGuiUiScale(const ImGuiStyle& base, float scale)
{
    ImGuiStyle& style = ImGui::GetStyle();
    style = base;
    style.ScaleAllSizes(scale);
    style.FontScaleMain = scale;
}

void AddImGuiUiFont()
{
    ImGui::GetIO().Fonts->AddFontDefaultVector();
}

}  // namespace repiu::engine
