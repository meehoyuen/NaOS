#pragma once
#include "common.hpp"
#include "cpu.hpp"
namespace SMP
{

void init();

/// tlb shutdown
void flush_all_tlb();

void reschedule_cpu(u32 cpuid);

/// call per cpu function
void call_cpu(u32 cpuid, cpu::call_cpu_func_t, u64 user_data);

} // namespace SMP
