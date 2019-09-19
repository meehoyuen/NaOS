#include "kernel/fs/vfs/dentry.hpp"
#include "kernel/fs/vfs/inode.hpp"
#include "kernel/fs/vfs/mm.hpp"
#include "kernel/fs/vfs/super_block.hpp"
#include "kernel/util/str.hpp"

namespace fs::vfs
{
dentry::dentry()
    : loaded_child(false)
    , child_dir_list(dentry_list_node_allocator)
    , child_file_list(dentry_list_node_allocator)

{
}

u64 dentry::hash() const { return 0; }

dentry *dentry::find_child_dir(const char *name) const
{
    for (auto it = child_dir_list.begin(); it != child_dir_list.end(); it = child_dir_list.next(it))
    {
        if (util::strcmp(name, it->element->name) == 0)
        {
            return it->element;
        }
    }
    return nullptr;
}

dentry *dentry::find_child_file(const char *name) const
{
    for (auto it = child_file_list.begin(); it != child_file_list.end(); it = child_file_list.next(it))
    {
        if (util::strcmp(name, it->element->name) == 0)
        {
            return it->element;
        }
    }
    return nullptr;
}

void dentry::set_parent(dentry *parent) { this->parent = parent; }

dentry *dentry::get_parent() const { return parent; }

void dentry::set_inode(inode *node) { this->node = node; }

inode *dentry::get_inode() const { return node; }

void dentry::set_name(const char *name) { this->name = name; }

const char *dentry::get_name() const { return name; }

void dentry::load_child() { node->get_super_block()->fill_dentry(this); }

void dentry::save_child() { node->get_super_block()->save_dentry(this); }

void dentry::add_sub_dir(dentry *child) { child_dir_list.push_back(child); }

void dentry::add_sub_file(dentry *file) { child_file_list.push_back(file); }

void dentry::remove_sub_dir(dentry *child) { child_dir_list.remove(child_dir_list.find(child)); }

void dentry::remove_sub_file(dentry *file) { child_file_list.remove(child_file_list.find(file)); }
} // namespace fs::vfs
