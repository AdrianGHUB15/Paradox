_THIS       := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
_ROOT       := $(_THIS)

NAME        := Paradox
# Deferred, so it picks up SUFFIX from the OS detection below. OpenBench
# builds with `make EXE=Engine-ABCDEFGH`, which overrides this either way.
EXE          = $(NAME)$(SUFFIX)
SUFFIX      :=

WARNINGS    := -Wall -Wextra -Wno-unused-variable
CXXFLAGS    := -std=c++17 -O3 -funroll-loops -fomit-frame-pointer -flto \
               -fno-exceptions -DIS_64BIT -DNDEBUG $(WARNINGS)
LDFLAGS     :=
LIBS        :=
NATIVE      := -march=native

# -----------------------------------------
#   Compiler detection
# -----------------------------------------
ifneq ($(wildcard C:/msys64/mingw64/bin/clang++.exe),)
    CXX = C:/msys64/mingw64/bin/clang++.exe
else
    CXX = clang++
endif

COMPILER_DIR := $(dir $(CXX))
LLVM_PROFDATA := $(COMPILER_DIR)llvm-profdata

# -----------------------------------------
#   OS detection
# -----------------------------------------
ifeq ($(OS), Windows_NT)
    SUFFIX   := .exe
    CXXFLAGS += -static
    LDFLAGS  += -Wl,--stack,16777216
    MKDIR    := mkdir
    LIBS    := -lpthread
else
    MKDIR    := mkdir -p
    LIBS    := -pthread -lm
    uname_S  := $(shell uname -s)
endif

ifeq ($(uname_S), Darwin)
    NATIVE := -mcpu=apple-a14
    LIBS   :=
endif

# -----------------------------------------
#   Linker selection
#
#   lld is clang-only here: GCC's -flto objects are LLVM-unreadable, so
#   ld.lld fails the link with "undefined symbol: main". Probed rather than
#   assumed, since plenty of machines have clang but no lld installed.
# -----------------------------------------
ifneq ($(findstring clang,$(CXX)),)
    ifneq ($(findstring LLD,$(shell ld.lld --version 2>&1)),)
        LDFLAGS += -fuse-ld=lld
    endif
endif
ifeq ($(ARCH_DETECTED),)
    ifneq ($(findstring __aarch64__, $(PROPERTIES)),)
        ARCH_DETECTED = NEON
    endif
endif

# -----------------------------------------
#   Debug modes
# -----------------------------------------
ifeq ($(build), debug)
	CXXFLAGS := -std=gnu++2a -O0 -g3 -fno-omit-frame-pointer -DIS_64BIT $(WARNINGS)
endif
# -----------------------------------------
#   Architecture selection
#
#   An explicit build= picks its own target, so the host probe must not run
#   as well -- otherwise a native AVX512 machine building x86-64-avx2 gets
#   both flag sets and both -DUSE_* defines at once.
# -----------------------------------------
AVX2FLAGS   := -DUSE_AVX2 -DUSE_SIMD -mavx2 -mbmi
BMI2FLAGS   := -DUSE_AVX2 -DUSE_SIMD -mavx2 -mbmi -mbmi2
AVX512FLAGS := -DUSE_AVX512 -DUSE_SIMD -mavx512f -mavx512bw
NEONFLAGS   := -DUSE_NEON -flax-vector-conversions

ARCH_FLAGS  :=
ifeq ($(build), x86-64-avx2)
    NATIVE     := -march=bdver4 -mno-tbm -mno-sse4a -mno-bmi2
    ARCH_FLAGS := $(AVX2FLAGS)
else ifeq ($(build), x86-64-bmi2)
    NATIVE     := -march=haswell
    ARCH_FLAGS := $(BMI2FLAGS)
else ifeq ($(build), x86-64-avx512)
    NATIVE     := -march=x86-64-v4 -mtune=znver4
    ARCH_FLAGS := $(AVX512FLAGS)
else
    PROPERTIES := $(shell echo | $(CXX) $(NATIVE) -E -dM -)

  ifneq ($(findstring __AVX512F__, $(PROPERTIES)),)
        ifneq ($(findstring __AVX512BW__, $(PROPERTIES)),)
            ARCH_FLAGS := $(AVX512FLAGS)
        endif
    endif
    ifeq ($(ARCH_FLAGS),)
        ifneq ($(findstring __BMI2__, $(PROPERTIES)),)
            ARCH_FLAGS := $(BMI2FLAGS)
        endif
    endif
    ifeq ($(ARCH_FLAGS),)
        ifneq ($(findstring __AVX2__, $(PROPERTIES)),)
            ARCH_FLAGS := $(AVX2FLAGS)
        endif
    endif
    ifeq ($(ARCH_FLAGS),)
        ifneq ($(findstring __aarch64__, $(PROPERTIES)),)
            ARCH_FLAGS := $(NEONFLAGS)
        endif
    endif
endif

CXXFLAGS += $(ARCH_FLAGS)

# -----------------------------------------
#   Sources
# -----------------------------------------
OBJDIR  := .tmp
SOURCES := $(wildcard *.cpp)
OBJECTS := $(patsubst %.cpp,$(OBJDIR)/%.o,$(SOURCES))
DEPENDS := $(OBJECTS:.o=.d)

# -----------------------------------------
#   Rules
# -----------------------------------------
default: build

build: $(EXE)

clean:
	@rm -rf $(OBJDIR) *.o *.d $(EXE) \
		$(NAME)-avx2$(SUFFIX) $(NAME)-bmi2$(SUFFIX) $(NAME)-avx512$(SUFFIX)

$(EXE): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(NATIVE) -o $@ $(OBJECTS) $(LIBS) $(LDFLAGS)

$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(NATIVE) -MMD -MP -c $< -o $@

$(OBJDIR):
	$(MKDIR) "$(OBJDIR)"

# -----------------------------------------
#   Release targets
# -----------------------------------------
release-avx2:
	@$(MAKE) clean
	@$(MAKE) build build=x86-64-avx2
	@mv $(EXE) $(NAME)-avx2$(SUFFIX)

release-bmi2:
	@$(MAKE) clean
	@$(MAKE) build build=x86-64-bmi2
	@mv $(EXE) $(NAME)-bmi2$(SUFFIX)

release-avx512:
	@$(MAKE) clean
	@$(MAKE) build build=x86-64-avx512
	@mv $(EXE) $(NAME)-avx512$(SUFFIX)

release: release-avx2 release-bmi2 release-avx512

-include $(DEPENDS)

.PHONY: default clean build release release-avx2 release-bmi2 release-avx512
