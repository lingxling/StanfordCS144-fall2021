#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    // 处理接收到的报文
    if (!active())
        return;
    _time_since_last_segment_received = 0;
    _receiver.segment_received(seg);

    // 如果remote发送和local接收的数据已经结束，那么就没必要linger
    if (_receiver.stream_out().input_ended() && !_sender.stream_in().eof())
        _linger_after_streams_finish = false;

    // if the RST flag is set, set the error flag on the inbound and outbound ByteStreams,
    // and any subsequent call to TCPConnection::active() should return false
    if (seg.header().rst) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _active = false;
        return;
    }

    // if the ACK flag is set, give that ackno (and window size) to the TCPSender
    if (seg.header().ack)
        _sender.ack_received(seg.header().ackno, seg.header().win);

    // if the incoming segment occupied any sequence numbers,
    // makes sure that at least one segment is sent in reply,
    // to reflect an update in the ackno and window size.
    if (seg.length_in_sequence_space() > 0) {
        _sender.fill_window();
        if (_sender.segments_out().size() == 0)
            _sender.send_empty_segment();  // 如果sender本身没有数据要发送，则sender额外生成一个ack报文
    }

    // extra special case, 额外生成ack报文，响应keep-alive报文
    if (_receiver.ackno().has_value() and (seg.length_in_sequence_space() == 0) and
        seg.header().seqno == _receiver.ackno().value() - 1)
        _sender.send_empty_segment();

    _send_segments();
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    // 在sender中写入数据并发送
    size_t write_size = _sender.stream_in().write(data);
    _sender.fill_window();
    _send_segments();
    return write_size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS)
        _unclean_shutdown();  // 关闭后不重传报文
    else if (_check_clean_shutdown_prereq())  // 检查是否已经结束
        _clean_shutdown();
    _send_segments();  // 发送报文，包括需要重传的报文
}

void TCPConnection::end_input_stream() {
    if (!active())
        return;  // 加个检查，避免非法调用
    // 构造FIN报文并发送
    _sender.stream_in().end_input();
    _sender.fill_window();
    _send_segments();
}

void TCPConnection::connect() {
    if (!active())
        return;  // 加个检查，避免非法调用
    // sender自动构造SYN报文并发送
    _sender.fill_window();
    _send_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _unclean_shutdown();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

bool TCPConnection::_check_clean_shutdown_prereq() {
    // Prereq 1, The inbound stream has been fully assembled and has ended
    if (!(_receiver.unassembled_bytes() == 0 && _receiver.stream_out().input_ended()))
        return false;

    // Prereq 2, The outbound stream has been ended by the local application and fully sent
    if (!(_sender.stream_in().eof() && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2))
        return false;

    // Prereq 3, The outbound stream has been fully acknowledged by the remote peer
    if (!(_sender.bytes_in_flight() == 0))
        return false;
    return true;
}

void TCPConnection::_clean_shutdown() {
    if (!_linger_after_streams_finish) 
        _active = false;
    if (_time_since_last_segment_received >= 10 * _cfg.rt_timeout)
        _active = false;
}

void TCPConnection::_unclean_shutdown() {
    if (!active())
        return;  // 加个检查，避免非法调用

    // 发送RST报文
    _sender.send_empty_segment();  // 生成一个有正确seqno的空报文
    TCPSegment rst_seg = _sender.segments_out().front();
    _sender.segments_out().pop();
    rst_seg.header().rst = true;          // 设置RST位
    if (_receiver.ackno().has_value()) {  // 设置ACK位，构造ackno
        rst_seg.header().ack = true;
        rst_seg.header().ackno = _receiver.ackno().value();
    }
    rst_seg.header().win = _receiver.window_size();
    _segments_out.push(rst_seg);

    // sender和receiver都置为error状态
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = false;
}

void TCPConnection::_send_segments() {
    if (!active())
        return;  // 加个检查，避免非法调用

    TCPSegment seg;
    while (!_sender.segments_out().empty()) {
        seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
        }
        seg.header().win = _receiver.window_size();
        _segments_out.push(seg);
    }
}