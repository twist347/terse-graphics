#include "tgx/core/app.h"

#include <type_traits>
#include <utility>

// The parts of an App come const from a const App, one method each. Checked
// at compile time: making an App needs a window.

static_assert(std::is_same_v<decltype(std::declval<tgx::App &>().window()), tgx::Window &>);
static_assert(std::is_same_v<decltype(std::declval<const tgx::App &>().window()), const tgx::Window &>);
static_assert(std::is_same_v<decltype(std::declval<tgx::App &>().device()), tgx::Device &>);
static_assert(std::is_same_v<decltype(std::declval<const tgx::App &>().device()), const tgx::Device &>);
static_assert(std::is_same_v<decltype(std::declval<tgx::App &>().canvas()), tgx::Canvas &>);
static_assert(std::is_same_v<decltype(std::declval<const tgx::App &>().canvas()), const tgx::Canvas &>);
static_assert(std::is_same_v<decltype(std::declval<tgx::App &>().audio()), tgx::Audio &>);
static_assert(std::is_same_v<decltype(std::declval<const tgx::App &>().audio()), const tgx::Audio &>);
