# SW/Makefile - Build host application for BFP kernel

#==============================================================================
# CONFIGURATION
#==============================================================================
CXX := g++
TARGET := bfp_host
SRC_DIR := src
INC_DIR := include
BUILD_DIR := build

SOURCES := $(SRC_DIR)/bfp_host.cpp
EXECUTABLE := $(BUILD_DIR)/$(TARGET)

#==============================================================================
# XRT CONFIGURATION
#==============================================================================
ifndef XILINX_XRT
$(error XILINX_XRT is not set. Please source /opt/xilinx/xrt/setup.sh)
endif

XRT_INCLUDE := $(XILINX_XRT)/include
XRT_LIB := $(XILINX_XRT)/lib

#==============================================================================
# COMPILER FLAGS
#==============================================================================
CXXFLAGS := -std=c++17 \
            -O3 \
            -Wall \
            -Wextra \
            -I$(XRT_INCLUDE) \
            -I$(INC_DIR)

LDFLAGS := -L$(XRT_LIB) \
           -lxrt_coreutil \
           -lpthread \
           -lrt \
           -lstdc++ \
           -Wl,-rpath,$(XRT_LIB)

#==============================================================================
# TARGETS
#==============================================================================
.PHONY: all clean run help

all: $(EXECUTABLE)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(EXECUTABLE): $(SOURCES) | $(BUILD_DIR)
	@echo "Compiling host application..."
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)
	@echo "[OK] Executable created: $@"

clean:
	rm -rf $(BUILD_DIR)
	@echo "[OK] Build directory cleaned"

run: $(EXECUTABLE)
	@echo "Running BFP host application..."
	@echo "Usage: $(EXECUTABLE) <xclbin_path>"
	@echo ""
	@echo "Example:"
	@echo "  $(EXECUTABLE) ../HW/build/bfp_kernel.xclbin"

help:
	@echo "Makefile for BFP Host Application"
	@echo ""
	@echo "Targets:"
	@echo "  all    - Build host executable (default)"
	@echo "  clean  - Remove build artifacts"
	@echo "  run    - Show usage information"
	@echo "  help   - Show this help"
	@echo ""
	@echo "Requirements:"
	@echo "  - XRT must be sourced: source /opt/xilinx/xrt/setup.sh"
	@echo "  - XCLBIN must be built: make -C ../HW"
	@echo ""
	@echo "Usage:"
	@echo "  make              # Build"
	@echo "  ./build/bfp_host ../HW/build/bfp_kernel.xclbin  # Run"

info:
	@echo "Build Configuration:"
	@echo "  Compiler:    $(CXX)"
	@echo "  Target:      $(TARGET)"
	@echo "  XRT Include: $(XRT_INCLUDE)"
	@echo "  XRT Lib:     $(XRT_LIB)"
	@echo "  Output:      $(EXECUTABLE)"
