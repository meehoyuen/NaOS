#pragma once
#include "../../types.hpp"
#include "common.hpp"
namespace fs
{
namespace permission_flags
{
enum permission : flag_t
{
    exec = 1,
    write = 2,
    read = 4,

    group_exec = 16,
    group_write = 32,
    group_read = 64,

    other_exec = 256,
    other_write = 512,
    other_read = 1024,
};
} // namespace permission_flags

namespace access_flags
{
enum access_flags : flag_t
{
    exec = 1,
    write = 2,
    read = 4,
    exist = 8,
};
} // namespace access_flags

namespace mode
{
// open file mode
enum mode : flag_t
{
    read = 1,
    write = 2,
    bin = 4,
    append = 8,
};
} // namespace mode

namespace create_flags
{
enum create_flags : flag_t
{
    directory = 1,
    file = 2,
};
} // namespace create_flags

namespace path_walk_flags
{
enum path_walk_flags : flag_t
{
    auto_create_file = 1,
    continue_parse = 4,
    not_symbolic_link = 8,
    directory = 16,
    file = 32,
    cross_root = 64,
    symbolic_file = 128,
};
} // namespace path_walk_flags

enum class inode_type_t : u8
{
    file,
    directory,
    symbolink,
    other,
};
namespace symlink_flags
{
enum symlink_flags : flag_t
{
    directory = 1,

};
}
namespace vfs
{
class inode;
class dentry;
class file;
class file_system;
struct nameidata;
class super_block;
} // namespace vfs

struct dirstream
{
    file_desc fd;
};

namespace fs_flags
{
enum : flag_t
{
    kernel_only = 1,

};
} // namespace fs_flags

constexpr u64 directory_maximum_entry_size = 512;
constexpr u64 directory_maximum_path_size = 4096;

struct dir_entry_str
{
    char name[directory_maximum_entry_size];
    char *operator*() { return name; }
};

struct dir_path_str
{
    char name[directory_maximum_path_size];
    char *operator*() { return name; }
};

} // namespace fs
