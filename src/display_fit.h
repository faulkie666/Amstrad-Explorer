#pragma once

namespace cpcexplorer::display {

struct ActiveArea {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool valid = false;
};

struct FitPlan {
    int source_x = 0;
    int source_y = 0;
    int source_width = 320;
    int source_height = 200;
    int output_x = 0;
    int output_y = 20;
    int output_width = 320;
    int output_height = 200;
    bool active_fit = false;

    bool operator==(const FitPlan&) const = default;
};

FitPlan make_fit_plan(const ActiveArea& active);
int source_x_for(const FitPlan& plan, int output_x);
int source_y_for(const FitPlan& plan, int output_y);

}  // namespace cpcexplorer::display
