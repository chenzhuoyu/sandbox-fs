#ifndef SANDBOX_FS_FILE_BACKEND_H
#define SANDBOX_FS_FILE_BACKEND_H

#include <string>
#include <vector>
#include <archive.h>
#include <sys/stat.h>

#include "backend.h"

class FileBackend : public Backend {
    struct archive *fp;

public:
    virtual ~FileBackend();
    explicit FileBackend(const std::string &fname);

public:
    void foreach(std::function<void(std::string, struct stat, ByteBuffer)> &&func) const override;
};

#endif /* SANDBOX_FS_FILE_BACKEND_H */
