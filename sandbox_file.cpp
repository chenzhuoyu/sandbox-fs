#include "sandbox_file.h"

void SandboxFile::resize(size_t size) {
    if ((_mode & O_ACCMODE) == O_RDONLY) {
        throw FuseError(EBADF);
    } else {
        return do_resize(size);
    }
}

void SandboxFile::getstat(struct stat *stat) {
    do_getstat(stat);
}

ssize_t SandboxFile::read(char *buf, size_t len, size_t off) {
    if ((_mode & O_ACCMODE) == O_WRONLY) {
        throw FuseError(EBADF);
    } else {
        return do_read(buf, len, off);
    }
}

ssize_t SandboxFile::write(const char *buf, size_t len, size_t off) {
    if ((_mode & O_ACCMODE) == O_RDONLY) {
        throw FuseError(EBADF);
    } else {
        return do_write(buf, len, off);
    }
}
