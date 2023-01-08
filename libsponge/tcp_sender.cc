#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _retransmission_timeout{retx_timeout} {}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _ackno; }

void TCPSender::fill_window() {
    if (_fin_sent)
        return;

    // 计算可以发送的数据大小，有next_seqno - ackno的数据在路上，因此最多可以发_rwnd -  (next_seqno - ackno)
    size_t payload_size = (_rwnd == 0 ? 1 : _rwnd) - (next_seqno_absolute() - _ackno);
    while (payload_size && !_fin_sent) {
        TCPSegment seg;

        TCPHeader &header = seg.header();  // 构造TCP报文头
        if (next_seqno_absolute() == 0) {  // 占用SYN
            payload_size -= 1;
            header.syn = true;
        }
        header.seqno = next_seqno();

        Buffer &buffer = seg.payload();  // 构造TCP报文载荷
        buffer = stream_in().read(min(TCPConfig::MAX_PAYLOAD_SIZE, payload_size));
        payload_size -= buffer.size();

        if (payload_size > 0 && stream_in().eof()) {  // 占用FIN
            header.fin = true;
            _fin_sent = true;
            payload_size -= 1;
        }

        // 如果seg包含任何内容，则发送该报文
        size_t seg_len = seg.length_in_sequence_space();
        if (seg_len) {
            segments_out().push(seg);
            _outstanding_segments.push(seg);
            _next_seqno += seg_len;
            if (!_timer.timing())
                _timer.start();
        } else
            break;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _rwnd = window_size;  // 根据测试代码，rwnd总是更新，不需要判断abs_ackno与_ackno的大小关系

    uint64_t abs_ackno = unwrap(ackno, _isn, next_seqno_absolute());
    if (abs_ackno > next_seqno_absolute() || abs_ackno <= _ackno)
        return;

    // 以下为abs_ackno > _ackno，即有新的seg被确认的情况
    _ackno = abs_ackno;
    while (!_outstanding_segments.empty()) {
        TCPSegment seg = _outstanding_segments.front();
        size_t seg_len = seg.length_in_sequence_space();
        if (unwrap(seg.header().seqno, _isn, next_seqno_absolute()) + seg_len <= _ackno)
            _outstanding_segments.pop();
        else
            break;
    }
    _retransmission_timeout = _initial_retransmission_timeout;
    if (!_outstanding_segments.empty())
        _timer.start();
    _consecutive_retransmissions = 0;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer.update(ms_since_last_tick);
    if (!_timer.expired(_retransmission_timeout))
        return;
    // 超时重传
    if (_rwnd) {
        _consecutive_retransmissions += 1;
        _retransmission_timeout <<= 1;
    }
    if (!_outstanding_segments.empty())
        segments_out().push(_outstanding_segments.front());
    _timer.start();
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().ackno = next_seqno();
    _segments_out.push(seg);
}
