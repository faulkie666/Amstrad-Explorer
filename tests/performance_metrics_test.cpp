#include "performance_metrics.h"

#include <cstdio>

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", message);
    return condition;
}

}  // namespace

int main() {
    using namespace cpcexplorer::performance;
    Aggregator stats;
    stats.reset(100);
    for (int i = 0; i < 40; ++i) {
        stats.add({
            .emulated_us = 20'000,
            .execution_us = static_cast<std::uint32_t>(14'000 + i),
            .cpu_us = 10'000,
            .event_us = 1'000,
            .reconstruction_us = 3'000,
            .conversion_us = 2'000,
            .lcd_wait_us = 6'000,
            .render_us = 8'100,
            .work_us = 22'000,
            .iteration_us = 25'000,
            .presented_frame = true,
            .missed_deadline = (i & 1) != 0,
        });
    }
    if (!expect(!stats.report_due(1'000'099), "report stays below one-second interval") ||
        !expect(stats.report_due(1'000'100), "report becomes due at one second")) return 1;
    const auto report = stats.take_report(1'000'100);
    if (!expect(report.interval_us == 1'000'000, "interval is preserved") ||
        !expect(report.slices == 40, "slice count is preserved") ||
        !expect(report.presented_frames == 40, "presented frame count is preserved") ||
        !expect(report.missed_deadlines == 20, "missed deadlines are counted") ||
        !expect(report.execution_average_us == 14'019, "execution average is integer bounded") ||
        !expect(report.execution_maximum_us == 14'039, "execution maximum is preserved") ||
        !expect(report.cpu_average_us == 10'000 && report.cpu_maximum_us == 10'000,
                "CPU timing is preserved") ||
        !expect(report.event_average_us == 1'000 && report.event_maximum_us == 1'000,
                "event timing is preserved") ||
        !expect(report.reconstruction_average_us == 3'000 && report.reconstruction_maximum_us == 3'000,
                "reconstruction timing is preserved") ||
        !expect(report.conversion_average_us == 2'000 && report.conversion_maximum_us == 2'000,
                "conversion timing is preserved") ||
        !expect(report.lcd_wait_average_us == 6'000 && report.lcd_wait_maximum_us == 6'000,
                "LCD wait timing is preserved") ||
        !expect(report.render_average_us == 8'100 && report.render_maximum_us == 8'100,
                "total render timing is preserved") ||
        !expect(report.work_average_us == 22'000 && report.work_maximum_us == 22'000,
                "work timing is preserved") ||
        !expect(report.iteration_average_us == 25'000 && report.iteration_maximum_us == 25'000,
                "iteration timing is preserved") ||
        !expect(report.effective_speed_tenths_percent == 800,
                "40 slices per second reports 80.0 percent CPC speed") ||
        !expect(report.presentation_fps_tenths == 400,
                "40 presented frames per second is reported")) return 2;
    if (!expect(!stats.report_due(2'000'100), "taking a report resets the interval")) return 3;
    std::puts("PASS: bounded performance aggregation and fixed-point telemetry calculations");
    return 0;
}
