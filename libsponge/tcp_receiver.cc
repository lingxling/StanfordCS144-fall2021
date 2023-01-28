#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader &header = seg.header();
    // 如果seg不是SYN报文，且没有提供新信息，则丢弃该报文
    if (_syn_rcv && header.seqno.raw_value() + seg.length_in_sequence_space() <= ackno().value().raw_value())
        return;
    if (!_syn_rcv) {
        if (!header.syn)
            return;
        _isn = header.seqno;
        _syn_rcv = true;
    }
    uint64_t stream_idx = unwrap(header.seqno + header.syn, _isn, stream_out().bytes_written()) - 1;
    _reassembler.push_substring(seg.payload().copy(), stream_idx, header.fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (_syn_rcv == false)
        return nullopt;
    return wrap(stream_out().bytes_written() + 1 + stream_out().input_ended(), _isn);
}

size_t TCPReceiver::window_size() const { return stream_out().remaining_capacity(); }
