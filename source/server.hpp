#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <pthread.h> 
#include <algorithm> // 使用 std::copy 需要包含此头文件
#include <sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include<csignal>
#include<function>
#include<syst/epoll.h>
#define INF 0
#define DBG 1
#define ERR 2
#define LOG_LEVEL DBG

#define LOG(level, format, ...) do{\
        if (level < LOG_LEVEL) break;\
        time_t t = time(NULL);\
        struct tm *ltm = localtime(&t);\
        char tmp[32] = {0};\
        strftime(tmp, 31, "%H:%M:%S", ltm);\
        fprintf(stdout, "[%p %s %s:%d] " format "\n", (void*)pthread_self(), tmp, __FILE__, __LINE__, ##__VA_ARGS__);\
    }while(0)

#define INF_LOG(format, ...) LOG(INF, format, ##__VA_ARGS__)
#define DBG_LOG(format, ...) LOG(DBG, format, ##__VA_ARGS__)
#define ERR_LOG(format, ...) LOG(ERR, format, ##__VA_ARGS__)
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

#define  MAX_LISTEN 1024
class Socket
{
private:
    int _sockfd;
public:
    Socket()
        :_sockfd(-1)
    {};
    Socket(int fd)
        :_sockfd(fd)
    {};
    ~Socket()
    {
        Close();
    };
    int Fd(){return _sockfd;}
    // 1. 创建套接字
    bool Create()
    {
        //int socket(int domain,int type,int protocol)
        _sockfd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
        if(_sockfd<0)
        {
            ERR_LOG("create sockfd failed!");
            return false;            
        }
        return true;
    }
    // 2. 绑定地址信息
    bool Bind(const std::string ip,uint16_t port )
    {
        struct sockaddr_in addr;
        addr.sin_family=AF_INET;
        addr.sin_port=htons(port);
        addr.sin_addr.s_addr=inet_addr(ip.c_str());
        socklen_t len =sizeof(struct sockaddr_in);
        //int bind(int sockfd,struct sockaddr* addr,sockleb_t len);
        int ret=bind(_sockfd,(struct sockaddr*)&addr,len);
        if(ret<0)
        {
            ERR_LOG("bind address error");
            return false;
        }
        return true;
    }
    // 3. 开始监听
    bool Listen(int backlog=MAX_LISTEN)//并发监听数
    {
        //int listen(int backlog)
        int ret=listen(_sockfd,backlog);
        if(ret<0)
        {
            ERR_LOG("listen sockfd error");
            return false;
        }
        return true;
    }
    // 4. 向服务器发起连接
    bool Connect(const std::string ip,uint16_t port)
    {
        struct sockaddr_in addr;
        addr.sin_family=AF_INET;
        addr.sin_port=htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        socklen_t len= sizeof(struct sockaddr_in);
        //int ret = connect(int sockfd,struct sockaddr* addr,socklen_t len);
        int ret = connect(_sockfd,(struct sockaddr*)& addr,len);
        if(ret<0)
        {
             ERR_LOG("connect sockfd error");
            return false;
        }
        return true;
    }
    // 5. 获取新连接
    int Accept()
    {
        //int accept(int sockfd,struct sockaddr* addr,socklen* len)
        int newfd=accept(_sockfd,NULL,NULL);
        if(newfd<0)
        {
            ERR_LOG("accept sockfd error");
            return -1;
        }
        return newfd;
    }
    // 6. 接收数据
    ssize_t Recv(void *buf,size_t len,int flag=0)
    {
        //size_t recv(int sockfd,void* buf,size_t len,int flag);
        ssize_t ret= recv(_sockfd,buf,len,flag);
        if(ret<0)
        {
            //EAGAIN当前socket的接收缓冲区中没有数据了。在非阻塞的情况下才会出现这样的情况
            //EINTR 表示当前sockfd的阻塞等待，被信号打断了
            if(errno==EAGAIN||errno==EINTR)
            {
                return 0;//表示这次没有接受到数据
            }
            ERR_LOG("SOCKET RECV FAILED!!");
            return -1;
        }
        return ret;//实际接受到数据的长度
    }
    ssize_t NonBlockRecv(void *buf,size_t len)
    {
        return Recv(buf,len,MSG_DONTWAIT);//msg_dontwait表示当前接受为非阻塞
    }
    // 7. 发送数据
    ssize_t Send(const void *buf,size_t len,int flag=0)
    {
        //ssize_t ret=send(_sockfd,void* buf,ssize_t len;int flag);
        ssize_t ret=send(_sockfd,buf,len,flag);
        if (ret < 0) {
                if (errno == EAGAIN || errno == EINTR) {
                    return 0;
                }
                ERR_LOG("SOCKET SEND FAILED!!");
                return -1;
            }
        return ret;//实际发送的数据长度
    }
    ssize_t NoneBlockSend(const void *buf,size_t len,int flag=0)
    {
        return Send(buf,len,MSG_DONTWAIT);//非阻塞等待
    }
    // 8. 关闭套接字
    void Close()
    {
        if(_sockfd !=-1)
        {
            close(_sockfd);
            _sockfd=-1;
        }
    }
    // 9. 创建一个服务端连接
    bool CreateServer(uint16_t port,const std::string &ip="0.0.0.0",bool block_flag=false)
    {
        //1. 创建套接字 2.绑定地址 3. 开始监听 4. 设置非阻塞 5.启动地址重用
        if(Create()==false) return false;
        if(block_flag) NonBlock();
        ReuseAddress();
        if(Bind(ip,port)==false) return false;
        if(Listen()==false) return false;
        return true;

    }
    // 10. 创建一个客户端连接
    bool CreateClient(uint16_t port,const std::string &ip)
    {
        //1. 创建套接字 2.指向连接服务器
        if(Create()==false) return false;
        if(Connect(ip,port)==false) return false;
        return true;
    }
    // 11.设置套接字选项--开启地址端口重用 1. 地址重用 2. 端口重用   
    void ReuseAddress()
    {
        //int setsockopt(int fd,int leve,int optname,void* val,int vallen);
        //给指定的套接字（fd）在特定的协议层级（level）上，设置或修改某个选项（optname）的值（val），值的长度由 vallen 决定。
        // 在 SOL_SOCKET 层级上，为 fd 设置 SO_REUSEADDR 选项，值为 1（开启），大小为 sizeof(int)
        int val=1;
        setsockopt(_sockfd,SOL_SOCKET,SO_REUSEADDR,(void*)& val,sizeof(int));
        val=1;
        setsockopt(_sockfd,SOL_SOCKET,SO_REUSEPORT,(void*)& val,sizeof(int));

    }            
    // 12.设置套接字阻塞属性--设置为非阻塞
    void NonBlock()
    {
        //int fcntl(int fd, int cmd, ... /* arg */ );
        //先获取套接字当前的文件状态标志（F_GETFL），然后在保留原有标志的基础上，
        // 追加非阻塞属性（O_NONBLOCK），最终将其重新设置回去（F_SETFL）。
        int flag = fcntl(_sockfd,F_GETFL,0);
        fcntl(_sockfd,F_SETFL,flag|O_NONBLOCK);
    }
};          
 
class Channel
{
    private:
        int _fd;
        EventLoop *_loop;
        uint32_t _events;//当前需要监控的事件
        uint32_t _revents;//当前连接触发的事件
        using EventCallback=std::function<void()>;
        EventCallback _read_callback;   //可读事件被触发的回调函数
        EventCallback _write_callback;  //可写事件被触发的回调函数
        EventCallback _error_callback;  //错误事件被触发的回调函数
        EventCallback _close_callback;  //连接断开事件被触发的回调函数
        EventCallback _event_callback;  //任意事件事件被触发的回调函数
    public:
        Channel(EventLoop *loop,int fd)
            :_fd(fd)
            ,_events(0)
            ,_revents(0)
            ,_loop(loop)
        {}
        int FD()
        {
            return _fd;
        }
        uint32_t Events(){return _events;}//获取想要监控的事件
        void SetRevents(uint32_t events){_revents=events;}//设置实际就绪的事件
        void SetReadCallback(const EventCallback &cb) {_read_callback=cb;}
        void SetWriteCallback(const EventCallback &cb) {_write_callback=cb;}
        void SetErrorCallback(const EventCallback &cb) {_error_callback=cb;}
        void SetCloseCallback(const EventCallback &cb) {_close_callback=cb;}
        void SetEventCallback(const EventCallback &cb) {_event_callback=cb;}
        //当前是否监控了可读
        bool ReadAble(){return (_events & EPOLLIN);}
        //当前是否监控了可写
        bool WRITEAble(){return (_events & EPOLLOUT);}
        //启动读事件监控
        void EnableRead(){_events |= EPOLLIN;}//添加
        //启动写事件监控
        void EnableWrite(){_events |= EPOLLOUT;}
        //关闭读事件
        void DisableRead(){_events &= ~EPOLLIN;}//去掉
        //关闭写事件
        void DisableWrite(){_events &= ~EPOLLOUT;}
        //移除监控
        void Remove();//后面会调用EventLoop接口来移除监控
        //事件处理，一旦连接了触发事件，就调用这个函数，自己触发了什么事件如何处理你自己决定
        void HandleEvent()
        {
            if((_revents&EPOLLIN)||(_revents&EPOLLRDHUP)||(_revents&EPOLLPRI))
            {
                if(_read_callback) _read_callback();
                //不管任何事情都调用回调函数
                if(_event_callback) _event_callback();
            }
            //有可能会释放连接的操作事件，一次只能处理一个
            if(_revents&EPOLLOUT)
            {
                if(_write_callback) _write_callback();
                //不管任何事情都调用回调函数
                if(_event_callback) _event_callback();//放到事件处理完毕后调用，刷新活跃度
            }
            else if(_revents&EPOLLERR)
            {
                if(_error_callback) _error_callback();//一旦出错，就会释放连接，因此要放到前面调用任意回调
                if(_event_callback) _event_callback();
            }
            if(_revents&EPOLLHUP)
            {
                if(_close_callback) _close_callback();
                if(_event_callback) _event_callback();
            }
        }
        ~Channel(){}
};