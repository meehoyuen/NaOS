#include "kernel/irq.hpp"
#include "kernel/arch/cpu.hpp"
#include "kernel/arch/exception.hpp"
#include "kernel/arch/idt.hpp"
#include "kernel/arch/interrupt.hpp"
#include "kernel/lock.hpp"
#include "kernel/mm/list_node_cache.hpp"
#include "kernel/mm/memory.hpp"
#include "kernel/ucontext.hpp"
#include "kernel/util/linked_list.hpp"
namespace irq
{
using request_list_t = util::linked_list<request_func_data>;
using request_list_node_allocator_t = memory::list_node_cache_allocator<request_list_t>;

struct request_lock_list_t
{
    lock::spinlock_t spinlock;
    request_list_t *list;
    request_lock_list_t(memory::IAllocator *a)
        : list(memory::New<request_list_t>(memory::KernelCommonAllocatorV, a))
    {
    }
};

request_lock_list_t *irq_list;
request_lock_list_t *soft_irq_list;

request_list_node_allocator_t *list_allocator;

const int irq_count = 256;

bool _ctx_interrupt_ do_irq(const arch::idt::regs_t *regs, u64 extra_data)
{
    task::disable_preempt();

    auto &locked_list = irq_list[regs->vector];
    if (!locked_list.list->empty())
    {
        uctx::SpinLockContext icu(locked_list.spinlock);
        bool ok = false;
        for (auto &it : *locked_list.list)
        {
            auto ret = it.hard_func(regs, extra_data, it.user_data);
            if (ret == request_result::ok)
                ok = true;
        }
        task::enable_preempt();
        return ok;
    }
    task::enable_preempt();
    return false;
}

void do_soft_irq()
{
    task::disable_preempt();
    for (int x = 0; x < 4; x++)
    {
        for (int i = 0; i < soft_vector::COUNT; i++)
        {
            if (arch::cpu::current().is_irq_pending(i))
            {
                request_list_t &list = *soft_irq_list[i].list;

                uctx::SpinLockUnInterruptableContextController ctr(soft_irq_list[i].spinlock);
                ctr.begin();
                if (arch::cpu::current().is_irq_pending(i))
                {
                    arch::cpu::current().clean_irq_pending(i);

                    for (auto &it : list)
                    {
                        request_func_data fd = it;
                        ctr.end();
                        fd.soft_func(i, fd.user_data);
                        ctr.begin();
                    }
                }
                ctr.end();
            }
        }
    }
    task::enable_preempt();
}

bool check_and_wakeup_soft_irq(const arch::idt::regs_t *regs, u64 extra_data)
{
    auto &cpu = arch::cpu::current();
    if (cpu.has_task())
    {
        do_soft_irq();
        return true;
    }
    else if (cpu.is_in_kernel_context((void *)regs->rsp))
    {
        do_soft_irq();
        return true;
    }
    return false;
}

void wakeup_soft_irq_daemon() {}

void raise_soft_irq(u64 soft_irq_number)
{
    uctx::UnInterruptableContext uic;
    arch::cpu::current().set_irq_pending(soft_irq_number);
}

void init()
{
    list_allocator = memory::New<request_list_node_allocator_t>(memory::KernelCommonAllocatorV);

    {
        uctx::UnInterruptableContext uic;
        arch::exception::set_callback(&do_irq);
        arch::interrupt::set_callback(&do_irq);
        arch::interrupt::set_soft_irq_callback(&check_and_wakeup_soft_irq);

        irq_list = memory::NewArray<request_lock_list_t>(memory::KernelCommonAllocatorV, irq_count, list_allocator);
        soft_irq_list =
            memory::NewArray<request_lock_list_t>(memory::KernelCommonAllocatorV, soft_vector::COUNT, list_allocator);
    }
}

void insert_request_func(u32 vector, request_func func, u64 user_data)
{
    auto &locked_list = irq_list[vector];
    uctx::SpinLockUnInterruptableContext icu(locked_list.spinlock);
    locked_list.list->push_back(request_func_data((void *)func, user_data));
}

void remove_request_func(u32 vector, request_func func)
{
    auto &locked_list = irq_list[vector];
    uctx::SpinLockUnInterruptableContext icu(locked_list.spinlock);
    auto &list = *locked_list.list;
    for (auto it = list.begin(); it != list.end(); ++it)
    {
        if (it->hard_func == func)
        {
            list.remove(it);
            return;
        }
    }
}

void insert_soft_request_func(u32 vector, soft_request_func func, u64 user_data)
{
    auto &locked_list = soft_irq_list[vector];
    uctx::SpinLockUnInterruptableContext icu(locked_list.spinlock);
    locked_list.list->push_back(request_func_data((void *)func, user_data));
}

void remove_soft_request_func(u32 vector, soft_request_func func)
{
    auto &locked_list = soft_irq_list[vector];
    uctx::SpinLockUnInterruptableContext icu(locked_list.spinlock);
    request_list_t &list = *locked_list.list;
    for (auto it = list.begin(); it != list.end(); ++it)
    {
        if (it->soft_func == func)
        {
            list.remove(it);
            return;
        }
    }
}

} // namespace irq