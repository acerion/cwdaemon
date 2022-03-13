CHECK_INCLUDE_PATHS="-I. -I.."

CHECK_CHECKS="*"
CHECK_CHECKS+=",-llvm-header-guard"
CHECK_CHECKS+=",-readability-braces-around-statements,-hicpp-braces-around-statements"
CHECK_CHECKS+=",-readability-else-after-return,-clang-diagnostic-deprecated-declarations"
CHECK_CHECKS+=",-readability-magic-numbers,-cppcoreguidelines-avoid-magic-numbers"
CHECK_CHECKS+=",-readability-redundant-control-flow"
CHECK_CHECKS+=",-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling"
CHECK_CHECKS+=",-llvmlibc-restrict-system-libc-headers"
CHECK_CHECKS+=",-cppcoreguidelines-avoid-non-const-global-variables"

clang-tidy-11 -checks=$CHECK_CHECKS                         \
	      library/*.c library/*.h                       \
	      test_001_basic_process_control/*.c            \
	      test_002_reset_register_callback/*.c          \
	      test_003_cwdevice_tty_line_options/           \
	      -- $CHECK_INCLUDE_PATHS
