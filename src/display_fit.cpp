#include "display_fit.h"

#include <algorithm>
#include <cstdint>

namespace cpcexplorer::display {
namespace {

constexpr int kViewportWidth = 320;
constexpr int kViewportHeight = 200;
constexpr int kPanelWidth = 320;
constexpr int kPanelHeight = 240;

bool safe_active_area(const ActiveArea& area) {
    if (!area.valid || area.width < 128 || area.height < 100 ||
        area.x < 0 || area.y < 0) return false;
    const std::int64_t right = static_cast<std::int64_t>(area.x) + area.width;
    const std::int64_t bottom = static_cast<std::int64_t>(area.y) + area.height;
    return right <= kViewportWidth && bottom <= kViewportHeight;
}

}  // namespace

FitPlan make_fit_plan(const ActiveArea& active) {
    FitPlan plan{};
    if (!safe_active_area(active)) return plan;

    plan.source_x = active.x;
    plan.source_y = active.y;
    plan.source_width = active.width;
    plan.source_height = active.height;
    if (active.width == kViewportWidth && active.height == kViewportHeight) return plan;

    if (static_cast<std::int64_t>(kPanelWidth) * active.height <=
        static_cast<std::int64_t>(kPanelHeight) * active.width) {
        plan.output_width = kPanelWidth;
        plan.output_height = static_cast<int>(
            (static_cast<std::int64_t>(active.height) * kPanelWidth + active.width / 2) /
            active.width);
    } else {
        plan.output_height = kPanelHeight;
        plan.output_width = static_cast<int>(
            (static_cast<std::int64_t>(active.width) * kPanelHeight + active.height / 2) /
            active.height);
    }
    plan.output_width = std::clamp(plan.output_width, 1, kPanelWidth);
    plan.output_height = std::clamp(plan.output_height, 1, kPanelHeight);
    plan.output_x = (kPanelWidth - plan.output_width) / 2;
    plan.output_y = (kPanelHeight - plan.output_height) / 2;
    plan.active_fit = true;
    return plan;
}

int source_x_for(const FitPlan& plan, int output_x) {
    const int bounded = std::clamp(output_x, 0, plan.output_width - 1);
    return plan.source_x + static_cast<int>(
        static_cast<std::int64_t>(bounded) * plan.source_width / plan.output_width);
}

int source_y_for(const FitPlan& plan, int output_y) {
    const int bounded = std::clamp(output_y, 0, plan.output_height - 1);
    return plan.source_y + static_cast<int>(
        static_cast<std::int64_t>(bounded) * plan.source_height / plan.output_height);
}

}  // namespace cpcexplorer::display
