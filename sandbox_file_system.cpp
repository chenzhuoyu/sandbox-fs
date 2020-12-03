#include <folly/logging/xlog.h>
#include "sandbox_file_system.h"

namespace {
struct FuseSession {
    struct fuse *         fs  = nullptr;
    struct fuse_session * ss  = nullptr;

public:
    ~FuseSession() {
        if (fs != nullptr) {
            fuse_destroy(fs);
        }
    }

public:
    bool init(struct fuse_chan *ch, struct fuse_args *args, struct fuse_operations *ops, void *data) {
        if ((fs = fuse_new(ch, args, ops, sizeof(struct fuse_operations), data)) == nullptr) {
            return false;
        } else {
            ss = fuse_get_session(fs);
            return true;
        }
    }
};

struct FuseChannel {
    const char *       mp;
    struct fuse_chan * ch;

public:
    ~FuseChannel() { fuse_unmount(mp, ch); }
    FuseChannel(const char *mp, struct fuse_args *args) : mp(mp) {
        if ((ch = fuse_mount(mp, args)) == nullptr) {
            throw FuseError();
        }
    }
};

struct FuseSignals {
    struct fuse_session *fs = nullptr;

public:
    ~FuseSignals() {
        if (fs != nullptr) {
            fuse_remove_signal_handlers(fs);
        }
    }

public:
    bool init(struct fuse_session *ss) {
        if (fuse_set_signal_handlers(ss) != 0) {
            return false;
        } else {
            fs = ss;
            return true;
        }
    }
};
}

SandboxFileSystem::~SandboxFileSystem() {
    XLOG(INFO, "Shutting down ...");
    _root.reset();
}

SandboxFileSystem::SandboxFileSystem(FileNode::Node root, ControlInterface *iface) : _ctrl(iface) {
    _root = std::move(root);
    XLOG(INFO, "Sandbox initialized successfully.");
}

void SandboxFileSystem::start(const std::string &mount, const std::string &options) {
    const char *opts[] = {
        "sandbox_fs",
        nullptr,
        nullptr,
    };

    /* fuse arguments */
    struct fuse_args args = {
        .argc = 1,
        .argv = const_cast<char **>(opts),
    };

    /* fuse operations */
    static struct fuse_operations ops = {
        .getattr   = fs_getattr,
        .mkdir     = fs_mkdir,
        .unlink    = fs_unlink,
        .rmdir     = fs_rmdir,
        .rename    = fs_rename,
        .truncate  = fs_truncate,
        .open      = fs_open,
        .read      = fs_read,
        .write     = fs_write,
        .release   = fs_release,
        .readdir   = fs_readdir,
        .access    = fs_access,
        .create    = fs_create,
        .ftruncate = fs_ftruncate,
        .fgetattr  = fs_fgetattr,
        .utimens   = fs_utimens,
    };

    /* add mount options if any */
    if (!options.empty()) {
        opts[1]   = "-o";
        opts[2]   = options.c_str();
        args.argc = 3;
    }

    /* initialise the VFS */
    FuseSession fs;
    FuseSignals ss;
    FuseChannel ch(mount.c_str(), &args);

    /* start the FUSE file system */
    if (!fs.init(ch.ch, &args, &ops, this) || !ss.init(fs.ss) || fuse_loop_mt(fs.fs) != 0) {
        throw FuseError();
    }
}

/** File-System Event Handlers **/

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

namespace {
class OpenedFile : public SandboxFile {
    FileNode::Node _node;

public:
    OpenedFile(int mode, FileNode::Node node) : SandboxFile(mode), _node(std::move(node)) {}

public:
    void do_resize  (size_t size)          override { _node->resize(size); }
    void do_getstat (FileNode::Stat *stat) override { *stat = _node->stat(); }

public:
    ssize_t do_read  (char *buf, size_t len, size_t off)       override { return _node->read(buf, len, off); }
    ssize_t do_write (const char *buf, size_t len, size_t off) override { return _node->write(buf, len, off); }
};

inline bool isControlFile(const char *path, ControlInterface *iface) {
    return *path == '/' &&
           iface != nullptr &&
           strcmp(path + 1, iface->name()) == 0;
}
}

void SandboxFileSystem::do_open(const char *path, struct fuse_file_info *fi) {
    if (isControlFile(path, _ctrl)) {
        fi->fh        = reinterpret_cast<uint64_t>(_ctrl->open(fi->flags));
        fi->direct_io = true;
    } else {
        fi->fh        = reinterpret_cast<uint64_t>(new OpenedFile(fi->flags, _root->get(path, (fi->flags & O_CREAT) != 0)));
        fi->direct_io = false;
    }
}

long SandboxFileSystem::do_read(const char *, char *buf, size_t size, off_t off, struct fuse_file_info *fi) {
    if (fi->fh == 0)  {
        throw FuseError(EINVAL);
    } else {
        return reinterpret_cast<SandboxFile *>(fi->fh)->read(buf, size, off);
    }
}

void SandboxFileSystem::do_rmdir(const char *path) {
    if (!isControlFile(path, _ctrl)) {
        _root->rmdir(path);
    } else {
        throw FuseError(ENOTDIR);
    }
}

void SandboxFileSystem::do_mkdir(const char *path, mode_t mode) {
    if (!S_ISDIR(mode)) {
        throw FuseError(EINVAL);
    } else if (isControlFile(path, _ctrl)) {
        throw FuseError(EEXIST);
    } else {
        _root->mkdir(path);
    }
}

long SandboxFileSystem::do_write(const char *, const char *buf, size_t size, off_t off, struct fuse_file_info *fi) {
    if (fi->fh == 0)  {
        throw FuseError(EINVAL);
    } else {
        return reinterpret_cast<SandboxFile *>(fi->fh)->write(buf, size, off);
    }
}

void SandboxFileSystem::do_create(const char *path, mode_t, struct fuse_file_info *fi) {
    do_open(path, fi);
}

void SandboxFileSystem::do_unlink(const char *path) {
    if (!isControlFile(path, _ctrl)) {
        _root->unlink(path);
    } else {
        throw FuseError(EPERM);
    }
}

void SandboxFileSystem::do_access(const char *path, int) {
    if (!isControlFile(path, _ctrl)) {
        _root->get(path)->access();
    }
}

void SandboxFileSystem::do_rename(const char *path, const char *dest) {
    if (isControlFile(path, _ctrl) || isControlFile(dest, _ctrl)) {
        throw FuseError(EPERM);
    } else {
        _root->rename(path, dest);
    }
}

void SandboxFileSystem::do_getattr(const char *path, struct stat *stat) {
    if (isControlFile(path, _ctrl)) {
        *stat = _ctrl->stat();
    } else {
        *stat = _root->get(path)->stat();
    }
}

void SandboxFileSystem::do_utimens(const char *path, const struct timespec *tv) {
    if (tv != nullptr) {
        if (isControlFile(path, _ctrl)) {
            throw FuseError(EPERM);
        } else {
            _root->get(path)->utimens(tv[0], tv[1]);
        }
    }
}

void SandboxFileSystem::do_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t, struct fuse_file_info *) {
    static FileNode::Stat st = {
        .st_mode      = S_IFDIR | 0755,
        .st_nlink     = 1,
        .st_uid       = geteuid(),
        .st_gid       = getegid(),
        .st_atimespec = T::nowts(),
        .st_size      = 0,
    };

    /* current & super directory */
    filler(buf, ".", &st, 0);
    filler(buf, "..", &st, 0);

    /* add the control file if listing the mount root */
    if (_ctrl != nullptr && strcmp(path, "/") == 0) {
        filler(buf, _ctrl->name(), &_ctrl->stat(), 0);
    }

    /* add every directory entry */
    for (auto &v : _root->get(path)->nodes()) {
        filler(buf, v.first.c_str(), &v.second->stat(), 0);
    }
}

void SandboxFileSystem::do_release(const char *, struct fuse_file_info *fi) {
    if (fi->fh == 0)  {
        throw FuseError(EINVAL);
    } else {
        delete reinterpret_cast<SandboxFile *>(fi->fh);
    }
}

void SandboxFileSystem::do_truncate(const char *path, off_t off) {
    if (isControlFile(path, _ctrl)) {
        throw FuseError(EPERM);
    } else {
        _root->get(path)->resize(off);
    }
}

void SandboxFileSystem::do_fgetattr(const char *path, struct stat *stat, struct fuse_file_info *fi) {
    if (fi->fh == 0) {
        do_getattr(path, stat);
    } else {
        reinterpret_cast<SandboxFile *>(fi->fh)->getstat(stat);
    }
}

void SandboxFileSystem::do_ftruncate(const char *path, off_t off, struct fuse_file_info *fi) {
    if (fi->fh == 0) {
        do_truncate(path, off);
    } else {
        reinterpret_cast<SandboxFile *>(fi->fh)->resize(off);
    }
}

/** File-System Proxy Stubs **/

#define FS_V(name, formal, actual)                                                          \
    int SandboxFileSystem::fs_ ## name formal {                                             \
        try {                                                                               \
            ((SandboxFileSystem *)fuse_get_context()->private_data)->do_ ## name actual;    \
            return 0;                                                                       \
        } catch (const FuseError &e) {                                                      \
            return -e.code();                                                               \
        }                                                                                   \
    }

#define FS_R(name, formal, actual)                                                              \
    int SandboxFileSystem::fs_ ## name formal {                                                 \
        try {                                                                                   \
            return ((SandboxFileSystem *)fuse_get_context()->private_data)->do_ ## name actual; \
        } catch (const FuseError &e) {                                                          \
            return -e.code();                                                                   \
        }                                                                                       \
    }

#define PATH const char *path
#define INFO struct fuse_file_info *fi

FS_V(open      , (PATH, INFO)                                                     , (path, fi))
FS_R(read      , (PATH, char *buf, size_t size, off_t off, INFO)                  , (path, buf, size, off, fi))
FS_V(rmdir     , (PATH)                                                           , (path))
FS_V(mkdir     , (PATH, mode_t mode)                                              , (path, mode))
FS_R(write     , (PATH, const char *buf, size_t size, off_t off, INFO)            , (path, buf, size, off, fi))
FS_V(create    , (PATH, mode_t mode, INFO)                                        , (path, mode, fi))
FS_V(unlink    , (PATH)                                                           , (path))
FS_V(access    , (PATH, int mode)                                                 , (path, mode))
FS_V(rename    , (PATH, const char *dest)                                         , (path, dest))
FS_V(getattr   , (PATH, struct stat *stat)                                        , (path, stat))
FS_V(utimens   , (PATH, const struct timespec *tv)                                , (path, tv))
FS_V(readdir   , (PATH, void *buf, fuse_fill_dir_t fn, off_t off, INFO)           , (path, buf, fn, off, fi))
FS_V(release   , (PATH, INFO)                                                     , (path, fi))
FS_V(truncate  , (PATH, off_t off)                                                , (path, off))
FS_V(fgetattr  , (PATH, struct stat *stat, INFO)                                  , (path, stat, fi))
FS_V(ftruncate , (PATH, off_t off, INFO)                                          , (path, off, fi))

#undef FS_V
#undef FS_R
#undef PATH
#undef INFO

#pragma clang diagnostic pop
