#ifndef SANDBOX_FS_BYTE_BUFFER_H
#define SANDBOX_FS_BYTE_BUFFER_H

#include <atomic>
#include <cstdlib>
#include <utility>
#include <folly/Synchronized.h>

class ByteBuffer {
    struct Storage final {
        std::atomic_int64_t ref = 1;
        char *              mem = nullptr;
        size_t              len = 0;
        size_t              cap = 0;

    private:
        ~Storage() noexcept {
            free(mem);
        }

    public:
        Storage()                = default;
        Storage(Storage &&)      = delete;
        Storage(const Storage &) = delete;

    public:
        explicit Storage(Storage *src) noexcept : ref(1), mem(static_cast<char *>(malloc(src->len))), cap(src->len), len(src->len) {
            memcpy(mem, src->mem, len);
            release(src);
        }

    public:
        Storage &operator=(Storage &&)      = delete;
        Storage &operator=(const Storage &) = delete;

    public:
        inline void ensure(size_t size) noexcept {
            if (cap < size) {
                cap = (((size - 1) >> 4) + 1) << 4;
                mem = static_cast<char *>(realloc(mem, cap));
            }
        }

    public:
        inline Storage *retain() noexcept {
            ref++;
            return this;
        }

    public:
        inline void zfill(size_t size) noexcept {
            if (len < size) {
                ensure(size);
                memset(mem + len, 0, size - len);
            }
        }

    public:
        inline void resize(size_t size) noexcept {
            zfill(size);
            len = size;
        }

    public:
        inline size_t read(char *buf, size_t size, size_t start) const noexcept {
            if (len <= start) {
                return 0;
            } else {
                memcpy(buf, mem + start, (size = std::min(size, len - start)));
                return size;
            }
        }

    public:
        inline size_t write(const void *data, size_t size, size_t start) noexcept {
            ensure(size + start);
            memcpy(mem + start, data, size);
            len = std::max(len, size + start);
            return size;
        }

    public:
        static inline void release(Storage *p) noexcept {
            if (p != nullptr && --p->ref == 0) {
                delete p;
            }
        }
    };

private:
    folly::Synchronized<Storage *> _buf;

public:
    ~ByteBuffer() noexcept {
        Storage::release(_buf.unsafeGetUnlocked());
    }

public:
    ByteBuffer() : _buf(nullptr) {}
    ByteBuffer(const ByteBuffer &) noexcept = delete;
    ByteBuffer(ByteBuffer &&other) noexcept : ByteBuffer() { _buf.swap(other._buf); }

private:
    explicit ByteBuffer(Storage *buf) noexcept : _buf(buf) {}

public:
    ByteBuffer &operator=(const ByteBuffer &) noexcept = delete;
    ByteBuffer &operator=(ByteBuffer &&other) noexcept { _buf.swap(other._buf); return *this; }

public:
    [[nodiscard]] size_t len() const noexcept {
        auto rbuf = _buf.rlock();
        return *rbuf == nullptr ? 0 : (*rbuf)->len;
    }

public:
    [[nodiscard]] ByteBuffer clone() const noexcept {
        auto rbuf = _buf.rlock();
        return *rbuf == nullptr ? ByteBuffer() : ByteBuffer((*rbuf)->retain());
    }

public:
    void ensure(size_t size) noexcept {
        auto wbuf = _buf.wlock();
        detach(*wbuf)->ensure(size);
    }

public:
    void resize(size_t size) noexcept {
        auto wbuf = _buf.wlock();
        detach(*wbuf)->resize(size);
    }

public:
    size_t read(char *buf, size_t size, size_t start) noexcept {
        auto rbuf = _buf.rlock();
        return *rbuf == nullptr ? 0 : (*rbuf)->read(buf, size, start);
    }

public:
    size_t write(const void *data, size_t size, size_t start) noexcept {
        auto wbuf = _buf.wlock();
        return detach(*wbuf)->write(data, size, start);
    }

private:
    inline Storage *detach(Storage *&buf) {
        unshare(buf);
        return buf;
    }

private:
    static inline void unshare(Storage *&wbuf) {
        if (wbuf == nullptr) {
            wbuf = new Storage();
        } else if (wbuf->ref != 1) {
            wbuf = new Storage(wbuf);
        }
    }
};

#endif /* SANDBOX_FS_BYTE_BUFFER_H */
