// The options and the suppressions of the sanitizers, linked into every program of a build with
// RTYPE_SANITIZERS (cmake/Sanitizers.cmake). The runtimes call these functions at start: the tests, the
// programs run by hand and the ones the tests start all run the same way, without any environment. The
// variables ASAN_OPTIONS, LSAN_OPTIONS, UBSAN_OPTIONS and TSAN_OPTIONS still override the options.
//
// A suppression hides real bugs too: fix the code first. Each one is under a comment that says why it is not
// ours.

namespace
{

constexpr const char *LeakSuppressions =
    // The OpenGL driver of NVIDIA, closed source, keeps allocations made when it is loaded until the end
    "leak:libnvidia-glcore.so\n";

constexpr const char *ThreadSuppressions = "";

} // namespace

extern "C" {

// Stops at the first error, so that a test fails on it, with what is needed to understand it
const char *__asan_default_options()
{
    return "detect_leaks=1:detect_stack_use_after_return=1:strict_string_checks=1:halt_on_error=1";
}

const char *__ubsan_default_options()
{
    return "print_stacktrace=1:halt_on_error=1";
}

const char *__tsan_default_options()
{
    return "halt_on_error=1:second_deadlock_stack=1";
}

const char *__lsan_default_suppressions()
{
    return LeakSuppressions;
}

const char *__tsan_default_suppressions()
{
    return ThreadSuppressions;
}

} // extern "C"
