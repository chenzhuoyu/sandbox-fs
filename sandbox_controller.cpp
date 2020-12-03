#include <string>
#include <stdexcept>

#include <folly/Random.h>
#include <folly/logging/xlog.h>
#include <folly/concurrency/ConcurrentHashMap.h>

#include "fuse_error.h"
#include "file_backend.h"
#include "sandbox_controller.h"

ssize_t SandboxController::do_read(char *buf, size_t len, size_t off) {
    return _rbuf.sgetn(buf, len);
}

ssize_t SandboxController::do_write(const char *buf, size_t len, size_t off) {
    _wbuf.sputn(buf, len);
    fireCommand(buf, len);
    return len;
}

void SandboxController::reply(const JSON &v) {
    auto s = v.dump() + '\n';
    _rbuf.sputn(s.data(), s.size());
}

void SandboxController::fireCommand(const char *buf, size_t len) {
    if (memchr(buf, '\n', len)) {
        int         ch;
        JSON        req;
        std::string str;
        std::string cmd;
        CommandArgs args;

        /* read one line */
        while ((ch = _wbuf.sbumpc()) != '\n') {
            if (ch == std::char_traits<char>::eof()) {
                break;
            } else {
                str.push_back(ch);
            }
        }

        /* parse the request */
        try {
            req  = JSON::parse(str);
            cmd  = req["cmd"].get<std::string>();
            args = req["args"].get<CommandArgs>();
        } catch (const JSON::exception &e) {
            XLOGF(ERR, "Cannot parse request, dropped. JSON Error: [{:d}] {:s}", e.id, e.what());
            throw FuseError(EINVAL);
        } catch (const std::exception &e) {
            XLOGF(ERR, "Cannot parse request, dropped. Error: {:s}", e.what());
            throw FuseError(EINVAL);
        }

        /* execute the request */
        try {
            executeCommand(cmd, args);
        } catch (const FuseError &) {
            throw;
        } catch (const std::exception &e) {
            XLOGF(ERR, "Cannot handle request. Error: {:s}", e.what());
            throw FuseError(EINVAL);
        }
    }
}

#define CALL_END()     throw FuseError(EINVAL)
#define CALL_CMD(name) do { if (cmd == #name) { execute_ ## name(args); return; } } while (false)

void SandboxController::executeCommand(const std::string &cmd, const CommandArgs &args) {
    CALL_CMD(LOAD);
    CALL_CMD(MOUNT);
    CALL_CMD(UNLOAD);
    CALL_CMD(UNMOUNT);
    CALL_END();
}

#undef CALL_CMD
#undef CALL_END

#pragma clang diagnostic push
#pragma ide diagnostic ignored "cert-err58-cpp"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

struct FileRecord {
    std::string    name;
    FileNode::Node node;
};

static constexpr int  TokenSize      = 32;
static constexpr int  TokenCount     = 62;
static constexpr char TokenCharset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

static folly::ThreadLocalPRNG                               prng;
static folly::ConcurrentHashMap<std::string, FileRecord>  * files  = new folly::ConcurrentHashMap<std::string, FileRecord>;
static folly::ConcurrentHashMap<std::string, std::string> * tokens = new folly::ConcurrentHashMap<std::string, std::string>;

static inline std::string nextToken() {
    std::string                        ret(TokenSize, 0);
    std::uniform_int_distribution<int> dist(0, TokenCount - 1);

    /* generate the result string */
    for (int i = 0; i < TokenSize; i++) {
        ret[i] = TokenCharset[dist(prng)];
    }

    /* all done */
    return std::move(ret);
}

static inline const std::string &validate(const std::string &s) {
    if (s.find('/') != std::string::npos || s.find('\0') != std::string::npos) {
        throw FuseError(EINVAL);
    } else {
        return s;
    }
}

void SandboxController::execute_LOAD(const std::string &file) {
    auto ret  = nextToken();
    auto iter = tokens->insert(file, ret);

    /* check for insertion */
    if (!iter.second) {
        throw FuseError(EEXIST);
    }

    /* load the file */
    files->insert(ret, FileRecord {
        .name = file,
        .node = FileNode::build(FileBackend(file)),
    });

    /* reply the file token */
    XLOGF(INFO, "Archive '{:s}' loaded as token '{:s}'", file, ret);
    reply({{"token", ret}});
}

void SandboxController::execute_MOUNT(const std::string &token, const std::string &alias) {
    auto end  = files->end();
    auto iter = files->find(token);

    /* check for loading status */
    if (iter == end) {
        throw FuseError(ENOENT);
    }

    /* mount the virtual directory */
    root()->add(validate(alias), iter->second.node->clone());
    XLOGF(INFO, "Virtual directory '{:s}' mounted from token '{:s}'", alias, token);
}

void SandboxController::execute_UNLOAD(const std::string &token) {
    auto end  = files->end();
    auto iter = files->find(token);

    /* check for loading status */
    if (iter == end) {
        throw FuseError(ENOENT);
    }

    /* get the file name */
    auto frec = iter->second;
    auto name = std::move(frec.name);

    /* erase from files and tokens */
    files->erase(iter);
    tokens->erase(name);
    XLOGF(INFO, "Archive '{:s}' of token '{:s}' has been unloaded.", name, token);
}

void SandboxController::execute_UNMOUNT(const std::string &alias) {
    root()->del(validate(alias));
    XLOGF(INFO, "Virtual directory '{:s}' has been unmounted.", alias);
}

#pragma clang diagnostic pop

template <typename T>
static inline void deleteAndNull(T *&p) {
    T *v = p;
    p = nullptr;
    delete v;
}

void SandboxController::end() {
    deleteAndNull(files);
    deleteAndNull(tokens);
}

FileNode::Node &SandboxController::root()  {
    static FileNode::Node v = std::make_shared<FileNode>();
    return v;
}