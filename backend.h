#ifndef SANDBOX_FS_BACKEND_H
#define SANDBOX_FS_BACKEND_H

#include <string>
#include <vector>
#include <functional>
#include <sys/stat.h>

#include "byte_buffer.h"

struct Backend {
    virtual void foreach(std::function<void (std::string path, struct stat stat, ByteBuffer data)> &&func) const = 0;
};

#endif /* SANDBOX_FS_BACKEND_H */
