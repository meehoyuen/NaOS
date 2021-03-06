#include "kernel/mm/slab.hpp"
#include "kernel/lock.hpp"
#include "kernel/ucontext.hpp"
#include "kernel/util/str.hpp"
namespace memory
{

slab_cache_pool *global_kmalloc_slab_domain, *global_dma_slab_domain, *global_object_slab_domain;
void slab_group::new_memory_node()
{
    slab *s = (slab *)memory::KernelBuddyAllocatorV->allocate(page_pre_slab * memory::page_size, 8);
    new (s) slab(node_pre_slab, 0);

    s->data_ptr = (char *)s + sizeof(slab);
    s->data_ptr = (char *)(((u64)s->data_ptr + align - 1) & ~(align - 1));
    s->bitmap.clean_all();
    list_empty.push_back(s);
    all_obj_count += node_pre_slab;
}

void slab_group::delete_memory_node(slab *s)
{
    kassert(s->rest == node_pre_slab, "slab error rest:", s->rest, " target:", node_pre_slab);
    all_obj_count -= node_pre_slab;
    s->~slab();
    memory::KernelBuddyAllocatorV->deallocate(s);
}

slab_group::slab_group(memory::IAllocator *allocator, u64 size, const char *name, u64 align, u64 flags)
    : obj_align_size((size + align - 1) & ~(align - 1))
    , size(size)
    , name(name)
    , list_empty(allocator)
    , list_partial(allocator)
    , list_full(allocator)
    , flags(flags)
    , align(align)
    , all_obj_count(0)
    , all_obj_used(0)
{
    u64 restsize = memory::page_size - ((sizeof(slab) + align - 1) & ~(align - 1));

    if (obj_align_size * 64 > restsize)
    {
        if (obj_align_size * 16 > restsize)
        {
            if (obj_align_size * 4 > restsize)
            {
                if (obj_align_size > restsize)
                {
                    page_pre_slab = 16;
                    node_pre_slab = (restsize + memory::page_size * 15) / obj_align_size;
                }
                else
                {
                    page_pre_slab = 8;
                    node_pre_slab = (restsize + memory::page_size * 7) / obj_align_size;
                }
            }
            else
            {
                page_pre_slab = 4;
                node_pre_slab = (restsize + memory::page_size * 3) / obj_align_size;
            }
        }
        else
        {
            // 63 - 251
            page_pre_slab = 2;
            node_pre_slab = (restsize + memory::page_size) / obj_align_size;
        }
    }
    else
    {
        // first page can save element >= 64
        page_pre_slab = 1;
        node_pre_slab = restsize / obj_align_size;
    }
}

void *slab_group::alloc()
{
    uctx::RawWriteLockUninterruptibleContext ctx(slab_lock);
    if (list_partial.empty())
    {
        if (list_empty.empty())
        {
            new_memory_node();
        }
        list_partial.push_back(list_empty.pop_back());
    }
    slab *slab = list_partial.back();
    auto &bitmap = slab->bitmap;
    u64 i = bitmap.scan_zero();
    kassert(i < bitmap.count() && i < node_pre_slab, "Memory leacorruption in slab");
    bitmap.set(i);
    kassert(bitmap.get(i), "Can't allocate an addresss. index: ", i);

    slab->rest--;
    all_obj_used++;

    if (slab->rest == 0)
    {
        list_full.push_back(list_partial.pop_back());
    }
    return slab->data_ptr + i * obj_align_size;
}

void slab_group::free(void *ptr)
{
    uctx::RawWriteLockUninterruptibleContext ctx(slab_lock);

    char *page_addr = (char *)((u64)ptr & ~(memory::page_size * page_pre_slab - 1));

    for (auto it = list_partial.begin(); it != list_partial.end(); ++it)
    {
        if (page_addr == (char *)*it)
        {
            auto &e = **it;
            u64 index = ((char *)ptr - e.data_ptr) / obj_align_size;
            kassert(e.bitmap.get(index), "Not an assigned address.");
            e.bitmap.clean(index);
            e.rest++;
            all_obj_used--;

            if (e.rest == node_pre_slab)
            {
                list_empty.push_back(*it);
                list_partial.remove(it);

                if (all_obj_used * 2 < all_obj_count)
                {
                    delete_memory_node(&e);
                    list_empty.pop_back();
                }
            }

            return;
        }
    }
    for (auto it = list_full.begin(); it != list_full.end(); ++it)
    {
        if (page_addr == (char *)*it)
        {
            auto &e = **it;
            e.bitmap.clean(((char *)ptr - e.data_ptr) / obj_align_size);
            e.rest++;
            all_obj_used--;
            list_partial.push_back(*it);
            list_full.remove(it);
            return;
        }
    }
    trace::panic("Unreachable control flow.");
}

bool slab_group::include_address(void *ptr)
{
    uctx::RawReadLockUninterruptibleContext ctx(slab_lock);

    char *page_addr = (char *)((u64)ptr & ~(memory::page_size * page_pre_slab - 1));
    for (auto e : list_full)
    {
        if (page_addr == (char *)e)
            return true;
    }

    for (auto e : list_partial)
    {
        if (page_addr == (char *)e)
            return true;
    }

    for (auto e : list_empty)
    {
        if (page_addr == (char *)e)
            return true;
    }

    return false;
}

slab_group_list_t::iterator slab_cache_pool::find_slab_group_node(const char *name)
{
    uctx::RawReadLockUninterruptibleContext ctx(group_lock);

    auto group = slab_groups.begin();
    while (group != slab_groups.end())
    {
        if (util::strcmp(group->get_name(), name) == 0)
        {
            return group;
        }
        ++group;
    }
    return slab_groups.end();
}

slab_group *slab_cache_pool::find_slab_group(const char *name)
{
    uctx::RawReadLockUninterruptibleContext ctx(group_lock);

    auto group_it = find_slab_group_node(name);
    if (group_it != slab_groups.end())
    {
        return &group_it;
    }
    return nullptr;
}

slab_group *slab_cache_pool::find_slab_group(u64 size)
{
    uctx::RawReadLockUninterruptibleContext ctx(group_lock);

    auto group = slab_groups.begin();
    while (group != slab_groups.end())
    {
        if (group->get_size() == size)
        {
            return &group;
        }
        ++group;
    }
    return nullptr;
}

slab_group *slab_cache_pool::create_new_slab_group(u64 size, const char *name, u64 align, u64 flags)
{
    uctx::RawWriteLockUninterruptibleContext ctx(group_lock);
    return &slab_groups.emplace_back(&slab_list_node_allocator, size, name, align, flags);
}

void slab_cache_pool::remove_slab_group(slab_group *slab_obj)
{
    uctx::RawWriteLockUninterruptibleContext ctx(group_lock);

    auto group = slab_groups.begin();
    while (group != slab_groups.end())
    {
        if (&group == slab_obj)
        {
            slab_groups.remove(group);
            return;
        }
        ++group;
    }
}

slab_cache_pool::slab_cache_pool()
    : slab_groups(memory::KernelBuddyAllocatorV)
{
}

void *SlabObjectAllocator::allocate(u64 size, u64 align)
{
    if (unlikely(size > slab_obj->get_size()))
        trace::panic("slab allocator can not alloc a larger size");

    return slab_obj->alloc();
};

void SlabObjectAllocator::deallocate(void *ptr) { slab_obj->free(ptr); };

} // namespace memory