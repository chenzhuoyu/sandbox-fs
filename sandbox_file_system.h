#ifndef SANDBOX_FS_SANDBOX_FILE_SYSTEM_H
#define SANDBOX_FS_SANDBOX_FILE_SYSTEM_H

#include <string>
#include <utility>

#include <fuse.h>
#include <sys/stat.h>

#include "file_node.h"
#include "fuse_error.h"
#include "sandbox_file.h"
#include "control_interface.h"

class SandboxFileSystem {
    FileNode::Node     _root;
    ControlInterface * _ctrl;

public:
   ~SandboxFileSystem();
    SandboxFileSystem(FileNode::Node root, ControlInterface *iface);

public:
    void start(const std::string &mount, const std::string &options = "");

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

private:
    void do_open(const char *path, struct fuse_file_info *fi);
    long do_read(const char *path, char *buf, size_t size, off_t off, struct fuse_file_info *fi);
    void do_rmdir(const char *path);
    void do_mkdir(const char *path, mode_t mode);
    long do_write(const char *path, const char *buf, size_t size, off_t off, struct fuse_file_info *fi);
    void do_create(const char *path, mode_t mode, struct fuse_file_info *fi);
    void do_unlink(const char *path);
    void do_access(const char *path, int mode);
    void do_rename(const char *path, const char *dest);
    void do_getattr(const char *path, struct stat *stat);
    void do_utimens(const char *path, const struct timespec *tv);
    void do_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t off, struct fuse_file_info *fi);
    void do_release(const char *path, struct fuse_file_info *fi);
    void do_truncate(const char *path, off_t off);
    void do_fgetattr(const char *path, struct stat *stat, struct fuse_file_info *fi);
    void do_ftruncate(const char *path, off_t off, struct fuse_file_info *fi);

#pragma clang diagnostic pop

private:
    static int fs_open(const char *path, struct fuse_file_info *fi);
    static int fs_read(const char *path, char *buf, size_t size, off_t off, struct fuse_file_info *fi);
    static int fs_rmdir(const char *path);
    static int fs_mkdir(const char *path, mode_t mode);
    static int fs_write(const char *path, const char *buf, size_t size, off_t off, struct fuse_file_info *fi);
    static int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
    static int fs_unlink(const char *path);
    static int fs_access(const char *path, int mode);
    static int fs_rename(const char *path, const char *dest);
    static int fs_getattr(const char *path, struct stat *stat);
    static int fs_utimens(const char *path, const struct timespec *tv);
    static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t fn, off_t off, struct fuse_file_info *fi);
    static int fs_release(const char *path, struct fuse_file_info *fi);
    static int fs_truncate(const char *path, off_t off);
    static int fs_fgetattr(const char *path, struct stat *stat, struct fuse_file_info *fi);
    static int fs_ftruncate(const char *path, off_t off, struct fuse_file_info *fi);
};

#endif /* SANDBOX_FS_SANDBOX_FILE_SYSTEM_H */
