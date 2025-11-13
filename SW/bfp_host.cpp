#include <iostream>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <vector>

// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

// Profiler
#include "timer.hpp"

// BFP common definitions
#include "common_bfp.h"

// Helper to compute metrics (MAE and MAPE)
void compute_metrics(const float* ref, const float* got, unsigned int len,
                     double& mae, double& mape) {
    double abs_sum = 0.0, ape_sum = 0.0;
    unsigned int mape_cnt = 0;
    for (unsigned int i = 0; i < len; ++i) {
        const double r = ref[i], g = got[i];
        const double ae = std::fabs(g - r);
        abs_sum += ae;
        if (std::fabs(r) > 1e-12) { 
            ape_sum += ae / std::fabs(r); 
            ++mape_cnt; 
        }
    }
    mae = abs_sum / double(len);
    mape = (mape_cnt ? (ape_sum / double(mape_cnt)) * 100.0 : 0.0);
}

// Simple CPU-side BFP encoding for test data preparation
struct SimpleBFP {
    unsigned int exp_shared;
    std::vector<unsigned int> sign;
    std::vector<unsigned int> mant;
    
    SimpleBFP(unsigned int n) : exp_shared(0), sign(n, 0), mant(n, 0) {}
};

SimpleBFP encode_fp32_to_bfp(const float* data, unsigned int n) {
    SimpleBFP result(n);
    
    // Find max exponent
    int max_exp = -1000;
    for (unsigned int i = 0; i < n; i++) {
        if (data[i] == 0.0f) continue;
        
        union {float f; uint32_t u;} u = {data[i]};
        int exp = int((u.u >> 23) & 0xFF);
        if (exp > 0) {
            int exp_unbiased = exp - 127;
            if (exp_unbiased > max_exp) max_exp = exp_unbiased;
        }
    }
    
    if (max_exp == -1000) {
        return result; // All zeros
    }
    
    // Calculate shared exponent with bias
    int exp_shared_bfp = max_exp + ((1 << (WE - 1)) - 1); // bias = 15 for WE=5
    if (exp_shared_bfp < 0) exp_shared_bfp = 0;
    if (exp_shared_bfp > 31) exp_shared_bfp = 31;
    result.exp_shared = exp_shared_bfp;
    
    // Quantize each element
    for (unsigned int i = 0; i < n; i++) {
        if (data[i] == 0.0f) {
            result.sign[i] = 0;
            result.mant[i] = 0;
            continue;
        }
        
        union {float f; uint32_t u;} u = {data[i]};
        result.sign[i] = (u.u >> 31) & 0x1;
        
        int exp = int((u.u >> 23) & 0xFF);
        if (exp == 0) continue;
        
        uint32_t mant24 = (u.u & 0x7FFFFF) | (1u << 23);
        int exp_unbiased = exp - 127;
        
        int shift = (23 - WM) + (max_exp - exp_unbiased);
        
        uint32_t mant_reduced;
        if (shift >= 32) {
            mant_reduced = 0;
        } else if (shift < 0) {
            mant_reduced = mant24 << (-shift);
        } else {
            // Round to nearest even
            uint32_t q = mant24 >> shift;
            uint32_t rem = mant24 & ((1u << shift) - 1);
            uint32_t half = 1u << (shift - 1);
            if (rem > half || (rem == half && (q & 1))) {
                q++;
            }
            mant_reduced = q;
        }
        
        uint32_t max_mant = (1u << (WM + 1)) - 1;
        if (mant_reduced > max_mant) mant_reduced = max_mant;
        
        result.mant[i] = mant_reduced;
    }
    
    return result;
}

int main(int argc, char** argv) {
    INIT_PROFILER(bfp_profiler)
    int device_index = 0;

    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <operation> <n_blocks>" << std::endl;
        std::cerr << "  operation: 0=ENCODE, 1=DECODE, 2=ADD, 3=SUB, 4=MUL, 5=DIV, 6=RCP" << std::endl;
        std::cerr << "  n_blocks: number of blocks (e.g., 2)" << std::endl;
        return EXIT_FAILURE;
    }

    // Get input parameters
    static std::string binaryFile = "../HW/package.hw/kernels.xclbin";
    unsigned int operation = std::stoi(argv[1]);
    unsigned int n_blocks = std::stoi(argv[2]);

    if (operation > 6) {
        std::cerr << "Error: Invalid operation code. Must be 0-6" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "BFP Accelerator Test" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Operation: " << OP_NAMES[operation] << " (" << operation << ")" << std::endl;
    std::cout << "Number of blocks: " << n_blocks << std::endl;
    std::cout << "Block size (N): " << N << std::endl;
    std::cout << std::endl;

    // Compute sizes
    unsigned int size = n_blocks * N;

    GET_PROFILE_INSTANCE(setup_time, bfp_profiler);
    setup_time->reset();

    std::cout << "Opening device " << device_index << "..." << std::endl;
    auto device = xrt::device(device_index);
    
    std::cout << "Loading xclbin: " << binaryFile << "..." << std::endl;
    auto uuid = device.load_xclbin(binaryFile);
    
    std::cout << "Creating kernel handle..." << std::endl;
    auto bfp_kernel = xrt::kernel(device, uuid, "bfp_kernel");
    
    setup_time->tick();

    std::cout << "Allocating buffers in global memory..." << std::endl;
    
    // CRITICAL: group_id(X) refers to kernel argument index, not bundle number
    // Kernel arguments:
    //   0: operation (scalar)
    //   1: n_blocks (scalar)
    //   2: in_fp32_a   -> bundle gmem0
    //   3: in_exp_a    -> bundle gmem1
    //   4: in_sign_a   -> bundle gmem2
    //   5: in_mant_a   -> bundle gmem3
    //   6: in_exp_b    -> bundle gmem1
    //   7: in_sign_b   -> bundle gmem2
    //   8: in_mant_b   -> bundle gmem3
    //   9: out_fp32    -> bundle gmem0
    //   10: out_exp    -> bundle gmem1
    //   11: out_sign   -> bundle gmem2
    //   12: out_mant   -> bundle gmem3
    
    auto bo_in_fp32_a = xrt::bo(device, size * sizeof(float), bfp_kernel.group_id(2));
    auto bo_in_exp_a  = xrt::bo(device, n_blocks * sizeof(unsigned int), bfp_kernel.group_id(3));
    auto bo_in_sign_a = xrt::bo(device, size * sizeof(unsigned int), bfp_kernel.group_id(4));
    auto bo_in_mant_a = xrt::bo(device, size * sizeof(unsigned int), bfp_kernel.group_id(5));
    
    auto bo_in_exp_b  = xrt::bo(device, n_blocks * sizeof(unsigned int), bfp_kernel.group_id(6));
    auto bo_in_sign_b = xrt::bo(device, size * sizeof(unsigned int), bfp_kernel.group_id(7));
    auto bo_in_mant_b = xrt::bo(device, size * sizeof(unsigned int), bfp_kernel.group_id(8));
    
    auto bo_out_fp32 = xrt::bo(device, size * sizeof(float), bfp_kernel.group_id(9));
    auto bo_out_exp  = xrt::bo(device, n_blocks * sizeof(unsigned int), bfp_kernel.group_id(10));
    auto bo_out_sign = xrt::bo(device, size * sizeof(unsigned int), bfp_kernel.group_id(11));
    auto bo_out_mant = xrt::bo(device, size * sizeof(unsigned int), bfp_kernel.group_id(12));

    // Map buffers
    auto bo_in_fp32_a_map = bo_in_fp32_a.map<float*>();
    auto bo_in_exp_a_map  = bo_in_exp_a.map<unsigned int*>();
    auto bo_in_sign_a_map = bo_in_sign_a.map<unsigned int*>();
    auto bo_in_mant_a_map = bo_in_mant_a.map<unsigned int*>();
    
    auto bo_in_exp_b_map  = bo_in_exp_b.map<unsigned int*>();
    auto bo_in_sign_b_map = bo_in_sign_b.map<unsigned int*>();
    auto bo_in_mant_b_map = bo_in_mant_b.map<unsigned int*>();
    
    auto bo_out_fp32_map = bo_out_fp32.map<float*>();
    auto bo_out_exp_map  = bo_out_exp.map<unsigned int*>();
    auto bo_out_sign_map = bo_out_sign.map<unsigned int*>();
    auto bo_out_mant_map = bo_out_mant.map<unsigned int*>();

    // Test data - Two different block patterns
    std::cout << "Preparing test data..." << std::endl;
    
    float A0[N] = {
        12.35f,  6.50f, 10.20f,  6.60f,  8.80f,  2.56f, 11.11f,  8.00f,
         5.45f,  9.99f,  0.15f, 18.00f,  3.80f, 90.10f, 14.00f, 10.00f
    };
    float A1[N] = {
         1.0f, 2.0f, 3.0f,  4.0f,  5.0f,  6.0f,  7.0f,  8.0f,
         9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f
    };
    float B0[N] = {
        2.0f, 1.0f, 2.0f, 3.0f, 2.0f, 2.0f, 2.0f, 2.0f,
        3.0f, 3.0f, 5.0f, 3.0f, 6.0f, 3.0f, 8.0f, 2.0f
    };
    float B1[N] = {
        15.0f, 14.0f, 13.0f, 12.0f, 11.0f, 10.0f, 9.0f, 8.0f,
         7.0f,  6.0f,  5.0f,  4.0f,  3.0f,  2.0f, 1.0f, 0.5f
    };

    // Initialize all buffers
    std::fill(bo_in_fp32_a_map, bo_in_fp32_a_map + size, 0.0f);
    std::fill(bo_in_exp_a_map, bo_in_exp_a_map + n_blocks, 0u);
    std::fill(bo_in_sign_a_map, bo_in_sign_a_map + size, 0u);
    std::fill(bo_in_mant_a_map, bo_in_mant_a_map + size, 0u);
    std::fill(bo_in_exp_b_map, bo_in_exp_b_map + n_blocks, 0u);
    std::fill(bo_in_sign_b_map, bo_in_sign_b_map + size, 0u);
    std::fill(bo_in_mant_b_map, bo_in_mant_b_map + size, 0u);

    // Prepare test data based on operation
    std::vector<float> A_fp(size), B_fp(size);
    std::vector<float> golden_ref(size);
    
    // Fill data for all blocks
    for (unsigned int blk = 0; blk < n_blocks; ++blk) {
        unsigned int offset = blk * N;
        if (blk % 2 == 0) {
            std::memcpy(&A_fp[offset], A0, sizeof(float) * N);
            std::memcpy(&B_fp[offset], B0, sizeof(float) * N);
        } else {
            std::memcpy(&A_fp[offset], A1, sizeof(float) * N);
            std::memcpy(&B_fp[offset], B1, sizeof(float) * N);
        }
    }

    // Compute golden reference
    for (unsigned int i = 0; i < size; ++i) {
        switch (operation) {
            case OP_ENCODE:
            case OP_DECODE:
                golden_ref[i] = A_fp[i];
                break;
            case OP_ADD:
                golden_ref[i] = A_fp[i] + B_fp[i];
                break;
            case OP_SUB:
                golden_ref[i] = A_fp[i] - B_fp[i];
                break;
            case OP_MUL:
                golden_ref[i] = A_fp[i] * B_fp[i];
                break;
            case OP_DIV:
                golden_ref[i] = (std::fabs(B_fp[i]) > 1e-30f) ? (A_fp[i] / B_fp[i]) : 0.f;
                break;
            case OP_RCP:
                golden_ref[i] = (std::fabs(B_fp[i]) > 1e-30f) ? (1.0f / B_fp[i]) : 0.f;
                break;
        }
    }

    // Fill input buffers based on operation
    if (operation == OP_ENCODE) {
        // ENCODE: input is FP32
        std::memcpy(bo_in_fp32_a_map, A_fp.data(), sizeof(float) * size);
        
    } else if (operation == OP_DECODE) {
        // DECODE: input is BFP - encode A on CPU
        for (unsigned int blk = 0; blk < n_blocks; ++blk) {
            unsigned int offset = blk * N;
            SimpleBFP bfp_a = encode_fp32_to_bfp(&A_fp[offset], N);
            
            bo_in_exp_a_map[blk] = bfp_a.exp_shared;
            for (int i = 0; i < N; ++i) {
                bo_in_sign_a_map[offset + i] = bfp_a.sign[i];
                bo_in_mant_a_map[offset + i] = bfp_a.mant[i];
            }
        }
        
    } else if (operation == OP_RCP) {
        // RCP: input is BFP B only
        for (unsigned int blk = 0; blk < n_blocks; ++blk) {
            unsigned int offset = blk * N;
            SimpleBFP bfp_b = encode_fp32_to_bfp(&B_fp[offset], N);
            
            bo_in_exp_b_map[blk] = bfp_b.exp_shared;
            for (int i = 0; i < N; ++i) {
                bo_in_sign_b_map[offset + i] = bfp_b.sign[i];
                bo_in_mant_b_map[offset + i] = bfp_b.mant[i];
            }
        }
        
    } else {
        // Binary ops: encode both A and B
        for (unsigned int blk = 0; blk < n_blocks; ++blk) {
            unsigned int offset = blk * N;
            
            SimpleBFP bfp_a = encode_fp32_to_bfp(&A_fp[offset], N);
            SimpleBFP bfp_b = encode_fp32_to_bfp(&B_fp[offset], N);
            
            bo_in_exp_a_map[blk] = bfp_a.exp_shared;
            bo_in_exp_b_map[blk] = bfp_b.exp_shared;
            
            for (int i = 0; i < N; ++i) {
                bo_in_sign_a_map[offset + i] = bfp_a.sign[i];
                bo_in_mant_a_map[offset + i] = bfp_a.mant[i];
                bo_in_sign_b_map[offset + i] = bfp_b.sign[i];
                bo_in_mant_b_map[offset + i] = bfp_b.mant[i];
            }
        }
    }

    std::cout << "Syncing input buffers to device..." << std::endl;
    
    START_PROFILE(kernel_execution, bfp_profiler, 10)
    
    bo_in_fp32_a.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    bo_in_exp_a.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    bo_in_sign_a.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    bo_in_mant_a.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    bo_in_exp_b.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    bo_in_sign_b.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    bo_in_mant_b.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    std::cout << "Executing kernel: " << OP_NAMES[operation] << "..." << std::endl;
    auto run = bfp_kernel(
        operation,
        n_blocks,
        bo_in_fp32_a,
        bo_in_exp_a,
        bo_in_sign_a,
        bo_in_mant_a,
        bo_in_exp_b,
        bo_in_sign_b,
        bo_in_mant_b,
        bo_out_fp32,
        bo_out_exp,
        bo_out_sign,
        bo_out_mant
    );
    
    run.wait();
    std::cout << "Kernel completed!" << std::endl;

    std::cout << "Reading output buffers from device..." << std::endl;
    bo_out_fp32.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    bo_out_exp.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    bo_out_sign.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    bo_out_mant.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    
    END_PROFILE(kernel_execution);

    // Display results
    std::cout << "\n========================================" << std::endl;
    std::cout << "Results" << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::cout << "\nFirst block (8 elements):" << std::endl;
    for (int i = 0; i < 8; ++i) {
        if (operation == OP_DECODE) {
            std::cout << "  [" << i << "] FP32: " << bo_out_fp32_map[i] 
                      << " (expected: " << golden_ref[i] << ")" << std::endl;
        } else if (operation == OP_ENCODE) {
            std::cout << "  [" << i << "] BFP - sign: " << bo_out_sign_map[i]
                      << ", mant: " << bo_out_mant_map[i] << std::endl;
            if (i == 0) {
                std::cout << "  Block exp_shared: " << bo_out_exp_map[0] << std::endl;
            }
        } else {
            std::cout << "  [" << i << "] BFP - sign: " << bo_out_sign_map[i]
                      << ", mant: " << bo_out_mant_map[i] << std::endl;
        }
    }

    // Validate results for operations with FP32 output
    if (operation == OP_DECODE) {
        double mae = 0.0, mape = 0.0;
        compute_metrics(golden_ref.data(), bo_out_fp32_map, size, mae, mape);
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "Accuracy Metrics" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "MAE:  " << mae << std::endl;
        std::cout << "MAPE: " << mape << "%" << std::endl;
        
        bool passed = (mae < 1.0 && mape < 10.0);
        std::cout << "\n" << (passed ? "✓ TEST PASSED" : "✗ TEST FAILED") << std::endl;
        
    } else if (operation == OP_ENCODE) {
        // For ENCODE, we can verify that output is non-zero
        bool has_data = false;
        for (unsigned int i = 0; i < size; ++i) {
            if (bo_out_mant_map[i] != 0) {
                has_data = true;
                break;
            }
        }
        std::cout << "\n" << (has_data ? "✓ TEST PASSED (data encoded)" : "✗ TEST FAILED (no data)") << std::endl;
        
    } else {
        // For arithmetic operations, decode the result and compare
        std::cout << "\nNote: Arithmetic operation completed. " 
                  << "Decode result to validate against golden reference." << std::endl;
        std::cout << "✓ TEST COMPLETED" << std::endl;
    }

    std::cout << "\n" << bfp_profiler << std::endl;
    std::cout << "\n========================================" << std::endl;

    return 0;
}
