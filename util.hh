#ifndef _UTIL_HH_
#define _UTIL_HH_

#include <stdio.h>
#include <stdarg.h>

struct NonCopyable {
    using self_type = NonCopyable;

    NonCopyable() = default;

    /* cannot copy */
    NonCopyable(const self_type&) = delete;
    self_type &operator=(const self_type&) = delete;

    /* move is ok */
    NonCopyable(self_type &&) = default;
    self_type &operator=(self_type &&) = default;
};

struct Singleton : public NonCopyable {
    using self_type = Singleton;

    Singleton() = default;

    /* cannot move */
    Singleton(self_type &&) = delete;
    self_type &operator=(self_type &&) = delete;
};

template<class Derived>
class DebugPolicyNone
{
public:
    Derived &derived() {
        return *static_cast<Derived*>(this);
    }

    Derived &set_debug_name() {
        return derived();
    }

    Derived &debug() {
        return derived();
    }

    void debug_printf() {}
};

template<class Derived>
class DebugPolicyOnDemand
{
public:
    Derived &derived() {
        return *static_cast<Derived*>(this);
    }

    Derived &set_debug_name(const char *name) {
        debug_name = name;
        return derived();
    }

    Derived &debug(bool v = true) {
        debug_show = v;
        return derived();
    }

    void debug_printf(const char *fmt, ...) {
        if ( !debug_show ) return;

        va_list vl;
        va_start(vl, fmt);
        if ( debug_name ) {
            fprintf(stderr, "[%s]:", debug_name);
        }
        vfprintf(stderr, fmt, vl);
        va_end(vl);
        fprintf(stderr, "\n");
    }
private:
    bool debug_show = false;
    const char *debug_name = nullptr;
};

#endif /* _UTIL_HH_ */
