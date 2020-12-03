#ifndef SANDBOX_FS_FILE_NODE_H
#define SANDBOX_FS_FILE_NODE_H

#include <memory>
#include <string>
#include <folly/logging/xlog.h>
#include <folly/concurrency/ConcurrentHashMap.h>

#include <utime.h>
#include <unistd.h>
#include <sys/stat.h>

#include "timer.h"
#include "backend.h"
#include "fuse_error.h"
#include "byte_buffer.h"

struct FileNode : public std::enable_shared_from_this<FileNode> {
    typedef std::string                                 Name;
    typedef struct stat                                 Stat;
    typedef struct timespec                             Time;
    typedef std::shared_ptr<FileNode>                   Node;
    typedef folly::ConcurrentHashMap<std::string, Node> NodeBuffer;

private:
    enum class Missing {
        Error,
        Empty,
        Create,
    };

private:
    Stat        _st;
    Name        _name;
    ByteBuffer  _data;
    NodeBuffer  _nodes;

public:
   ~FileNode() { _nodes.clear(); }
    FileNode() : _st() { setstat(&_st, S_IFDIR | 0755); }

public:
    FileNode(FileNode &&) = delete;
    FileNode(const FileNode &) = delete;

public:
    FileNode &operator=(FileNode &&) = delete;
    FileNode &operator=(const FileNode &) = delete;

public:
    [[nodiscard]] const Stat &       stat()  const { return _st; }
    [[nodiscard]] const NodeBuffer & nodes() const { return _nodes; }
    [[nodiscard]] Node               clone() const;

public:
    inline void del(const std::string &name) {
        if (_nodes.erase(name) == 0) {
            throw FuseError(ENOENT);
        }
    }

public:
    inline void add(const std::string &name, Node &&node) {
        if (!_nodes.try_emplace(name, std::move(node)).second) {
            throw FuseError(EEXIST);
        }
    }

public:
    inline Node get(const std::string &path, bool autoCreate = false) {
        Stat st;
        auto node = resolve(path, autoCreate ? Missing::Empty : Missing::Error);

        /* check for existing node */
        if (node != nullptr) {
            return node;
        }

        /* create a new one if needed */
        setstat(&st, S_IFREG | 0644);
        return resolve(path, Missing::Create, nullptr, &st);
    }

public:
    void rmdir(const std::string &path);
    void mkdir(const std::string &path);
    void unlink(const std::string &path);
    void rename(const std::string &path, const std::string &dest);

public:
    void access();
    void resize(size_t size);
    void utimens(const Time &atime, const Time &mtime);

public:
    size_t read(char *buf, size_t len, size_t off);
    size_t write(const char *buf, size_t len, size_t off);

private:
    Node resolve(
        const std::string & path,
        Missing             ifnx   = Missing::Error,
        Node *              parent = nullptr,
        Stat *              stat   = nullptr,
        ByteBuffer *        mbuf   = nullptr,
        NodeBuffer *        nodes  = nullptr
    );

public:
    static Node build(const Backend &be) {
        auto now = T::now();
        auto ret = std::make_shared<FileNode>();

        /* add every file */
        be.foreach([&](const std::string &name, Stat stat, ByteBuffer data) {
            XLOG(INFO, "Loading file " + name); // NOLINT(bugprone-lambda-function-name)
            ret->resolve(name, Missing::Create, nullptr, &stat, &data, nullptr);
        });

        /* log the time cost */
        XLOGF(INFO, "Storage initialized successfully in {:.3f}s.", (double)(T::now() - now) * 1e-9);
        return ret;
    }

public:
    static void setstat(Stat *st, mode_t mode) {
        st->st_mode      = mode;
        st->st_nlink     = 1;
        st->st_uid       = geteuid();
        st->st_gid       = getegid();
        st->st_size      = 0;
        st->st_atimespec = T::nowts();
        st->st_ctimespec = st->st_atimespec;
        st->st_mtimespec = st->st_ctimespec;
    }
};

#endif /* SANDBOX_FS_FILE_NODE_H */
