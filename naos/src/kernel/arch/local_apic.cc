#include "kernel/arch/local_apic.hpp"
#include "kernel/arch/cpu_info.hpp"
#include "kernel/arch/interrupt.hpp"
#include "kernel/arch/klib.hpp"
#include "kernel/arch/pit.hpp"
#include "kernel/irq.hpp"
#include "kernel/mm/memory.hpp"
#include "kernel/trace.hpp"
#include "kernel/ucontext.hpp"

const u16 id_register = 2;
const u16 version_register = 3;
const u16 task_priority_register = 8;
const u16 arbitration_priority_register = 9;
const u16 processor_priority_register = 10;
const u16 eoi_register = 11;
const u16 remote_read_register = 12;
const u16 logical_destination_register = 13;
const u16 destination_format_register = 14;
const u16 spurious_interrupt_vector_register = 15;
const u16 isr_0 = 16;
const u16 isr_1 = 17;
const u16 isr_2 = 18;
const u16 isr_3 = 19;
const u16 isr_4 = 20;
const u16 isr_5 = 21;
const u16 isr_6 = 22;
const u16 isr_7 = 23;

const u16 tmr_0 = 24;
const u16 tmr_1 = 25;
const u16 tmr_2 = 26;
const u16 tmr_3 = 27;
const u16 tmr_4 = 28;
const u16 tmr_5 = 29;
const u16 tmr_6 = 30;
const u16 tmr_7 = 31;

const u16 irr_0 = 32;
const u16 irr_1 = 33;
const u16 irr_2 = 34;
const u16 irr_3 = 35;
const u16 irr_4 = 36;
const u16 irr_5 = 37;
const u16 irr_6 = 38;
const u16 irr_7 = 39;
const u16 esr = 40;

const u16 lvt_cmci = 47; // 0x82F
const u16 icr_0 = 48;
const u16 icr_1 = 49;
const u16 lvt_timer = 50;
const u16 lvt_thermal_sensor_register = 51;
const u16 lvt_performance_register = 52;
const u16 lvt_lint_0 = 53;
const u16 lvt_lint_1 = 54;
const u16 lvt_error = 55;
const u16 timer_initial_count_register = 56;
const u16 timer_current_count_register = 57;

const u16 timer_divide_register = 62; // 0x83E

const u16 self_pip = 63;

const u16 lvt_index_array[] = {
    lvt_cmci, lvt_timer, lvt_thermal_sensor_register, lvt_performance_register, lvt_lint_0, lvt_lint_1, lvt_error, 0};

namespace arch::APIC
{

void *apic_base_addr;
u64 ms_tsc;

u32 read_register_MSR(u16 reg)
{
    reg += 0x800;
    return _rdmsr(reg);
}

void write_register_MSR(u16 reg, u32 v)
{
    reg += 0x800;
    _wrmsr(reg, v);
}

u32 read_register_mm(u16 reg)
{
    reg <<= 4;
    u32 v = *(u32 *)((byte *)apic_base_addr + reg);
    _mfence();
    return v;
}

void write_register_mm(u16 reg, u32 v)
{
    reg <<= 4;
    *(u32 *)((byte *)apic_base_addr + reg) = v;
    _mfence();
}

typedef u32 (*read_register_func)(u16);
typedef void (*write_register_func)(u16, u32);

typedef u64 (*io_read_register_func)(u16);
typedef void (*io_write_register_func)(u16, u64);

read_register_func read_register;
write_register_func write_register;

u64 current_apic_id() { return _rdmsr(0x802); }

void disable_all_lvt()
{
    u16 i = 1;

    while (lvt_index_array[i] != 0)
    {
        write_register(lvt_index_array[i], 1 << 16);
        i++;
    }
} // namespace arch::APIC

void local_init()
{
    u32 id;
    u64 v = (1 << 11);
    if (cpu_info::has_future(cpu_info::future::x2apic))
    {
        trace::debug("x2APIC is supported");
        v |= (1 << 10);
        read_register = read_register_MSR;
        write_register = write_register_MSR;
    }
    else
    {
        read_register = read_register_mm;
        write_register = write_register_mm;
    }

    trace::debug("Enable Local APIC...");
    apic_base_addr = (void *)(_rdmsr(0x1B) & ~((1 << 13) - 1));
    apic_base_addr = (void *)(_rdmsr(0x1B) & ~((1 << 13) - 1));

    _wrmsr(0x1B, _rdmsr(0x1B) | v);
    trace::debug("APIC base ", apic_base_addr);

    kassert((_rdmsr(0x1B) & v) == v, "Can't enable (IA32_APIC_BASE) APIC value ", (void *)_rdmsr(0x1B));

    apic_base_addr = memory::kernel_phyaddr_to_virtaddr(apic_base_addr);

    u64 version_value = read_register(version_register);
    v = (1 << 8);
    u16 lvtCount = ((version_value & 0xFF0000) >> 16) + 1;
    u8 version = version_value & 0xFF;
    trace::debug("Local APIC version ", version, ", LVT count ", lvtCount);

    id = read_register(id_register) >> 24;
    trace::debug("Local APIC ID ", id);

    if (version > 0xf)
    {
        trace::debug("Use Intergrated APIC");
    }
    else
    {
        trace::debug("Use 82489DX");
    }

    v = 1 << 8;
    if ((version_value & (1 << 24)) != 0)
    {
        v |= 1 << 12; // disable broadcast EOI
    }

    // enable software Local APIC
    write_register(spurious_interrupt_vector_register, read_register(spurious_interrupt_vector_register) | v);
    kassert((read_register(spurious_interrupt_vector_register) & v) == v, "Can't software enable local-APIC value ",
            (void *)(u64)read_register(spurious_interrupt_vector_register));

    disable_all_lvt();
}

void local_software_enable()
{
    u32 v = 1 << 8;
    write_register(spurious_interrupt_vector_register, read_register(spurious_interrupt_vector_register) | v);
}

void local_software_disable()
{
    u32 v = 1 << 8;
    write_register(spurious_interrupt_vector_register, read_register(spurious_interrupt_vector_register) & ~v);
}

void local_disable(u8 index)
{
    write_register(lvt_index_array[index], read_register(lvt_index_array[index]) | (1 << 16));
}

void local_irq_setup(u8 index, u8 vector, u8 flags)
{
    write_register(lvt_index_array[index], vector | ((u16)flags << 8));
}

void local_enable(u8 index)
{
    write_register(lvt_index_array[index], read_register(lvt_index_array[index]) & ~(1 << 16));
}

void local_EOI(u8 index) { write_register(eoi_register, 0); }

void local_post_IPI(u64 targes) {}

irq::request_result _ctx_interrupt_ on_event(const arch::idt::regs_t *regs, u64 extra_data, u64 user_data)
{
    auto event = (clock_event *)user_data;

    if (likely(event && !event->is_suspend))
    {
        event->tick_count++;
        irq::raise_soft_irq(irq::soft_vector::timer);
        return irq::request_result::ok;
    }
    return irq::request_result::no_handled;
}

/*
divide configuration register
000: Divide by 2
001: Divide by 4
010: Divide by 8
011: Divide by 16
100: Divide by 32
101: Divide by 64
110: Divide by 128
111: Divide by 1
*/
u8 dv_table[] = {2, 4, 8, 16, 32, 64, 128, 1};
u8 divide_value(u8 d) { return dv_table[d]; }

void clock_event::init(u64 HZ)
{
    tick_count = 0;
    this->hz = HZ;
    init_counter = 10000;
    divide = 0b001;

    is_suspend = false;
    suspend();
    local_irq_setup(lvt_index::timer, irq::hard_vector::local_apic_timer, 0);
    write_register(lvt_index_array[lvt_index::timer], read_register(lvt_index_array[lvt_index::timer]) | (0b01 << 17));
}

void clock_event::destroy() {}

void clock_event::suspend()
{
    if (!is_suspend)
    {
        uctx::UnInterruptableContext icu;
        is_suspend = true;
        irq::remove_request_func(irq::hard_vector::local_apic_timer, on_event);
        local_disable(lvt_index::timer);

        write_register(timer_initial_count_register, 0xFFFFFFFF);
        write_register(timer_divide_register, 0);
    }
}

void clock_event::resume()
{
    if (is_suspend)
    {
        uctx::UnInterruptableContext icu;

        local_enable(lvt_index::timer);

        write_register(timer_initial_count_register, init_counter);
        write_register(timer_divide_register, divide);
        is_suspend = false;
        irq::insert_request_func(irq::hard_vector::local_apic_timer, on_event, (u64)this);
    }
}

void clock_event::wait_next_tick()
{
    u64 old = tick_count;
    while (tick_count == old)
        ;

    kassert(tick_count > old, "Unexpected value");
}
void clock_source::init() {}

void clock_source::destroy() {}

u64 clock_source::current()
{
    clock_event *ev = (clock_event *)event;
    return 1000000UL * ev->tick_count / ev->hz + (read_register(timer_current_count_register) * 1000000UL) /
                                                     ev->init_counter / divide_value(ev->divide) / 1000000UL;
}
u64 clock_source::calibrate_apic(::clock::clock_source *cs)
{
    clock_event *ev = (clock_event *)event;
    // 5ms
    u64 test_time = 5000;
    ev->resume();
    cs->get_event()->resume();
    cs->get_event()->wait_next_tick();
    cs->current();
    ev->wait_next_tick();
    u64 t = test_time + cs->current();
    u64 start_count = ev->tick_count;
    while (cs->current() < t)
        ;
    u64 end_count = ev->tick_count;

    t = cs->current() - t + test_time;

    cs->get_event()->suspend();
    ev->suspend();
    return (end_count - start_count) * 1000000 / t;
}

void clock_source::calibrate(::clock::clock_source *cs)
{
    clock_event *ev = (clock_event *)event;

    i64 apic_hz[7];
    i64 sum = 0;
    for (auto &i : apic_hz)
    {
        i = calibrate_apic(cs);
        sum += i;
    }
    sum /= (sizeof(apic_hz) / (sizeof(apic_hz[0])));
    u64 hz = sum;
    u64 abs_delta = 0;
    for (auto i : apic_hz)
    {
        i64 d = (i - sum);
        abs_delta += d * d;
    }

    abs_delta /= (sizeof(apic_hz) / (sizeof(apic_hz[0])));

    u64 frq = hz * ev->init_counter * divide_value(ev->divide);
    ev->bus_frequency = frq;

    trace::debug("Local APIC Timer ", frq, "HZ. ", frq / 1000000, "MHZ. ", "divide ", divide_value(ev->divide));

    trace::debug("cpu bus frequency ", frq / 1000000, "MHZ");

    u64 c = frq / (divide_value(ev->divide) * ev->hz);
    ev->init_counter = c;
    trace::debug("set counter ", c);
    u64 currentHZ = calibrate_apic(cs);
    trace::debug("Local APIC Timer ", currentHZ, "HZ. ", currentHZ / 1000000, "MHZ");

    ev->hz = currentHZ;
}

clock_source *global_lt_cs = nullptr;
clock_event *global_lt_ev = nullptr;

clock_source *make_clock()
{
    if (global_lt_cs == nullptr)
    {
        global_lt_cs = memory::New<clock_source>(memory::KernelCommonAllocatorV);
        global_lt_ev = memory::New<clock_event>(memory::KernelCommonAllocatorV);
        global_lt_ev->set_source(global_lt_cs);
        global_lt_cs->set_event(global_lt_ev);
        global_lt_ev->init(1000);
        global_lt_cs->init();
    }
    return global_lt_cs;
}

} // namespace arch::APIC