#include <iostream>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>

// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

// Profiler
#include "timer.hpp"

// FP32 common definitions
#include "common_fp32.h"

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

int main(int argc, char** argv) {
    INIT_PROFILER(fp32_profiler)
    int device_index = 0;

    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <operation> <n_blocks>" << std::endl;
        std::cerr << "  operation: 2=ADD, 3=SUB, 4=MUL, 5=DIV, 6=RCP" << std::endl;
        std::cerr << "  n_blocks: number of blocks (e.g., 2)" << std::endl;
        return EXIT_FAILURE;
    }

    // Get input parameters
    static std::string binaryFile = "../HW/package.hw/fp32_kernel.xclbin";
    unsigned int operation = std::stoi(argv[1]);
    unsigned int n_blocks = std::stoi(argv[2]);

    if (operation < 2 || operation > 6) {
        std::cerr << "Error: Invalid operation code. Must be 2-6" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "FP32 Accelerator Test" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Operation: " << FP32_OP_NAMES[operation-2] << " (" << operation << ")" << std::endl;
    std::cout << "Number of blocks: " << n_blocks << std::endl;
    std::cout << "Block size (N): " << N << std::endl;
    std::cout << std::endl;

    // Compute sizes
    unsigned int size_fp32 = n_blocks * N;

    GET_PROFILE_INSTANCE(setup_time, fp32_profiler);
    setup_time->reset();

    std::cout << "Opening device " << device_index << "..." << std::endl;
    auto device = xrt::device(device_index);
    
    std::cout << "Loading xclbin: " << binaryFile << "..." << std::endl;
    auto uuid = device.load_xclbin(binaryFile);
    
    std::cout << "Creating kernel handle..." << std::endl;
    auto fp32_kernel = xrt::kernel(device, uuid, "fp32_kernel");
    
    setup_time->tick();

    std::cout << "Allocating buffers in global memory..." << std::endl;
    
    // Kernel arguments for FP32
    //   0: operation (scalar)
    //   1: n_blocks (scalar)  
    //   2: in_fp32_a -> gmem0
    //   3: in_fp32_b -> gmem1
    //   4: out_fp32  -> gmem2
    
    auto bo_in_fp32_a = xrt::bo(device, size_fp32 * sizeof(float), fp32_kernel.group_id(2));
    auto bo_in_fp32_b = xrt::bo(device, size_fp32 * sizeof(float), fp32_kernel.group_id(3));
    auto bo_out_fp32  = xrt::bo(device, size_fp32 * sizeof(float), fp32_kernel.group_id(4));

    // Map buffers
    auto bo_in_fp32_a_map = bo_in_fp32_a.map<float*>();
    auto bo_in_fp32_b_map = bo_in_fp32_b.map<float*>();
    auto bo_out_fp32_map  = bo_out_fp32.map<float*>();

    // Test data - Same patterns as BFP for fair comparison
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

    // Initialize buffers
    std::fill(bo_in_fp32_a_map, bo_in_fp32_a_map + size_fp32, 0.0f);
    std::fill(bo_in_fp32_b_map, bo_in_fp32_b_map + size_fp32, 0.0f);
    std::fill(bo_out_fp32_map, bo_out_fp32_map + size_fp32, 0.0f);

    // Prepare test data
    std::vector<float> A_fp(size_fp32), B_fp(size_fp32);
    std::vector<float> golden_ref(size_fp32);
    
    // Fill data for all blocks
    for (unsigned int blk = 0; blk < n_blocks; ++blk) {
        unsigned int offset = blk * N;
        
        switch(blk % 2) {
            case 0:
                std::memcpy(&A_fp[offset], A0, sizeof(float) * N);
                std::memcpy(&B_fp[offset], B0, sizeof(float) * N);
                break;
            case 1:
                std::memcpy(&A_fp[offset], A1, sizeof(float) * N);
                std::memcpy(&B_fp[offset], B1, sizeof(float) * N);
                break;
        }
    }

    // Compute golden reference (software implementation)
    for (unsigned int i = 0; i < size_fp32; ++i) {
        switch (operation) {
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

    // Fill input buffers
    std::memcpy(bo_in_fp32_a_map, A_fp.data(), sizeof(float) * size_fp32);
    std::memcpy(bo_in_fp32_b_map, B_fp.data(), sizeof(float) * size_fp32);

    std::cout << "Syncing input buffers to device..." << std::endl;
    
    START_PROFILE(kernel_execution, fp32_profiler, 10)
    
    bo_in_fp32_a.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    bo_in_fp32_b.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    std::cout << "Executing kernel: " << FP32_OP_NAMES[operation-2] << "..." << std::endl;
    
    // Kernel call
    auto run = fp32_kernel(
        operation,
        n_blocks,
        bo_in_fp32_a,
        bo_in_fp32_b,
        bo_out_fp32
    );
    
    run.wait();
    std::cout << "Kernel completed!" << std::endl;

    std::cout << "Reading output buffers from device..." << std::endl;
    bo_out_fp32.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    
    END_PROFILE(kernel_execution);

    // Display results
    std::cout << "\n========================================" << std::endl;
    std::cout << "Results" << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::cout << "\nFirst block - " << FP32_OP_NAMES[operation-2] << " result (first 8 elements):" << std::endl;
    for (int i = 0; i < 8; ++i) {
        std::cout << "  [" << i << "] ";
        
        // Show operation
        if (operation == OP_RCP) {
            std::cout << "1 / " << B_fp[i] << " = " << bo_out_fp32_map[i];
        } else {
            std::cout << A_fp[i];
            switch(operation) {
                case OP_ADD: std::cout << " + "; break;
                case OP_SUB: std::cout << " - "; break;
                case OP_MUL: std::cout << " * "; break;
                case OP_DIV: std::cout << " / "; break;
            }
            std::cout << B_fp[i] << " = " << bo_out_fp32_map[i];
        }
        
        // Show expected value
        std::cout << " (expected: " << golden_ref[i] << ")" << std::endl;
    }

    // Validate results
    double mae = 0.0, mape = 0.0;
    compute_metrics(golden_ref.data(), bo_out_fp32_map, size_fp32, mae, mape);
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Accuracy Metrics" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "MAE:  " << mae << std::endl;
    std::cout << "MAPE: " << mape << "%" << std::endl;
    
    // FP32 should be bit-exact with software reference
    bool passed = (mae < 1e-6 && mape < 0.001);
    std::cout << "\n" << (passed ? "✓ TEST PASSED" : "✗ TEST FAILED") << std::endl;

    std::cout << "\n" << fp32_profiler << std::endl;
    std::cout << "\n========================================" << std::endl;

    return 0;
}
