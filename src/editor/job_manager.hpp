#pragma once

#include <string>
#include <vector>
#include <future>
#include <chrono>
#include <optional>
#include <mutex>

namespace editor {

struct job {
    std::string name;
    std::future<void> future;
    std::chrono::steady_clock::time_point start_time;
    float progress = -1.0F; // -1.0f for indeterminate
};

class job_manager {
public:
    static job_manager& get();

    void add_job(const std::string& name, std::future<void>&& future);
    void update();

    struct job_info {
        std::string name;
        float progress;
    };

    std::vector<job_info> get_active_jobs();
    std::optional<std::string> get_last_finished_info();

private:
    std::mutex m_mutex;
    std::vector<job> m_jobs;
    std::string m_last_finished_job_name;
    std::chrono::milliseconds m_last_job_duration{0};
};

} // namespace editor
