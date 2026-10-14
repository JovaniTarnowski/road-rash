# Plan 22 Task 2: locates Homebrew LLVM for the fuzzing, TSan and clang-tidy
# trees. Apple clang ships neither libFuzzer nor clang-tidy, so these builds
# require the Homebrew compiler.
set(RR_LLVM_ROOT "/opt/homebrew/opt/llvm" CACHE PATH "Homebrew LLVM prefix")
set(RR_LLVM_CC "${RR_LLVM_ROOT}/bin/clang")
set(RR_LLVM_CXX "${RR_LLVM_ROOT}/bin/clang++")
if(NOT EXISTS "${RR_LLVM_CXX}")
    message(FATAL_ERROR "Homebrew LLVM clang++ not found at ${RR_LLVM_CXX}; install it with `brew install llvm` (ask the owner before installing anything).")
endif()
