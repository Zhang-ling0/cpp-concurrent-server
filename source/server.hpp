#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <algorithm> // 使用 std::copy 需要包含此头文件

#define BUFFER_DEFAULT_SIZE 1024

class Buffer
{
private:
    std::vector<char> _buffer; // 使用vector进行内存空间管理
    uint64_t _reader_idx;      // 读偏移
    uint64_t _writer_idx;      // 写偏移

public:
    Buffer()
        : _reader_idx(0), _writer_idx(0), _buffer(BUFFER_DEFAULT_SIZE)
    {
    }

    // 获取缓冲区起始地址
    char *Begin() { return &*_buffer.begin(); }
    
    // 获取当前写入起始地址，_buffer的空间起始地址，加上写偏移量
    char *WritePosition() { return Begin() + _writer_idx; }
    
    // 获取当前读取的起始地址
    char *ReadPosition() { return Begin() + _reader_idx; }
    
    // 获取缓冲区末尾空闲大小--写偏移之后的空闲空间，总体空间大小减去写偏移
    uint64_t TailIdleSize() { return _buffer.size() - _writer_idx; }
    
    // 获取缓冲区起始空闲大小--读偏移之前的空闲空间
    uint64_t HeadIdleSize() { return _reader_idx; }
    
    // 获取可读数据的大小 = 写偏移 - 读偏移
    uint64_t ReadAbleSize() { return _writer_idx - _reader_idx; }

    // 将读偏移向后移动
    void MoveReadOffset(uint64_t len)
    {
        if (len == 0) return;
        // 向后移动的大小必须小于等于可读数据区的大小
        assert(len <= ReadAbleSize());
        _reader_idx += len;
    }

    // 将写偏移向后移动 (修正了原拼写错误 MoveWriteOffer -> MoveWriteOffset)
    void MoveWriteOffset(uint64_t len)
    {
        // 向后移动的大小，必须小于等于当前后面空闲空间的大小
        assert(len <= TailIdleSize());
        _writer_idx += len;
    }

    // 确保可写空间足够（整体空闲空间够了就移动数据，否则就扩容）
    void EnsureWriteSpace(uint64_t len)
    {
        // 1. 如果末尾空闲空间大小足够，直接返回
        if (TailIdleSize() >= len) return;
        
        // 2. 如果末尾空间不够，判断加上头部空闲空间是否足够，够了就将数据移动到起始位置
        if (len <= TailIdleSize() + HeadIdleSize())
        {
            // 将数据移动到起始位置
            uint64_t rsz = ReadAbleSize(); // 把当前数据的大小保存起来
            // 修正：原代码 std::copy(ReadAbleSize(), ...) 是错的，必须是 ReadPosition()
            std::copy(ReadPosition(), ReadPosition() + rsz, Begin());
            _reader_idx = 0;   // 将读偏移置为0
            _writer_idx = rsz; // 当前数据的大小就是写偏移的位置
        }
        else
        {
            // 3. 扩容，不移动数据，直接给偏移之后扩容足够空间即可
            _buffer.resize(_writer_idx + len);
        }
    }

    // ================== 写入数据相关接口 ==================

    // 写入数据（不移动写偏移）
    void Write(const void *data, uint64_t len)
    {
        if (len == 0) return;
        EnsureWriteSpace(len);
        const char *d = (const char *)data;
        std::copy(d, d + len, WritePosition());
    }

    // 写入数据并移动写偏移
    void WriteAndPush(const void *data, uint64_t len)
    {
        Write(data, len);
        MoveWriteOffset(len);
    }

    // 写入字符串（不移动写偏移）
    void WriteString(const std::string &data)
    {
        return Write(data.c_str(), data.size());
    }

    // 写入字符串并移动写偏移
    void WriteStringAndPush(const std::string &data)
    {
        WriteString(data);
        MoveWriteOffset(data.size());
    }

    // 向当前缓冲区写入另一个Buffer的数据
    void WriteBuffer(Buffer &data)
    {
        return Write(data.ReadPosition(), data.ReadAbleSize());
    }

    void WriteBufferAndPush(Buffer &data)
    {
        WriteBuffer(data);
        MoveWriteOffset(data.ReadAbleSize());
    }

    // ================== 读取数据相关接口 ==================

    // 读取数据（不移动读偏移）
    void Read(void *buf, uint64_t len)
    {
        assert(len <= ReadAbleSize());
        std::copy(ReadPosition(), ReadPosition() + len, (char *)buf);
    }

    // 读取数据并移动读偏移
    void ReadAndPop(void *buf, uint64_t len)
    {
        Read(buf, len);
        MoveReadOffset(len);
    }

    // 读取为字符串（不移动读偏移）
    std::string ReadAsString(uint64_t len)
    {
        assert(len <= ReadAbleSize());
        std::string str;
        str.resize(len);
        // 注意：C++11 之后 &str[0] 保证是连续内存，可以传入
        Read(&str[0], len);
        return str;
    }

    // 读取为字符串并移动读偏移（补充的接口）
    std::string ReadAsStringAndPop(uint64_t len)
    {
        assert(len <= ReadAbleSize());
        std::string str = ReadAsString(len);
        MoveReadOffset(len);
        return str;
    }

    // ================== 特殊处理接口 ==================

    // 快速查找第一个换行符 '\n' 的指针
    char *FindCRLF()
    {
        char *res = (char *)memchr(ReadPosition(), '\n', ReadAbleSize());
        return res;
    }

    // 获取一行数据（不移动读偏移）
    std::string GetLine()
    {
        char *pos = FindCRLF();
        if (pos == NULL)
        {
            return ""; // 没找到换行符，说明数据还不完整
        }
        // +1 是为了把换行字符也取出来
        return ReadAsString(pos - ReadPosition() + 1);
    }

    // 获取一行数据并移动读偏移
    std::string GetLineAndPop()
    {
        std::string str = GetLine();
        MoveReadOffset(str.size());
        return str;
    }

    // 清空缓冲区
    void Clear()
    {
        // 只需要将偏移量归0即可
        _reader_idx = 0;
        _writer_idx = 0;
    }
};