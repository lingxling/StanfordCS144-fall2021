#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _unassem_base(0), _unassem_buffer(), _is_eof(false),  _eof_idx(0), _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof) {
        _eof_idx = data.size() + index;  
        _is_eof = true;
    }
    // 如果data的index和unassem_buffer中的信息重合，则覆盖unassem_buffer
    for (size_t i = 0; i < data.size(); ++i) {
        if (_unassem_buffer.count(i + index)) _unassem_buffer[i + index] = data[i];
    }

    // 清理unassem_buffer中可以输出到_output的字符
    while (_unassem_buffer.count(_unassem_base) != 0 && _output.remaining_capacity() > 0) {
        _output.write(string(1, _unassem_buffer[_unassem_base]));
        _unassem_buffer.erase(_unassem_base);
        _unassem_base += 1;
    }
    size_t i = 0;
    while (i < data.size()) {
        if (i + index == _unassem_base) {  // 当前字符就是期望字符
            if (_output.remaining_capacity() > 0) { // 如果_output的buffer未满，则考虑将该字符输入到ByteStream中
                _output.write(string(1, data[i])); 
                _unassem_base += 1;
            }
            else if (_unassem_buffer.size() < _capacity) _unassem_buffer[i + index] = data[i];  // _output的buffer已满时，放在_unassem_buffer中
            
        } else if (i + index > _unassem_base && _unassem_buffer.size() < _capacity) {  // 如果是尚未重组的字符，则将其输入到unassembled buffer中
            _unassem_buffer[i + index] = data[i];
        }
        // 在当前字符放在_output、_unassem_buffer、丢弃后，考虑_unassem_buffer的剩余字符是否可以继续输入ByteStream
        while (_unassem_buffer.count(_unassem_base) != 0 && _output.remaining_capacity() > 0) {
            _output.write(string(1, _unassem_buffer[_unassem_base]));
            _unassem_buffer.erase(_unassem_base);
            _unassem_base += 1;
        }        
        ++i;
    }
    
    // 在目前的抽象层次上，可以认为所有的bytes终将到达（可能是乱序）
    if (_is_eof && _output.bytes_written() == _eof_idx) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassem_buffer.size(); }

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }
