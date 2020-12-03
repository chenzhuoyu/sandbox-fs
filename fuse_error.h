#ifndef SANDBOX_FS_FUSE_ERROR_H
#define SANDBOX_FS_FUSE_ERROR_H

#include <cerrno>
#include <string>
#include <stdexcept>

class FuseError : public std::runtime_error {
    int         _code;
    std::string _message;

public:
    explicit FuseError()                          : FuseError(errno) {}
    explicit FuseError(int code)                  : FuseError(code, strerror(code)) {}
    explicit FuseError(int code, std::string msg) : runtime_error("fuse error: [" + std::to_string(code) + "] " + msg),
        _code    (code),
        _message (std::move(msg)) {}

public:
    [[nodiscard]] int                 code()    const { return _code; }
    [[nodiscard]] const std::string & message() const { return _message; }

};

#endif /* SANDBOX_FS_FUSE_ERROR_H */
