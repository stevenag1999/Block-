# BFP Alveo U55C - Command Cheatsheet

## Setup (One-time)

```bash
# Source tools
source /tools/Xilinx/Vitis/2024.2/settings64.sh
source /opt/xilinx/xrt/setup.sh

# Run setup script
./scripts/setup_env.sh
```

## Build (First Time - Takes 2-4 hours)

```bash
make              # Build everything
```

Or separately:
```bash
make hw           # Build kernel (slow, 2-4 hours)
make sw           # Build host (fast, 10 sec)
```

## Program FPGA

```bash
make flash
```

Or manually:
```bash
xbutil program -d 0 -u HW/build/bfp_kernel.xclbin
```

## Run Test

```bash
make test
```

Or manually:
```bash
cd SW
./build/bfp_host ../HW/build/bfp_kernel.xclbin
```

## Device Management

```bash
# Check device
xbutil examine -d 0

# Validate
xbutil validate -d 0

# Monitor
xbutil top -d 0

# Reset
xbutil reset -d 0
```

## Development (Fast Emulation)

```bash
# Build emulation (10 minutes)
make hw-emu
make sw

# Run emulation
cd SW
XCL_EMULATION_MODE=hw_emu ./build/bfp_host ../HW/build/bfp_kernel.xclbin
```

## PYNQ (JupyterLab)

```bash
# On Alveo node
cd /mnt/scratch/$USER
export http_proxy=http://proxy.ethz.ch:3128
export https_proxy=http://proxy.ethz.ch:3128

source /mnt/scratch/anaconda3/bin/activate
pip install pynq

jupyter-lab --ip='0.0.0.0'
# Copy token from output, open in browser
```

## Clean Build

```bash
make clean        # Clean all
make clean-hw     # Clean HW only
make clean-sw     # Clean SW only
```

## Debugging

```bash
# Check environment
make check

# View build logs
tail -f HW/build/logs/*.log

# Check timing
grep "timing" HW/build/*.rpt

# Enable XRT debug
export XRT_INI=xrt.ini  # Create xrt.ini with debug settings
```

## Common Fixes

### Environment not set
```bash
source /tools/Xilinx/Vitis/2024.2/settings64.sh
source /opt/xilinx/xrt/setup.sh
```

### Device not found
```bash
lspci | grep Xilinx
xbutil reset -d 0
```

### Timing failure
```bash
# Edit HW/Makefile
# Change: --kernel_frequency 250
# To:     --kernel_frequency 200
make clean-hw
make hw
```

## File Locations

```
Configuration:     HW/src/bfp_kernel.cpp (WE, WM, N)
Kernel source:     HW/src/bfp_kernel.cpp
BFP core:          HW/src/bfp_hls.h
BFP operations:    HW/src/bfp_ops_hls.h
Host app:          SW/src/bfp_host.cpp
Bitstream:         HW/build/bfp_kernel.xclbin
Executable:        SW/build/bfp_host
```

## Performance Monitoring

```bash
# Real-time stats
xbutil top -d 0

# Execution time
./SW/build/bfp_host ../HW/build/bfp_kernel.xclbin | grep "Time:"

# Resource utilization
cat HW/build/*_csynth.rpt | grep -A 10 "Utilization"
```

## Help

```bash
make help                    # Makefile help
cat GUIDE.md                 # Full documentation
cat README.md                # Quick overview
xbutil --help                # XRT tool help
v++ --help                   # Vitis compiler help
```

---

**Quick Reference Card**

| What | Command |
|------|---------|
| Build all | `make` |
| Program FPGA | `make flash` |
| Run test | `make test` |
| Check device | `xbutil examine -d 0` |
| Clean | `make clean` |
| Help | `make help` |

---

**Emergency Reset**

```bash
xbutil reset -d 0
sudo rmmod xocl xclmgmt
sudo modprobe xocl xclmgmt
```
