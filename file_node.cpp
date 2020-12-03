#include "utils.h"
#include "file_node.h"

static inline void settime(FileNode::Time *tm, const FileNode::Time &time) {
    switch (time.tv_nsec) {
        default         : *tm = time; break;
        case UTIME_NOW  : *tm = T::nowts(); break;
        case UTIME_OMIT : break;
    }
}

FileNode::Node FileNode::clone() const {
    auto buf = NodeBuffer();
    auto ret = std::make_shared<FileNode>();

    /* clone the node tree */
    for (auto &v : _nodes) {
        buf.emplace(v.first, v.second->clone());
    }

    /* initialize the result node */
    ret->_st    = _st;
    ret->_name  = _name;
    ret->_data  = _data.clone();
    ret->_nodes = std::move(buf);
    return ret;
}

void FileNode::rmdir(const std::string &path) {
    Node par;
    auto node = resolve(path, Missing::Error, &par);

    /* can only remove empty directories */
    if (!node->_nodes.empty()) {
        throw FuseError(ENOTEMPTY);
    } else if (!S_ISDIR(node->_st.st_mode)) {
        throw FuseError(ENOTDIR);
    } else {
        par->_nodes.erase(node->_name);
    }
}

void FileNode::mkdir(const std::string &path) {
    if (resolve(path, Missing::Empty) != nullptr) {
        throw FuseError(EEXIST);
    } else {
        resolve(path, Missing::Create);
    }
}

void FileNode::unlink(const std::string &path) {
    Node par;
    auto node = resolve(path, Missing::Error, &par);

    /* can only unlink files */
    if (S_ISDIR(node->_st.st_mode)) {
        throw FuseError(EISDIR);
    } else {
        par->_nodes.erase(node->_name);
    }
}

void FileNode::rename(const std::string &path, const std::string &dest) {
    Node par;
    auto node = resolve(path, Missing::Error, &par);

    /* erase from the old path, and attach to the new path */
    par->_nodes.erase(node->_name);
    resolve(dest, Missing::Create, nullptr, &node->_st, &node->_data, &node->_nodes);
}

void FileNode::access() {
    _st.st_atimespec = T::nowts();
}

void FileNode::resize(size_t size) {
    if (S_ISDIR(_st.st_mode)) {
        throw FuseError(EISDIR);
    } else {
        _data.resize(size);
        _st.st_size = _data.len();
        _st.st_mtimespec = T::nowts();
    }
}

void FileNode::utimens(const FileNode::Time &atime, const FileNode::Time &mtime) {
    settime(&_st.st_atimespec, atime);
    settime(&_st.st_mtimespec, mtime);
}

size_t FileNode::read(char *buf, size_t len, size_t off) {
    access();
    return _data.read(buf, len, off);
}

size_t FileNode::write(const char *buf, size_t len, size_t off) {
    _data.write(buf, len, off);
    _st.st_size = _data.len();
    _st.st_mtimespec = T::nowts();
    return len;
}

FileNode::Node FileNode::resolve(
    const std::string & path,
    Missing             ifnx,
    Node *              parent,
    Stat *              stat,
    ByteBuffer *        mbuf,
    NodeBuffer *        nodes
) {
    Node q = nullptr;
    Node p = shared_from_this();

    /* find in nodes */
    for (auto &v : str::split(path, "/").filterNot(&std::string::empty)) {
        auto end  = p->_nodes.end();
        auto iter = p->_nodes.find(v);

        /* check for node existance */
        if (iter != end) {
            q = std::move(p);
            p = iter->second;
            continue;
        }

        /* node does not exist, follow the selected strategy */
        switch (ifnx) {
            case Missing::Error  : throw FuseError(S_ISDIR(p->_st.st_mode) ? ENOENT : ENOTDIR);
            case Missing::Empty  : return nullptr;
            case Missing::Create : break;
        }

        /* can only create new nodes under directories */
        if (!S_ISDIR(p->_st.st_mode)) {
            throw FuseError(ENOTDIR);
        }

        /* create a new node */
        q = std::move(p);
        p = std::make_shared<FileNode>();

        /* set it's name and add to the node set */
        p->_name = v;
        q->_nodes.emplace(v, p);
    }

    /* set all the optional fields */
    if (stat   != nullptr) std::swap(*stat, p->_st);
    if (mbuf   != nullptr) std::swap(*mbuf, p->_data);
    if (nodes  != nullptr) std::swap(*nodes, p->_nodes);
    if (parent != nullptr) std::swap(*parent, q);
    return p;
}
