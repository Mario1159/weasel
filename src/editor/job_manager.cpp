#include "job_manager.hpp"
#include <chrono>
#include <cstdio>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace editor {

job_manager& job_manager::get() {
    static job_manager instance;
    return instance;
}

void job_manager::add_job(const std::string& name, std::future<void>&& future) {
    std::lock_guard<std::mutex> const lock(m_mutex);
    job j;
    j.name = name;
    j.future = std::move(future);
    j.start_time = std::chrono::steady_clock::now();
    m_jobs.push_back(std::move(j));
}

void job_manager::update() {
    std::lock_guard<std::mutex> const lock(m_mutex);
    for (auto it = m_jobs.begin(); it != m_jobs.end(); ) {
        if (it->future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            try {
                it->future.get();
            } catch (...) {
                // Ignore errors
            }
            m_last_finished_job_name = it->name;
            auto end_time = std::chrono::steady_clock::now();
            m_last_job_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - it->start_time);
            it = m_jobs.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<job_manager::job_info> job_manager::get_active_jobs() {
    std::lock_guard<std::mutex> const lock(m_mutex);
    std::vector<job_info> infos;
    infos.reserve(m_jobs.size());
    for (const auto& j : m_jobs) {
        infos.push_back({j.name, j.progress});
    }
    return infos;
}

std::optional<std::string> job_manager::get_last_finished_info() {
    std::lock_guard<std::mutex> const lock(m_mutex);
    if (m_last_finished_job_name.empty()) { return std::nullopt;
}
    
    char buf[512];
    double const seconds = m_last_job_duration.count() / 1000.0;
    std::snprintf(buf, sizeof(buf), "%s finished successfully in %.2fs", m_last_finished_job_name.c_str(), seconds);
    return std::string(buf);
}

} // namespace editor
