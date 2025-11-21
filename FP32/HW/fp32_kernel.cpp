#include <ap_int.h>
#include "fp32_ops_hls.h"

// Configuration
#define N 16

using blk_t = float[N];

// Operation codes
typedef enum : unsigned int {
    OP_ADD = 2,
    OP_SUB = 3, 
    OP_MUL = 4,
    OP_DIV = 5,
    OP_RCP = 6
} fp32_op_t;

//=============================================================================
// MAIN FP32 KERNEL
//=============================================================================
extern "C" {
void fp32_kernel(
    const unsigned int operation,
    const unsigned int n_blocks,
    const float* in_fp32_a,
    const float* in_fp32_b,
    float* out_fp32
) {

#pragma HLS INTERFACE s_axilite port=operation
#pragma HLS INTERFACE s_axilite port=n_blocks
#pragma HLS INTERFACE s_axilite port=return

  // Interface pragmas
    #pragma HLS INTERFACE m_axi port=in_fp32_a offset=slave bundle=gmem0 \
        depth=64 max_read_burst_length=16 num_read_outstanding=4

    #pragma HLS INTERFACE m_axi port=in_fp32_b offset=slave bundle=gmem1 \
        depth=64 max_read_burst_length=16 num_read_outstanding=4

    #pragma HLS INTERFACE m_axi port=out_fp32 offset=slave bundle=gmem2 \
        depth=64 max_write_burst_length=16 num_write_outstanding=4

    const unsigned int total = n_blocks * N;

    // ==================== LOAD A ====================
    float A[N];
    float B[N];
    float Z[N];

LOAD_A:
    for (int blk = 0; blk < n_blocks; blk++) {
        const unsigned int offset = blk * N;

        for (int i = 0; i < N; i++) {
#pragma HLS PIPELINE II=1
            A[i] = in_fp32_a[offset + i];
        }

        for (int i = 0; i < N; i++) {
#pragma HLS PIPELINE II=1
            B[i] = in_fp32_b[offset + i];
        }

        switch (operation) {
            case OP_ADD: fp32_add_blocks<N>(A, B, Z); break;
            case OP_SUB: fp32_sub_blocks<N>(A, B, Z); break;
            case OP_MUL: fp32_mul_blocks<N>(A, B, Z); break;
            case OP_DIV: fp32_div_blocks<N>(A, B, Z); break;
            case OP_RCP: fp32_rcp_blocks<N>(B, Z); break;
            default:     fp32_add_blocks<N>(A, B, Z); break;
        }

        for (int i = 0; i < N; i++) {
#pragma HLS PIPELINE II=1
            out_fp32[offset + i] = Z[i];
        }
    }
}
}
