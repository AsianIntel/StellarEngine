export module stellar.core;

export [[noreturn]] inline void unreachable() {
#if defined(_MSC_VER) && !defined(__clang__)
    __assume(false);
#else
    __builtin_unreachable();
#endif
}