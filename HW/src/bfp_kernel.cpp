#include <ap_int.h>
#include "bfp_hls.h"
#include "bfp_ops_hls.h"

// Configuration
#define WE 5
#define WM 7
#define N 16

using Cfg = BFP_bias<WE, WM>;
using blk_t = BFP_Global<Cfg, N>;

// Operation codes
typedef enum : unsigned int {
    OP_ENCODE = 0,
    OP_DECODE = 1,
    OP_ADD    = 2,
    OP_SUB    = 3,
    OP_MUL    = 4,
    OP_DIV    = 5,
    OP_RCP    = 6
} bfp_op_t;

//=============================================================================
// MAIN KERNEL - Simplified without DATAFLOW for stable synthesis
//=============================================================================
extern "C" {

void bfp_kernel(
    // Control
    const unsigned int operation,
    const unsigned int n_blocks,
    
    // Input A (FP32 or BFP format)
    const float* in_fp32_a,
    const unsigned int* in_exp_a,
    const unsigned int* in_sign_a,
    const unsigned int* in_mant_a,
    
    // Input B (BFP format for binary ops)
    const unsigned int* in_exp_b,
    const unsigned int* in_sign_b,
    const unsigned int* in_mant_b,
    
    // Output (FP32 or BFP format)
    float* out_fp32,
    unsigned int* out_exp,
    unsigned int* out_sign,
    unsigned int* out_mant
) {
    // Interface pragmas
    #pragma HLS INTERFACE s_axilite port=operation bundle=control
    #pragma HLS INTERFACE s_axilite port=n_blocks bundle=control
    #pragma HLS INTERFACE s_axilite port=return bundle=control

    // Memory interfaces - m_axi for DDR access
    #pragma HLS INTERFACE m_axi port=in_fp32_a offset=slave bundle=gmem0 \
        max_read_burst_length=16 num_read_outstanding=4
    #pragma HLS INTERFACE s_axilite port=in_fp32_a bundle=control
    
    #pragma HLS INTERFACE m_axi port=in_exp_a offset=slave bundle=gmem1 \
        max_read_burst_length=16 num_read_outstanding=4
    #pragma HLS INTERFACE s_axilite port=in_exp_a bundle=control
    
    #pragma HLS INTERFACE m_axi port=in_sign_a offset=slave bundle=gmem2 \
        max_read_burst_length=16 num_read_outstanding=4
    #pragma HLS INTERFACE s_axilite port=in_sign_a bundle=control
    
    #pragma HLS INTERFACE m_axi port=in_mant_a offset=slave bundle=gmem3 \
        max_read_burst_length=16 num_read_outstanding=4
    #pragma HLS INTERFACE s_axilite port=in_mant_a bundle=control

    #pragma HLS INTERFACE m_axi port=in_exp_b offset=slave bundle=gmem1 \
        max_read_burst_length=16 num_read_outstanding=4
    #pragma HLS INTERFACE s_axilite port=in_exp_b bundle=control
    
    #pragma HLS INTERFACE m_axi port=in_sign_b offset=slave bundle=gmem2 \
        max_read_burst_length=16 num_read_outstanding=4
    #pragma HLS INTERFACE s_axilite port=in_sign_b bundle=control
    
    #pragma HLS INTERFACE m_axi port=in_mant_b offset=slave bundle=gmem3 \
        max_read_burst_length=16 num_read_outstanding=4
    #pragma HLS INTERFACE s_axilite port=in_mant_b bundle=control

    #pragma HLS INTERFACE m_axi port=out_fp32 offset=slave bundle=gmem0 \
        max_write_burst_length=16 num_write_outstanding=4
    #pragma HLS INTERFACE s_axilite port=out_fp32 bundle=control
    
    #pragma HLS INTERFACE m_axi port=out_exp offset=slave bundle=gmem1 \
        max_write_burst_length=16 num_write_outstanding=4
    #pragma HLS INTERFACE s_axilite port=out_exp bundle=control
    
    #pragma HLS INTERFACE m_axi port=out_sign offset=slave bundle=gmem2 \
        max_write_burst_length=16 num_write_outstanding=4
    #pragma HLS INTERFACE s_axilite port=out_sign bundle=control
    
    #pragma HLS INTERFACE m_axi port=out_mant offset=slave bundle=gmem3 \
        max_write_burst_length=16 num_write_outstanding=4
    #pragma HLS INTERFACE s_axilite port=out_mant bundle=control

    // Main processing loop - Simplified sequential design
    process_blocks: for (unsigned int blk_idx = 0; blk_idx < n_blocks; blk_idx++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=128 avg=32
        
        const unsigned int offset = blk_idx * N;
        blk_t A{}, B{}, Z{};
        std::array<float, N> fp_in{}, fp_out{};
        
        //=====================================================================
        // PHASE 1: LOAD DATA
        //=====================================================================
        if (operation == OP_ENCODE) {
            // Load FP32 for encoding
        load_fp32: 
            for (int i = 0; i < N; i++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=16 max=16 avg=16
                fp_in[i] = in_fp32_a[offset + i];
            }
            
        } else if (operation == OP_DECODE) {
            // Load BFP A for decoding
            A.exp_shared = in_exp_a[blk_idx];
        load_bfp_decode: 
            for (int i = 0; i < N; i++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=16 max=16 avg=16
                A.sign[i] = in_sign_a[offset + i];
                A.mant[i] = in_mant_a[offset + i];
            }
            
        } else if (operation == OP_RCP) {
            // Load only B for reciprocal
            B.exp_shared = in_exp_b[blk_idx];
        load_bfp_rcp: 
            for (int i = 0; i < N; i++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=16 max=16 avg=16
                B.sign[i] = in_sign_b[offset + i];
                B.mant[i] = in_mant_b[offset + i];
            }
            
        } else {
            // Binary operations: Load both A and B
            A.exp_shared = in_exp_a[blk_idx];
            B.exp_shared = in_exp_b[blk_idx];
        load_bfp_binary: 
            for (int i = 0; i < N; i++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=16 max=16 avg=16
                A.sign[i] = in_sign_a[offset + i];
                A.mant[i] = in_mant_a[offset + i];
                B.sign[i] = in_sign_b[offset + i];
                B.mant[i] = in_mant_b[offset + i];
            }
        }
        
        //=====================================================================
        // PHASE 2: COMPUTE
        //=====================================================================
        switch (operation) {
            case OP_ENCODE:
                Z = encode_block<Cfg, N>(fp_in);
                break;
                
            case OP_DECODE:
                fp_out = decode_block<Cfg, N>(A);
                break;
                
            case OP_ADD:
                Z = add_blocks<Cfg, N>(A, B);
                break;
                
            case OP_SUB:
                Z = sub_blocks<Cfg, N>(A, B);
                break;
                
            case OP_MUL:
                Z = mul_blocks<Cfg, N>(A, B);
                break;
                
            case OP_DIV:
                Z = div_blocks<Cfg, N>(A, B);
                break;
                
            case OP_RCP:
                Z = rcp_blocks<Cfg, N>(B);
                break;
                
            default:
                Z = A;
                break;
        }
        
        //=====================================================================
        // PHASE 3: STORE RESULTS
        //=====================================================================
        if (operation == OP_DECODE) {
            // Write FP32 output
        store_fp32: 
            for (int i = 0; i < N; i++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=16 max=16 avg=16
                out_fp32[offset + i] = fp_out[i];
            }
            
        } else {
            // Write BFP output
            out_exp[blk_idx] = Z.exp_shared;
        store_bfp: 
            for (int i = 0; i < N; i++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=16 max=16 avg=16
                out_sign[offset + i] = Z.sign[i];
                out_mant[offset + i] = Z.mant[i];
            }
        }
    }
}

} // extern "C"
