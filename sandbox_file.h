#ifndef SANDBOX_FS_SANDBOX_FILE_H
#define SANDBOX_FS_SANDBOX_FILE_H

#include <cerrno>
#include <cstddef>
#include <fcntl.h>

#include "fuse_error.h"

class SandboxFile {
    int _mode;

public:
    virtual ~SandboxFile() = default;
    explicit SandboxFile(int mode) : _mode(mode) {}

public:
    void resize(size_t size);
    void getstat(struct stat *stat);

public:
    ssize_t read(char *buf, size_t len, size_t off);
    ssize_t write(const char *buf, size_t len, size_t off);

protected:
    virtual void do_resize(size_t size) = 0;
    virtual void do_getstat(struct stat *stat) = 0;

protected:
    virtual ssize_t do_read(char *buf, size_t len, size_t off) = 0;
    virtual ssize_t do_write(const char *buf, size_t len, size_t off) = 0;
};

#endif /* SANDBOX_FS_SANDBOX_FILE_H */
