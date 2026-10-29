#pragma once

#include <cstdint>

namespace cpcexplorer::performance {

constexpr std::uint32_t kCpcSliceUs = 20'000;
constexpr std::uint64_t kReportIntervalUs = 1'000'000;

struct Sample {
    std::uint32_t emulated_us = 0;
    std::uint32_t execution_us = 0;
    std::uint32_t cpu_us = 0;
    std::uint32_t event_us = 0;
    std::uint32_t reconstruction_us = 0;
    std::uint32_t conversion_us = 0;
    std::uint32_t lcd_wait_us = 0;
    std::uint32_t render_us = 0;
    std::uint32_t work_us = 0;
    std::uint32_t iteration_us = 0;
    bool presented_frame = false;
    bool missed_deadline = false;
};

struct Report {
    std::uint64_t interval_us = 0;
    std::uint64_t emulated_total_us = 0;
    std::uint32_t slices = 0;
    std::uint32_t presented_frames = 0;
    std::uint32_t missed_deadlines = 0;
    std::uint32_t execution_average_us = 0;
    std::uint32_t execution_maximum_us = 0;
    std::uint32_t cpu_average_us = 0;
    std::uint32_t cpu_maximum_us = 0;
    std::uint32_t event_average_us = 0;
    std::uint32_t event_maximum_us = 0;
    std::uint32_t reconstruction_average_us = 0;
    std::uint32_t reconstruction_maximum_us = 0;
    std::uint32_t conversion_average_us = 0;
    std::uint32_t conversion_maximum_us = 0;
    std::uint32_t lcd_wait_average_us = 0;
    std::uint32_t lcd_wait_maximum_us = 0;
    std::uint32_t render_average_us = 0;
    std::uint32_t render_maximum_us = 0;
    std::uint32_t work_average_us = 0;
    std::uint32_t work_maximum_us = 0;
    std::uint32_t iteration_average_us = 0;
    std::uint32_t iteration_maximum_us = 0;
    std::uint32_t effective_speed_tenths_percent = 0;
    std::uint32_t presentation_fps_tenths = 0;
};

class Aggregator {
public:
    void reset(std::uint64_t now_us);
    void add(const Sample& sample);
    bool report_due(std::uint64_t now_us) const;
    Report take_report(std::uint64_t now_us);

private:
    std::uint64_t interval_start_us_ = 0;
    std::uint64_t execution_sum_us_ = 0;
    std::uint64_t emulated_sum_us_ = 0;
    std::uint64_t cpu_sum_us_ = 0;
    std::uint64_t event_sum_us_ = 0;
    std::uint64_t reconstruction_sum_us_ = 0;
    std::uint64_t conversion_sum_us_ = 0;
    std::uint64_t lcd_wait_sum_us_ = 0;
    std::uint64_t render_sum_us_ = 0;
    std::uint64_t work_sum_us_ = 0;
    std::uint64_t iteration_sum_us_ = 0;
    std::uint32_t slices_ = 0;
    std::uint32_t presented_frames_ = 0;
    std::uint32_t missed_deadlines_ = 0;
    std::uint32_t execution_maximum_us_ = 0;
    std::uint32_t cpu_maximum_us_ = 0;
    std::uint32_t event_maximum_us_ = 0;
    std::uint32_t reconstruction_maximum_us_ = 0;
    std::uint32_t conversion_maximum_us_ = 0;
    std::uint32_t lcd_wait_maximum_us_ = 0;
    std::uint32_t render_maximum_us_ = 0;
    std::uint32_t work_maximum_us_ = 0;
    std::uint32_t iteration_maximum_us_ = 0;
};

}  // namespace cpcexplorer::performance
