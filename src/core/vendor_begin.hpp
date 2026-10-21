// No include guard: opens vendored code compiled into a tgx file, until
// core/vendor_end.hpp.
//
// MSVC's code generation warns about vendored code past /external:W0, and a
// warning level does not reach it: all its warnings on at /W4 are turned off
// by number (uninitialized, unreachable, not inlined, division by zero,
// constant overflow, buffer overrun).
#if defined(_MSC_VER)
#pragma warning(push, 0)
#pragma warning(disable: 4700 4701 4702 4703 4714 4723 4724 4756 4789)
#endif
