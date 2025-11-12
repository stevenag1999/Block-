# ✅ BFP Alveo U55C - Step-by-Step Checklist

Use this checklist to ensure successful setup and execution.

---

## 📋 Pre-Flight Checklist

### 1. Environment Setup
- [ ] Vitis 2024.2 installed
- [ ] XRT installed
- [ ] Connected to Alveo node (SSH)
- [ ] Have access to `/mnt/scratch/$USER`

### 2. Source Required Tools
```bash
- [ ] source /tools/Xilinx/Vitis/2024.2/settings64.sh
- [ ] source /opt/xilinx/xrt/setup.sh
- [ ] echo $XILINX_VITIS  # Verify
- [ ] echo $XILINX_XRT    # Verify
```

### 3. Verify Alveo Device
```bash
- [ ] lspci | grep Xilinx           # Card detected
- [ ] xbutil examine -d 0           # Device accessible
- [ ] xbutil validate -d 0          # Device functional
```

---

## 🔨 Build Checklist

### 4. Initial Setup
```bash
- [ ] cd bfp_alveo
- [ ] ./scripts/setup_env.sh        # Run setup
- [ ] make check                    # Verify environment
```

### 5. Build Hardware (2-4 hours - Do this first!)
```bash
- [ ] cd HW
- [ ] make info                     # Check configuration
- [ ] make                          # Start build
- [ ] # Go get coffee ☕
```

**While hardware builds, monitor progress:**
```bash
- [ ] tail -f build/logs/*.log      # In another terminal
```

### 6. Build Software (10 seconds)
```bash
- [ ] cd SW
- [ ] make                          # Build host
- [ ] ls build/bfp_host             # Verify executable exists
```

### 7. Verify Build Outputs
```bash
- [ ] ls HW/build/bfp_kernel.xo     # Kernel object exists
- [ ] ls HW/build/bfp_kernel.xclbin # Bitstream exists
- [ ] ls SW/build/bfp_host          # Executable exists
- [ ] # All files present? ✓
```

---

## 🚀 Execution Checklist

### 8. Program FPGA
```bash
- [ ] cd bfp_alveo  # Project root
- [ ] make flash    # Program FPGA
- [ ] # Wait for "FPGA programmed" message
```

**Verify programming:**
```bash
- [ ] xbutil examine -d 0 -r platform  # Check loaded xclbin
```

### 9. Run Test
```bash
- [ ] make test     # Run full test suite
```

**Expected results:**
```
- [ ] See "Initializing Alveo device..." ✓
- [ ] See "ENCODE block A" completed ✓
- [ ] See "ADDITION" test passed ✓
- [ ] See "MULTIPLICATION" test passed ✓
- [ ] See "ALL TESTS COMPLETED SUCCESSFULLY!" ✓
```

### 10. Verify Results
```
- [ ] Max error < 5% for all operations
- [ ] No crashes or segfaults
- [ ] Execution time < 1 second per test
```

---

## 🔍 Troubleshooting Checklist

### If Build Fails

**Check logs:**
```bash
- [ ] cat HW/build/logs/v++_compile*.log | grep ERROR
- [ ] cat HW/build/logs/v++_link*.log | grep ERROR
```

**Common fixes:**
- [ ] Timing violation? → Reduce clock frequency in `HW/Makefile`
- [ ] Resource overflow? → Reduce `N` or `WM`/`WE` in `bfp_kernel.cpp`
- [ ] Platform mismatch? → Check PLATFORM in `HW/Makefile`

### If Flash Fails

```bash
- [ ] xbutil reset -d 0              # Reset device
- [ ] xbutil examine -d 0            # Re-check
- [ ] make flash                     # Try again
```

### If Test Fails

```bash
- [ ] Check XCLBIN loaded: xbutil examine -d 0 -r platform
- [ ] Check host built: ls SW/build/bfp_host
- [ ] Enable debug: export XRT_INI=xrt.ini
```

---

## 📊 Performance Verification Checklist

### 11. Benchmark Performance
```bash
- [ ] xbutil top -d 0                # Monitor utilization
- [ ] # Run test again with timing
- [ ] ./SW/build/bfp_host ../HW/build/bfp_kernel.xclbin | grep "Time:"
```

**Expected performance:**
```
- [ ] ENCODE: < 100 us
- [ ] ADD:    < 50 us
- [ ] MUL:    < 50 us
- [ ] DIV:    < 100 us
- [ ] Total:  < 500 us per full pipeline
```

### 12. Resource Utilization
```bash
- [ ] Check report: cat HW/build/*_csynth.rpt
```

**Expected utilization (U55C):**
```
- [ ] LUTs:  < 20% (of 425,280)
- [ ] FFs:   < 15% (of 850,560)
- [ ] BRAMs: < 10% (of 1,824)
- [ ] DSPs:  < 5%  (of 1,312)
```

---

## 🎓 PYNQ Integration Checklist

### 13. Setup JupyterLab (Optional)
```bash
- [ ] cd /mnt/scratch/$USER
- [ ] export http_proxy=http://proxy.ethz.ch:3128
- [ ] export https_proxy=http://proxy.ethz.ch:3128
- [ ] source /mnt/scratch/anaconda3/bin/activate
- [ ] pip install pynq
- [ ] jupyter-lab --ip='0.0.0.0'
```

### 14. Access JupyterLab
```bash
- [ ] Copy token from terminal output
- [ ] Open browser: http://127.0.0.1:8888/
- [ ] Paste token
- [ ] Create new Python 3 notebook
```

### 15. Test in Notebook
```python
- [ ] import pynq
- [ ] overlay = pynq.Overlay('/path/to/bfp_kernel.xclbin')
- [ ] # Run sample code
```

---

## ✨ Final Verification

### 16. Complete System Check
```
- [ ] Environment sourced correctly ✓
- [ ] Alveo device detected ✓
- [ ] Hardware built successfully ✓
- [ ] Software built successfully ✓
- [ ] FPGA programmed ✓
- [ ] Tests pass with good accuracy ✓
- [ ] Performance meets expectations ✓
```

### 17. Documentation Check
```
- [ ] README.md read ✓
- [ ] GUIDE.md reviewed ✓
- [ ] CHEATSHEET.md available ✓
```

---

## 🎯 Next Steps

### After Successful Setup

1. **Experiment with configurations:**
   - [ ] Try different `WE`/`WM` values
   - [ ] Increase block size `N`
   - [ ] Add new operations

2. **Integrate into your workflow:**
   - [ ] Copy XCLBIN to shared location
   - [ ] Create wrapper scripts
   - [ ] Document your use case

3. **Share results:**
   - [ ] Benchmark your application
   - [ ] Compare vs FP32
   - [ ] Document findings

---

## 📞 Need Help?

If any checklist item fails:

1. **Read the error message** carefully
2. **Check GUIDE.md** section on troubleshooting
3. **Review CHEATSHEET.md** for quick fixes
4. **Check device status**: `xbutil examine -d 0`
5. **Reset if needed**: `xbutil reset -d 0`

**Still stuck?**
- Check GitHub issues: https://github.com/ECASLab/
- Contact ECASLab support
- Review Xilinx forums

---

## 🏆 Success Criteria

You're done when:
✅ All checkboxes above are marked
✅ Tests pass consistently
✅ You understand the workflow
✅ You can modify and rebuild

**Congratulations!** 🎉

You now have a working BFP accelerator on Alveo U55C!

---

**Print this checklist and mark items as you complete them.**

Last updated: November 2025
