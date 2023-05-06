CHECK_INCLUDE_PATHS="-I. -Isrc/ -Itests_unit/"

# TODO (acerion) 2023.05.03: remove clang-analyzer-valist.Uninitialized
# from the list of exclusions. Right now it's just a workaround for
# clang-tidy bug. See https://bugs.llvm.org/show_bug.cgi?id=41311.

CHECK_CHECKS="*"
CHECK_CHECKS+=",-llvm-header-guard"
CHECK_CHECKS+=",-readability-braces-around-statements,-hicpp-braces-around-statements"
CHECK_CHECKS+=",-readability-else-after-return,-clang-diagnostic-deprecated-declarations"
CHECK_CHECKS+=",-readability-magic-numbers,-cppcoreguidelines-avoid-magic-numbers"
CHECK_CHECKS+=",-readability-redundant-control-flow"
CHECK_CHECKS+=",-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling"
CHECK_CHECKS+=",-llvmlibc-restrict-system-libc-headers"
CHECK_CHECKS+=",-cppcoreguidelines-avoid-non-const-global-variables"
CHECK_CHECKS+=",-clang-analyzer-valist.Uninitialized"

clang-tidy -checks=$CHECK_CHECKS src/*.c src/*.h tests_unit/*.c -- $CHECK_INCLUDE_PATHS
