#pragma once

#include <fast_io.h>
#include <nlohmann/json.hpp>
#include <process/process.hpp>
#include <queue>
#include <ranges>
#include <spf/spf-queue.hpp>

struct By_priority {
    bool operator()(Process const *lhs, Process const *rhs) const
    {
        return lhs->priority() > rhs->priority();
    }
};

struct By_arrival_time {
    bool operator()(Process const *lhs, Process const *rhs) const
    {
        return lhs->arrival_time < rhs->arrival_time;
    }
};

std::vector<Process::Id> to_vector(
    std::priority_queue<Process *, std::vector<Process *>, By_priority> ready);
std::vector<Process::Id> to_vector(std::queue<Process *> ready);

struct Frame {
    int system_time;
    std::vector<Process> processes;
    std::optional<Process> running_process;
    std::vector<Process::Id> not_ready_queue;
    std::vector<Process::Id> ready_queue;
    std::vector<Process::Id> finish_queue;
    nlohmann::json extra;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Frame, system_time, processes,
                                   running_process, not_ready_queue,
                                   ready_queue, finish_queue, extra)
};

class Priority_scheduling_queue {
    friend std::vector<Process::Id> to_vector(Priority_scheduling_queue ready)
    {
        return to_vector(std::move(ready).queue);
    }

  public:
    // Constructor (default)
    Priority_scheduling_queue() = default;

    // Check if the queue is empty
    [[nodiscard]] bool empty() const
    {
        return queue.empty();
    }

    // Get the size of the queue
    [[nodiscard]] std::size_t size() const
    {
        return queue.size();
    }

    // Mimic std::queue's front: return the highest-priority process (peek)
    [[nodiscard]] Process *front() const
    {
        return queue.top(); // top() gives the highest-priority element
    }

    // Mimic std::queue's push: add a process to the queue
    void push(Process *process)
    {
        queue.push(process);
    }

    // Mimic std::queue's pop: remove the highest-priority process
    void pop()
    {
        queue.pop(); // Removes the highest-priority element
    }

  private:
    std::priority_queue<Process *, std::vector<Process *>, By_priority> queue;
};

#define any_job_unfinished()                                                   \
    [&]() {                                                                    \
        return (jobs_it != jobs.end() || !ready.empty() ||                     \
                cpu.running_process() != nullptr);                             \
    }()

#define drive_time_events()                                                    \
    [&]() {                                                                    \
        while (jobs_it != jobs.end() &&                                        \
               cpu.system_time() >= jobs_it->arrival_time) {                   \
            ready.push(&*jobs_it);                                             \
            ++jobs_it;                                                         \
        }                                                                      \
    }()

#define push_frame()                                                           \
    [&]() {                                                                    \
        auto const not_ready_queue{                                            \
            std::vector(jobs_it, jobs.end()) |                                 \
            std::views::transform([](auto const &e) { return e.id; }) |        \
            std::ranges::to<std::vector>()};                                   \
        frames.push_back({.system_time = cpu.system_time().minutes(),          \
                          .processes = jobs,                                   \
                          .running_process = cpu.running_process() != nullptr  \
                                                 ? std::optional<Process>(     \
                                                       *cpu.running_process()) \
                                                 : std::nullopt,               \
                          .not_ready_queue = not_ready_queue,                  \
                          .ready_queue = to_vector(ready),                     \
                          .finish_queue = finish_queue});                      \
    }()

#define push_frame_extra(extra)                                                \
    [&]() {                                                                    \
        auto const not_ready_queue{                                            \
            std::vector(jobs_it, jobs.end()) |                                 \
            std::views::transform([](auto const &e) { return e.id; }) |        \
            std::ranges::to<std::vector>()};                                   \
        frames.push_back({.system_time = cpu.system_time().minutes(),          \
                          .processes = jobs,                                   \
                          .running_process = cpu.running_process() != nullptr  \
                                                 ? std::optional<Process>(     \
                                                       *cpu.running_process()) \
                                                 : std::nullopt,               \
                          .not_ready_queue = not_ready_queue,                  \
                          .ready_queue = to_vector(ready),                     \
                          .finish_queue = finish_queue,                        \
                          .extra = (extra)});                                  \
    }()

#define check_cpu_set_next()                                                   \
    [&]() {                                                                    \
        if (cpu.running_process() == nullptr && !ready.empty()) {              \
            cpu.set_running(ready.front());                                    \
            ready.pop();                                                       \
            push_frame();                                                      \
        }                                                                      \
    }()

#define check_cpu_set_next_extra(extra)                                        \
    [&]() {                                                                    \
        if (cpu.running_process() == nullptr && !ready.empty()) {              \
            cpu.set_running(ready.front());                                    \
            ready.pop();                                                       \
            push_frame_extra(extra);                                           \
        }                                                                      \
    }()

#define check_cpu_remove_finished()                                            \
    [&]() {                                                                    \
        if (cpu.running_process() != nullptr &&                                \
            cpu.running_process()->finished()) {                               \
            finish_queue.push_back(cpu.running_process()->id);                 \
            cpu.set_running(nullptr);                                          \
        }                                                                      \
    }()

std::string solve_first_come_fisrt_serve(CPU cpu, std::vector<Process> jobs);
std::string solve_short_process_first(CPU cpu, std::vector<Process> jobs);
std::string solve_round_robin(CPU cpu, std::vector<Process> jobs, int quantum);
std::string solve_priority_scheduling(CPU cpu, std::vector<Process> jobs);

std::string route_algorithm(std::string_view algorithm,
                            std::vector<Process> jobs);
