#include "performance_metrics.h"

#include <algorithm>

namespace cpcexplorer::performance {
namespace {

std::uint32_t average(std::uint64_t total, std::uint32_t count) {
    return count == 0 ? 0 : static_cast<std::uint32_t>(total / count);
}

}  // namespace

void Aggregator::reset(std::uint64_t now_us) {
    interval_start_us_ = now_us;
    execution_sum_us_ = emulated_sum_us_ = cpu_sum_us_ = event_sum_us_ = reconstruction_sum_us_ = 0;
    conversion_sum_us_ = lcd_wait_sum_us_ = render_sum_us_ = 0;
    work_sum_us_ = iteration_sum_us_ = 0;
    slices_ = presented_frames_ = missed_deadlines_ = 0;
    execution_maximum_us_ = cpu_maximum_us_ = event_maximum_us_ = reconstruction_maximum_us_ = 0;
    conversion_maximum_us_ = lcd_wait_maximum_us_ = 0;
    render_maximum_us_ = 0;
    work_maximum_us_ = iteration_maximum_us_ = 0;
}

void Aggregator::add(const Sample& sample) {
    ++slices_;
    if (sample.presented_frame) ++presented_frames_;
    if (sample.missed_deadline) ++missed_deadlines_;
    emulated_sum_us_ += sample.emulated_us;
    execution_sum_us_ += sample.execution_us;
    cpu_sum_us_ += sample.cpu_us;
    event_sum_us_ += sample.event_us;
    reconstruction_sum_us_ += sample.reconstruction_us;
    conversion_sum_us_ += sample.conversion_us;
    lcd_wait_sum_us_ += sample.lcd_wait_us;
    render_sum_us_ += sample.render_us;
    work_sum_us_ += sample.work_us;
    iteration_sum_us_ += sample.iteration_us;
    execution_maximum_us_ = std::max(execution_maximum_us_, sample.execution_us);
    cpu_maximum_us_ = std::max(cpu_maximum_us_, sample.cpu_us);
    event_maximum_us_ = std::max(event_maximum_us_, sample.event_us);
    reconstruction_maximum_us_ = std::max(reconstruction_maximum_us_, sample.reconstruction_us);
    conversion_maximum_us_ = std::max(conversion_maximum_us_, sample.conversion_us);
    lcd_wait_maximum_us_ = std::max(lcd_wait_maximum_us_, sample.lcd_wait_us);
    render_maximum_us_ = std::max(render_maximum_us_, sample.render_us);
    work_maximum_us_ = std::max(work_maximum_us_, sample.work_us);
    iteration_maximum_us_ = std::max(iteration_maximum_us_, sample.iteration_us);
}

bool Aggregator::report_due(std::uint64_t now_us) const {
    return slices_ != 0 && now_us - interval_start_us_ >= kReportIntervalUs;
}

Report Aggregator::take_report(std::uint64_t now_us) {
    Report report{};
    report.interval_us = now_us - interval_start_us_;
    report.emulated_total_us = emulated_sum_us_;
    report.slices = slices_;
    report.presented_frames = presented_frames_;
    report.missed_deadlines = missed_deadlines_;
    report.execution_average_us = average(execution_sum_us_, slices_);
    report.execution_maximum_us = execution_maximum_us_;
    report.cpu_average_us = average(cpu_sum_us_, slices_);
    report.cpu_maximum_us = cpu_maximum_us_;
    report.event_average_us = average(event_sum_us_, slices_);
    report.event_maximum_us = event_maximum_us_;
    report.reconstruction_average_us = average(reconstruction_sum_us_, slices_);
    report.reconstruction_maximum_us = reconstruction_maximum_us_;
    report.conversion_average_us = average(conversion_sum_us_, slices_);
    report.conversion_maximum_us = conversion_maximum_us_;
    report.lcd_wait_average_us = average(lcd_wait_sum_us_, slices_);
    report.lcd_wait_maximum_us = lcd_wait_maximum_us_;
    report.render_average_us = average(render_sum_us_, slices_);
    report.render_maximum_us = render_maximum_us_;
    report.work_average_us = average(work_sum_us_, slices_);
    report.work_maximum_us = work_maximum_us_;
    report.iteration_average_us = average(iteration_sum_us_, slices_);
    report.iteration_maximum_us = iteration_maximum_us_;
    if (report.interval_us != 0) {
        report.effective_speed_tenths_percent = static_cast<std::uint32_t>(
            emulated_sum_us_ * 1000U / report.interval_us);
        report.presentation_fps_tenths = static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(presented_frames_) * 10'000'000U / report.interval_us);
    }
    reset(now_us);
    return report;
}

}  // namespace cpcexplorer::performance
