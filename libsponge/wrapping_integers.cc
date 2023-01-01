#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    return isn + static_cast<uint32_t>(n);
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    ///*
    // 1. 找到最小的合法abs seqno，合法的abs_seqno包括：a, a + 2^32, ...
    // 等价于：min_seqno = static_cast<uint64_t>(signed_n + signed_isn + UINT32_MAX + 1);
    uint32_t min_seqno = n.raw_value() - isn.raw_value();
    
    // 2. 调整abs seqno使其接近checkpoint
    // 计算min_seqno与checkpoint的距离，实际上只需要关注checkpoint的低32位
    const uint32_t checkpoint_32 = static_cast<uint32_t>(checkpoint);
    const uint32_t offset = min_seqno - checkpoint_32;

    if (offset <= (1U << 31)) {  // abs_seqno应该在检查点的右侧
        return checkpoint + offset;
    } else {  // abs_seqno应该在检查点的左侧
        // 此时需要考虑checkpoint与((1UL << 32) - offset)的大小关系
        return (checkpoint >= ((1UL << 32) - offset)) ? checkpoint - ((1UL << 32) - offset) : checkpoint + offset;
    }
}
