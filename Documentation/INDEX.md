# 📚 BFP Alveo U55C - File Index

Quick reference to know what each file does and when to use it.

---

## 📖 Documentation Files (Start Here)

### README.md
**Purpose:** Project overview and quick start  
**Read when:** First time using the project  
**Contains:** Basic info, quick commands, project structure  
**Time to read:** 2 minutes

### GUIDE.md ⭐ **MAIN DOCUMENTATION**
**Purpose:** Complete guide with everything you need  
**Read when:** Building from scratch, troubleshooting, optimizing  
**Contains:**
- Detailed build instructions
- PYNQ integration steps
- Troubleshooting solutions
- Performance tuning
- Advanced usage

**Time to read:** 20 minutes  
**Sections:**
1. Overview
2. Hardware Configuration
3. Prerequisites
4. Quick Start
5. Project Structure
6. Detailed Build Process
7. Running Tests
8. PYNQ Integration
9. Troubleshooting
10. Advanced Usage

### CHEATSHEET.md
**Purpose:** Quick command reference  
**Use when:** You know what to do, just forgot the command  
**Contains:** Common commands, quick fixes, one-liners  
**Time to read:** 5 minutes

### CHECKLIST.md
**Purpose:** Step-by-step verification  
**Use when:** First build, or verifying everything works  
**Contains:** Checkboxes for each step, success criteria  
**Time to read:** Print and use as guide

### THIS FILE (INDEX.md)
**Purpose:** Understand project structure  
**Use when:** Navigating project, looking for specific file  

---

## 🔧 Build System

### Makefile (Root)
**Location:** `./Makefile`  
**Purpose:** Main build orchestration  
**Use:** `make`, `make hw`, `make sw`, `make test`  
**Contains:**
- `all` - Build everything
- `hw` - Build hardware kernel
- `sw` - Build software host
- `flash` - Program FPGA
- `test` - Run tests
- `clean` - Clean builds
- `help` - Show help

**Edit when:** Changing global build options

### HW/Makefile
**Location:** `./HW/Makefile`  
**Purpose:** Build Vitis kernel  
**Use:** `make -C HW`  
**Contains:**
- Platform configuration (Alveo U55C)
- Vitis compiler flags
- Link options
- Target selection (hw vs hw_emu)

**Edit when:**
- Changing clock frequency
- Modifying optimization level
- Switching between hw/hw_emu
- Adjusting resource limits

**Key variables:**
```makefile
PLATFORM = xilinx_u55c_gen3x16_xdma_3_202210_1
DEVICE = xcu55c-fsvh2892-2L-e
TARGET = hw  # or hw_emu
--kernel_frequency 250  # Clock in MHz
```

### SW/Makefile
**Location:** `./SW/Makefile`  
**Purpose:** Build XRT host application  
**Use:** `make -C SW`  
**Contains:**
- XRT linking configuration
- Compiler flags
- Include paths

**Edit when:**
- Adding new source files
- Changing compiler options

---

## 💻 Hardware (Kernel) Files

### HW/src/bfp_kernel.cpp ⭐ **MAIN KERNEL**
**Purpose:** Vitis kernel entry point  
**Lines:** ~200  
**Contains:**
- Kernel interface (`extern "C" void bfp_kernel(...)`)
- XRT pragmas (m_axi, s_axilite)
- Operation dispatcher (switch on opcode)
- Buffer management

**Edit when:**
- Changing precision (WE, WM)
- Modifying block size (N)
- Adding new operations
- Tuning interface parameters

**Key sections:**
```cpp
#define WE 5   // Exponent bits
#define WM 7   // Mantissa bits
#define N 16   // Block size

// Operation codes
OP_ENCODE = 0
OP_DECODE = 1
OP_ADD = 2
// etc...
```

### HW/src/bfp_hls.h
**Purpose:** BFP core library (from your original code)  
**Lines:** ~200  
**Contains:**
- `BFP_bias<WE, WM>` - Configuration template
- `helper_rne()` - Round to nearest even
- `BFP_Global<>` - Block structure
- `encode_block()` - FP32 → BFP
- `decode_block()` - BFP → FP32

**Edit when:**
- Modifying encoding algorithm
- Changing rounding behavior
- Optimizing core operations

**DO NOT edit** unless you know what you're doing!

### HW/src/bfp_ops_hls.h
**Purpose:** BFP arithmetic operations (from your original code)  
**Lines:** ~400  
**Contains:**
- `add_blocks()` - BFP addition
- `sub_blocks()` - BFP subtraction
- `mul_blocks()` - BFP multiplication
- `div_blocks()` - BFP division
- `rcp_blocks()` - BFP reciprocal

**Edit when:**
- Optimizing arithmetic operations
- Adding new operations
- Modifying precision handling

**DO NOT edit** unless you know what you're doing!

---

## 🖥️ Software (Host) Files

### SW/src/bfp_host.cpp ⭐ **MAIN HOST**
**Purpose:** XRT host application  
**Lines:** ~450  
**Contains:**
- XRT device initialization
- Buffer allocation
- Kernel execution
- Test suite (ENCODE → OPS → DECODE)
- Error calculation

**Edit when:**
- Changing test vectors
- Adding new tests
- Modifying buffer sizes
- Implementing your application

**Key sections:**
```cpp
// Initialize device
auto device = xrt::device(device_index);
auto uuid = device.load_xclbin(xclbin_path);
auto kernel = xrt::kernel(device, uuid, "bfp_kernel");

// Allocate buffers
auto bo_in = xrt::bo(device, size, kernel.group_id(arg_id));

// Run kernel
auto run = kernel(OP_ENCODE, ...);
run.wait();

// Read results
bo_out.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
```

**Typical modifications:**
1. Change test data (lines 50-60)
2. Add new operations (lines 200+)
3. Modify error checking (lines 150+)

---

## 🛠️ Utility Files

### scripts/setup_env.sh
**Purpose:** Environment setup and verification  
**Use:** `./scripts/setup_env.sh`  
**Contains:**
- Tool detection
- Device check
- Directory creation
- Environment summary

**Run when:**
- First time setup
- After reinstalling tools
- Verifying environment

### .gitignore
**Purpose:** Exclude build artifacts from git  
**Contains:** Patterns for build/, logs, temp files  

---

## 📁 Generated Files (After Build)

### HW/build/bfp_kernel.xo
**Generated by:** `v++ -c` (compile step)  
**Size:** ~500 KB  
**Purpose:** Intermediate kernel object  
**Build time:** ~5-10 minutes

### HW/build/bfp_kernel.xclbin ⭐ **BITSTREAM**
**Generated by:** `v++ -l` (link step)  
**Size:** ~40-80 MB  
**Purpose:** FPGA bitstream for Alveo  
**Build time:** 2-4 hours  
**This is what gets loaded onto the FPGA**

### SW/build/bfp_host
**Generated by:** `g++` + XRT libs  
**Size:** ~50 KB  
**Purpose:** Host executable  
**Build time:** ~10 seconds

### HW/build/logs/*.log
**Generated during:** v++ compilation/linking  
**Purpose:** Build logs, warnings, errors  
**Use:** Debugging build failures  
**Check:** `tail -f HW/build/logs/*.log`

### HW/build/*_csynth.rpt
**Generated by:** HLS synthesis  
**Purpose:** Resource utilization, timing, performance  
**Contains:**
- Latency (cycles)
- Resource usage (LUT, FF, BRAM, DSP)
- Timing (slack)
- Interface details

**Check:** `cat HW/build/*_csynth.rpt`

---

## 🎯 Workflow Diagram

```
1. READ:        README.md → GUIDE.md → CHEATSHEET.md
                     ↓
2. SETUP:       scripts/setup_env.sh
                     ↓
3. BUILD HW:    HW/Makefile → bfp_kernel.cpp + headers
                     ↓
                HW/build/bfp_kernel.xclbin
                     ↓
4. BUILD SW:    SW/Makefile → bfp_host.cpp
                     ↓
                SW/build/bfp_host
                     ↓
5. FLASH:       xbutil program -d 0 -u *.xclbin
                     ↓
6. RUN:         ./bfp_host *.xclbin
                     ↓
7. VERIFY:      Check results, use CHECKLIST.md
```

---

## 📊 File Modification Priority

### ⚠️ HIGH - Likely to Modify

1. **SW/src/bfp_host.cpp** - Your application logic
2. **HW/src/bfp_kernel.cpp** - Kernel configuration (WE, WM, N)
3. **HW/Makefile** - Clock frequency, optimization

### ⚙️ MEDIUM - Sometimes Modify

4. **HW/src/bfp_ops_hls.h** - Add new operations
5. **Makefile** - Custom build targets

### 🔒 LOW - Rarely Modify

6. **HW/src/bfp_hls.h** - Core BFP implementation
7. **SW/Makefile** - Usually works as-is
8. **scripts/setup_env.sh** - Usually works as-is

---

## 🔍 Finding Things

### "Where do I change...?"

| What to change | File to edit | Line/Section |
|----------------|--------------|--------------|
| Precision (WE/WM) | `HW/src/bfp_kernel.cpp` | Lines 10-11 |
| Block size (N) | `HW/src/bfp_kernel.cpp` | Line 12 |
| Clock frequency | `HW/Makefile` | `--kernel_frequency` |
| Test data | `SW/src/bfp_host.cpp` | Lines 50-60 |
| Platform | `HW/Makefile` | `PLATFORM` variable |
| Add operation | `HW/src/bfp_kernel.cpp` | Switch statement |

### "Where is the...?"

| Looking for | Location |
|-------------|----------|
| Bitstream | `HW/build/bfp_kernel.xclbin` |
| Executable | `SW/build/bfp_host` |
| Build logs | `HW/build/logs/` |
| Reports | `HW/build/*.rpt` |
| Help | `GUIDE.md` |

---

## 📝 File Dependencies

```
bfp_kernel.cpp
    ├── includes: bfp_hls.h
    ├── includes: bfp_ops_hls.h
    └── builds to: bfp_kernel.xclbin

bfp_host.cpp
    ├── includes: XRT headers
    ├── links: libxrt_coreutil
    └── builds to: bfp_host

Makefile (root)
    ├── calls: HW/Makefile
    └── calls: SW/Makefile
```

---

## 🆘 Emergency Reference

**Lost? Start here:**
1. Read `README.md` (2 min)
2. Run `make help`
3. Check `CHEATSHEET.md`

**Build failing?**
1. Check `HW/build/logs/*.log`
2. Read `GUIDE.md` → Troubleshooting

**Test failing?**
1. Check `xbutil examine -d 0`
2. Read `GUIDE.md` → Troubleshooting

**Need quick command?**
1. See `CHEATSHEET.md`

---

**Use this index to navigate the project efficiently!**
