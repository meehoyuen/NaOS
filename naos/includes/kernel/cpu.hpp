#pragma once
#include "common.hpp"
namespace task
{
struct thread_t;
} // namespace task

namespace cpu
{
class cpu_data_t
{
    task::thread_t *current_task;
    task::thread_t *idle_task;
    void *schedule_data;

  public:
    bool is_bsp();
    int id();
    void set_task(task::thread_t *task);

    task::thread_t *get_task() { return current_task; }
    void set_idle_task(::task::thread_t *task) { idle_task = task; }
    task::thread_t *get_idle_task() { return idle_task; }

    bool has_task() { return current_task != nullptr; }
};
cpu_data_t &current();
void init();
u64 count();
} // namespace cpu