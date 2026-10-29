#include "display_fit.h"

#include <array>
#include <cstdio>

namespace {
bool expect(bool condition, const char* message) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", message);
    return condition;
}
}  // namespace

int main() {
    using cpcexplorer::display::ActiveArea;
    using cpcexplorer::display::make_fit_plan;
    using cpcexplorer::display::source_x_for;
    using cpcexplorer::display::source_y_for;

    const auto manic = make_fit_plan({32, 0, 256, 184, true});
    if (!expect(manic.active_fit, "Manic Miner uses active-display fit") ||
        !expect(manic.output_x == 0 && manic.output_y == 5,
                "Manic Miner is centred at x=0, y=5") ||
        !expect(manic.output_width == 320 && manic.output_height == 230,
                "Manic Miner covers 320x230") ||
        !expect(source_x_for(manic, 0) == 32 && source_x_for(manic, 319) == 287,
                "horizontal mapping preserves both gameplay edges") ||
        !expect(source_y_for(manic, 0) == 0 && source_y_for(manic, 229) == 183,
                "vertical mapping preserves both gameplay edges")) return 1;

    std::array<bool, 256> seen_x{};
    std::array<bool, 184> seen_y{};
    for (int x = 0; x < manic.output_width; ++x)
        seen_x[static_cast<std::size_t>(source_x_for(manic, x) - manic.source_x)] = true;
    for (int y = 0; y < manic.output_height; ++y)
        seen_y[static_cast<std::size_t>(source_y_for(manic, y) - manic.source_y)] = true;
    for (bool seen : seen_x) if (!expect(seen, "every gameplay column is represented")) return 2;
    for (bool seen : seen_y) if (!expect(seen, "every gameplay row is represented")) return 2;

    const auto basic = make_fit_plan({0, 0, 320, 200, true});
    if (!expect(!basic.active_fit, "BASIC retains established viewport") ||
        !expect(basic.output_x == 0 && basic.output_y == 20 &&
                basic.output_width == 320 && basic.output_height == 200,
                "BASIC remains 320x200 centred vertically")) return 3;

    const auto knight = make_fit_plan({32, 0, 256, 200, true});
    if (!expect(knight.active_fit && knight.output_width == 307 &&
                knight.output_height == 240 && knight.output_x == 6 && knight.output_y == 0,
                "Knight Tyme fits proportionally without clipping") ||
        !expect(source_x_for(knight, 306) == 287 && source_y_for(knight, 239) == 199,
                "Knight Tyme preserves its final source row and column")) return 4;

    const ActiveArea unusual[] = {
        {0, 0, 0, 200, true}, {0, 0, 320, 0, true}, {-1, 0, 256, 184, true},
        {32, -1, 256, 184, true}, {0, 0, 321, 200, true},
        {0, 0, 320, 201, true}, {0, 0, 64, 64, true},
        {32, 0, 256, 184, false},
    };
    for (const auto& geometry : unusual) {
        const auto fallback = make_fit_plan(geometry);
        if (!expect(!fallback.active_fit && fallback.source_width == 320 &&
                    fallback.source_height == 200 && fallback.output_y == 20,
                    "unusual CRTC geometry fails closed to established viewport")) return 5;
    }
    std::puts("PASS: active-display fit geometry, mapping, clipping and fallback");
    return 0;
}
