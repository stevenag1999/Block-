/**
 * Common definitions for BFP HW/SW interface
 * ADAPTED for compact format with delta (HW: WE=5, WM=7)
 */

#ifndef COMMON_BFP_H
#define COMMON_BFP_H

#include <cstdint>

// BFP Configuration - Must match HW kernel
#define WE 5
#define WM 7
#define N  16

// Compact format size: 1 exp_shared + 3*N (sign, mant, delta per element)
#define BFP_BLOCK_SIZE (1 + 3 * N)  // 49 for N=16

// Operation codes - Must match bfp_kernel.cpp enum
typedef enum : unsigned int {
    OP_ENCODE = 0,
    OP_DECODE = 1,
    OP_ADD    = 2,
    OP_SUB    = 3,
    OP_MUL    = 4,
    OP_DIV    = 5,
    OP_RCP    = 6
} bfp_op_t;

// Operation names for display
static const char* OP_NAMES[] = {
    "ENCODE",
    "DECODE",
    "ADD",
    "SUB",
    "MUL",
    "DIV",
    "RCP"
};

// Helper: Pack BFP data into compact format for HW
// Format: [exp_shared, sign[0], mant[0], delta[0], sign[1], mant[1], delta[1], ...]
inline void pack_bfp_to_compact(
    uint32_t exp_shared,
    const uint32_t* sign,
    const uint32_t* mant,
    const uint32_t* delta,
    uint32_t* compact_buf,
    uint32_t offset
) {
    compact_buf[offset] = exp_shared;
    uint32_t idx = offset + 1;
    
    for (int i = 0; i < N; i++) {
        compact_buf[idx++] = sign[i];
        compact_buf[idx++] = mant[i];
        compact_buf[idx++] = delta[i];
    }
}

// Helper: Unpack compact format from HW to separate arrays  
inline void unpack_compact_to_bfp(
    const uint32_t* compact_buf,
    uint32_t offset,
    uint32_t& exp_shared,
    uint32_t* sign,
    uint32_t* mant,
    uint32_t* delta
) {
    exp_shared = compact_buf[offset];
    uint32_t idx = offset + 1;
    
    for (int i = 0; i < N; i++) {
        sign[i]  = compact_buf[idx++];
        mant[i]  = compact_buf[idx++];
        delta[i] = compact_buf[idx++];
    }
}

#endif // COMMON_BFP_H
