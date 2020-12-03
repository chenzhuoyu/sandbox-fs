#ifndef SANDBOX_FS_UTILS_H
#define SANDBOX_FS_UTILS_H

#include <string>
#include <utility>
#include <iterator>
#include <functional>
#include <type_traits>

template <typename T>
using Predicate = std::function<bool (const T &)>;

namespace fn {
template <typename T>
Predicate<T> not_fn(Predicate<T> &fn) {
    return [pred = std::move(fn)](const std::string &v) {
        return !pred(v);
    };
}

template <typename T>
Predicate<T> and_fn(Predicate<T> &f0, Predicate<T> &f1) {
    return [p0 = std::move(f0), p1 = std::move(f1)](const std::string &v) {
        return p0(v) && p1(v);
    };
}

template <typename T>
Predicate<T> chain_fn(Predicate<T> &prev, Predicate<T> &next) {
    if (prev == nullptr) {
        return std::move(next);
    } else {
        return and_fn(prev, next);
    }
}
}

namespace str {
class split {
    std::string            _d;
    std::string            _s;
    Predicate<std::string> _f;

public:
    class iterator : public std::forward_iterator_tag {
        size_t _n;
        size_t _p;
        size_t _q;

    private:
        std::string            _s;
        std::string            _d;
        std::string            _v;
        Predicate<std::string> _f;

    private:
        friend class split;

    private:
        explicit iterator(
            std::string &&            s = "",
            std::string &&            d = "",
            Predicate<std::string> && f = nullptr,
            size_t                    p = std::string::npos,
            size_t                    q = std::string::npos,
            size_t                    n = 0
        ) : _n(n),
            _p(p),
            _q(q),
            _s(std::move(s)),
            _d(std::move(d)),
            _f(std::move(f))
            { next(); }

    public:
        iterator &operator++() {
            next();
            return *this;
        }

    public:
        const std::string &operator*() const { return _v; }
        const std::string *operator->() const { return &_v; }

    public:
        bool operator==(const iterator &v) const { return _p == v._p; }
        bool operator!=(const iterator &v) const { return _p != v._p; }

    private:
        inline void next() {
            do {
                if (_q == std::string::npos) {
                    _v = "";
                    _p = std::string::npos;
                    break;
                } else {
                    _p = _q + dist();
                    _q = _s.find(_d, _p);
                    _v = substr(_s, _p, _q);
                }
            } while (_f != nullptr && !_f(_v));
        }

    private:
        inline size_t dist() {
            return _d.size() * (_n++ != 0);
        }

    private:
        static std::string substr(const std::string &s, size_t p, size_t q) {
            if (p == std::string::npos) {
                return "";
            } else if (q == std::string::npos) {
                return s.substr(p);
            } else {
                return s.substr(p, q - p);
            }
        }
    };

public:
    split(std::string val, std::string delim) :
        _s(std::move(val)),
        _d(std::move(delim)) {}

public:
    split(const split &)            = delete;
    split &operator=(split &&)      = delete;
    split &operator=(const split &) = delete;

private:
    split(split &&other) noexcept {
        std::swap(_s, other._s);
        std::swap(_d, other._d);
        std::swap(_f, other._f);
    }

public:
    [[nodiscard]] iterator end()   noexcept { return iterator(); }
    [[nodiscard]] iterator begin() noexcept { return iterator(std::move(_s), std::move(_d), std::move(_f), 0, 0); }

public:
    split filter(Predicate<std::string> pred) {
        _f = fn::chain_fn(_f, pred);
        return std::move(*this);
    }

public:
    split filterNot(Predicate<std::string> pred) {
        return filter(fn::not_fn(pred));
    }
};
}

#endif /* SANDBOX_FS_UTILS_H */
