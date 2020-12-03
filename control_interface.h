#ifndef SANDBOX_FS_CONTROL_INTERFACE_H
#define SANDBOX_FS_CONTROL_INTERFACE_H

#include <sys/stat.h>
#include <type_traits>
#include "sandbox_file.h"

struct ControlInterface {
    [[nodiscard]] virtual const char *        name()          const = 0;
    [[nodiscard]] virtual SandboxFile *       open(int flags) const = 0;
    [[nodiscard]] virtual const struct stat & stat()          const = 0;
};

template <typename T, const char Name[], mode_t Mode>
class Controller : public ControlInterface {
    struct stat _st;

public:
    virtual ~Controller() = default;
    explicit Controller() : _st() { FileNode::setstat(&_st, Mode); }

public:
    [[nodiscard]] const char *        name()          const override { return Name; }
    [[nodiscard]] SandboxFile *       open(int flags) const override { return new T(flags); }
    [[nodiscard]] const struct stat & stat()          const override { return _st; }
};

template <typename T, const char Name[], mode_t Mode>
struct ControlInterfaceAdapter : public SandboxFile {
    using SandboxFile::SandboxFile;

public:
    void do_resize  (size_t size)       override {}
    void do_getstat (struct stat *stat) override { *stat = iface()->stat(); }

public:
    ssize_t do_read  (char *buf, size_t len, size_t off)       override { return 0; }
    ssize_t do_write (const char *buf, size_t len, size_t off) override { return len; }

public:
    static ControlInterface *iface() {
        static Controller<T, Name, Mode> v;
        return &v;
    }
};

#endif /* SANDBOX_FS_CONTROL_INTERFACE_H */
