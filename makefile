_THIS       := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
_ROOT       := $(_THIS)

NAME        := Paradox
TARGET      := $(NAME)
SUFFIX      :=
EXE         := foo$(SUFFIX)

WARNINGS    = -Wall -Wextra -Wno-unused-variable
CXXFLAGS    := -O3 -funroll-loops -fomit-frame-pointer -flto -fno-exceptions -DIS_64BIT -DNDEBUG $(WARNINGS)
LDFLAGS     := -fuse-ld=lld
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
    FLAGS    := -lpthread
else
    MKDIR    := mkdir -p
    FLAGS    := -pthread -lm
    uname_S  := $(shell uname -s)
endif

ifeq ($(uname_S), Darwin)
    NATIVE = -mcpu=apple-a14
    FLAGS  =
endif

# -----------------------------------------
#   Architecture auto-detect
# -----------------------------------------
AVX2FLAGS   = -DUSE_AVX2 -DUSE_SIMD -mavx2 -mbmi
BMI2FLAGS   = -DUSE_AVX2 -DUSE_SIMD -mavx2 -mbmi -mbmi2
AVX512FLAGS = -DUSE_AVX512 -DUSE_SIMD -mavx512f -mavx512bw
NEONFLAGS   = -DUSE_NEON -flax-vector-conversions

ARCH_DETECTED =
PROPERTIES = $(shell echo | $(CXX) $(NATIVE) -E -dM -)

ifneq ($(findstring __AVX512F__, $(PROPERTIES)),)
    ifneq ($(findstring __AVX512BW__, $(PROPERTIES)),)
        ARCH_DETECTED = AVX512
    endif
endif
ifeq ($(ARCH_DETECTED),)
    ifneq ($(findstring __BMI2__, $(PROPERTIES)),)
        ARCH_DETECTED = BMI2
    endif
endif
ifeq ($(ARCH_DETECTED),)
    ifneq ($(findstring __AVX2__, $(PROPERTIES)),)
        ARCH_DETECTED = AVX2
    endif
endif
ifeq ($(ARCH_DETECTED),)
    ifneq ($(findstring __aarch64__, $(PROPERTIES)),)
        ARCH_DETECTED = NEON
    endif
endif

ifeq ($(ARCH_DETECTED), AVX512)
    CXXFLAGS += $(AVX512FLAGS)
endif
ifeq ($(ARCH_DETECTED), BMI2)
    CXXFLAGS += $(BMI2FLAGS)
endif
ifeq ($(ARCH_DETECTED), AVX2)
    CXXFLAGS += $(AVX2FLAGS)
endif
ifeq ($(ARCH_DETECTED), NEON)
    CXXFLAGS += $(NEONFLAGS)
endif

# -----------------------------------------
#   Build modes
# -----------------------------------------
ifeq ($(build), debug)
    CXXFLAGS = -O0 -g3 -std=gnu++2a -fno-omit-frame-pointer
endif

ifeq ($(build), x86-64-avx2)
    NATIVE = -march=bdver4 -mno-tbm -mno-sse4a -mno-bmi2
    CXXFLAGS += $(AVX2FLAGS)
endif

ifeq ($(build), x86-64-bmi2)
    NATIVE = -march=haswell
    CXXFLAGS += $(BMI2FLAGS)
endif

ifeq ($(build), x86-64-avx512)
    NATIVE = -march=x86-64-v4 -mtune=znver4
    CXXFLAGS += $(AVX512FLAGS)
endif

# -----------------------------------------
#   Sources
# -----------------------------------------
OBJDIR  := .tmp
SOURCES := $(wildcard *.cpp)
OBJECTS := $(patsubst %.cpp,$(OBJDIR)/%.o,$(SOURCES))

# -----------------------------------------
#   Rules
# -----------------------------------------
default: build

build: $(TARGET)

clean:
	@rm -rf $(OBJDIR) *.o *.d $(TARGET) *.exe

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(NATIVE) -o $(EXE) $(OBJECTS) $(FLAGS) $(LDFLAGS)

$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(NATIVE) -MMD -MP -c $< -o $@ $(FLAGS)

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

.PHONY: clean build release release-avx2 release-bmi2 release-avx512
