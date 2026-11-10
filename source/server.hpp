#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <pthread.h>
#include <algorithm> // 使用 std::copy 需要包含此头文件
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <csignal>
#include <functional>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <thread>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <sys/timerfd.h>
#define INF 0
#define DBG 1
#define ERR 2
#define LOG_LEVEL DBG

#define LOG(level, format, ...)                                                                                        \
    do                                                                                                                 \
    {                                                                                                                  \
        if (level < LOG_LEVEL)                                                                                         \
            break;                                                                                                     \
        time_t t = time(NULL);                                                                                         \
        struct tm *ltm = localtime(&t);                                                                                \
        char tmp[32] = {0};                                                                                            \
        strftime(tmp, 31, "%H:%M:%S", ltm);                                                                            \
        fprintf(stdout, "[%p %s %s:%d] " format "\n", (void *)pthread_self(), tmp, __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)

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
        if (len == 0)
            return;
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
        if (TailIdleSize() >= len)
            return;

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
        if (len == 0)
            return;
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

#define MAX_LISTEN 1024
class Socket
{
private:
    int _sockfd;

public:
    Socket()
        : _sockfd(-1) {};
    Socket(int fd)
        : _sockfd(fd) {};
    ~Socket()
    {
        Close();
    };
    int Fd() { return _sockfd; }
    // 1. 创建套接字
    bool Create()
    {
        // int socket(int domain,int type,int protocol)
        _sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (_sockfd < 0)
        {
            ERR_LOG("create sockfd failed!");
            return false;
        }
        return true;
    }
    // 2. 绑定地址信息
    bool Bind(const std::string ip, uint16_t port)
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        socklen_t len = sizeof(struct sockaddr_in);
        // int bind(int sockfd,struct sockaddr* addr,sockleb_t len);
        int ret = bind(_sockfd, (struct sockaddr *)&addr, len);
        if (ret < 0)
        {
            ERR_LOG("bind address error");
            return false;
        }
        return true;
    }
    // 3. 开始监听
    bool Listen(int backlog = MAX_LISTEN) // 并发监听数
    {
        // int listen(int backlog)
        int ret = listen(_sockfd, backlog);
        if (ret < 0)
        {
            ERR_LOG("listen sockfd error");
            return false;
        }
        return true;
    }
    // 4. 向服务器发起连接
    bool Connect(const std::string ip, uint16_t port)
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        socklen_t len = sizeof(struct sockaddr_in);
        // int ret = connect(int sockfd,struct sockaddr* addr,socklen_t len);
        int ret = connect(_sockfd, (struct sockaddr *)&addr, len);
        if (ret < 0)
        {
            ERR_LOG("connect sockfd error");
            return false;
        }
        return true;
    }
    // 5. 获取新连接
    int Accept()
    {
        // int accept(int sockfd,struct sockaddr* addr,socklen* len)
        int newfd = accept(_sockfd, NULL, NULL);
        if (newfd < 0)
        {
            ERR_LOG("accept sockfd error");
            return -1;
        }
        return newfd;
    }
    // 6. 接收数据
    ssize_t Recv(void *buf, size_t len, int flag = 0)
    {
        // size_t recv(int sockfd,void* buf,size_t len,int flag);
        ssize_t ret = recv(_sockfd, buf, len, flag);
        if (ret < 0)
        {
            // EAGAIN当前socket的接收缓冲区中没有数据了。在非阻塞的情况下才会出现这样的情况
            // EINTR 表示当前sockfd的阻塞等待，被信号打断了
            if (errno == EAGAIN || errno == EINTR)
            {
                return 0; // 表示这次没有接受到数据
            }
            ERR_LOG("SOCKET RECV FAILED!!");
            return -1;
        }
        return ret; // 实际接受到数据的长度
    }
    ssize_t NonBlockRecv(void *buf, size_t len)
    {
        return Recv(buf, len, MSG_DONTWAIT); // msg_dontwait表示当前接受为非阻塞
    }
    // 7. 发送数据
    ssize_t Send(const void *buf, size_t len, int flag = 0)
    {
        // ssize_t ret=send(_sockfd,void* buf,ssize_t len;int flag);
        ssize_t ret = send(_sockfd, buf, len, flag);
        if (ret < 0)
        {
            if (errno == EAGAIN || errno == EINTR)
            {
                return 0;
            }
            ERR_LOG("SOCKET SEND FAILED!!");
            return -1;
        }
        return ret; // 实际发送的数据长度
    }
    ssize_t NoneBlockSend(const void *buf, size_t len, int flag = 0)
    {
        return Send(buf, len, MSG_DONTWAIT); // 非阻塞等待
    }
    // 8. 关闭套接字
    void Close()
    {
        if (_sockfd != -1)
        {
            close(_sockfd);
            _sockfd = -1;
        }
    }
    // 9. 创建一个服务端连接
    bool CreateServer(uint16_t port, const std::string &ip = "0.0.0.0", bool block_flag = false)
    {
        // 1. 创建套接字 2.绑定地址 3. 开始监听 4. 设置非阻塞 5.启动地址重用
        if (Create() == false)
            return false;
        if (block_flag)
            NonBlock();
        ReuseAddress();
        if (Bind(ip, port) == false)
            return false;
        if (Listen() == false)
            return false;
        return true;
    }
    // 10. 创建一个客户端连接
    bool CreateClient(uint16_t port, const std::string &ip)
    {
        // 1. 创建套接字 2.指向连接服务器
        if (Create() == false)
            return false;
        if (Connect(ip, port) == false)
            return false;
        return true;
    }
    // 11.设置套接字选项--开启地址端口重用 1. 地址重用 2. 端口重用
    void ReuseAddress()
    {
        // int setsockopt(int fd,int leve,int optname,void* val,int vallen);
        // 给指定的套接字（fd）在特定的协议层级（level）上，设置或修改某个选项（optname）的值（val），值的长度由 vallen 决定。
        //  在 SOL_SOCKET 层级上，为 fd 设置 SO_REUSEADDR 选项，值为 1（开启），大小为 sizeof(int)
        int val = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&val, sizeof(int));
        val = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, (void *)&val, sizeof(int));
    }
    // 12.设置套接字阻塞属性--设置为非阻塞
    void NonBlock()
    {
        // int fcntl(int fd, int cmd, ... /* arg */ );
        // 先获取套接字当前的文件状态标志（F_GETFL），然后在保留原有标志的基础上，
        //  追加非阻塞属性（O_NONBLOCK），最终将其重新设置回去（F_SETFL）。
        int flag = fcntl(_sockfd, F_GETFL, 0);
        fcntl(_sockfd, F_SETFL, flag | O_NONBLOCK);
    }
};

class Poll;
class EventLoop;
class Channel
{
private:
    int _fd;
    EventLoop *_loop;
    uint32_t _events;  // 当前需要监控的事件
    uint32_t _revents; // 当前连接触发的事件
    using EventCallback = std::function<void()>;
    EventCallback _read_callback;  // 可读事件被触发的回调函数
    EventCallback _write_callback; // 可写事件被触发的回调函数
    EventCallback _error_callback; // 错误事件被触发的回调函数
    EventCallback _close_callback; // 连接断开事件被触发的回调函数
    EventCallback _event_callback; // 任意事件事件被触发的回调函数
public:
    Channel(EventLoop *loop, int fd)
        : _fd(fd), _events(0), _revents(0), _loop(loop)
    {
    }
    int FD()
    {
        return _fd;
    }
    uint32_t Events() { return _events; }                   // 获取想要监控的事件
    void SetRevents(uint32_t events) { _revents = events; } // 设置实际就绪的事件
    void SetReadCallback(const EventCallback &cb) { _read_callback = cb; }
    void SetWriteCallback(const EventCallback &cb) { _write_callback = cb; }
    void SetErrorCallback(const EventCallback &cb) { _error_callback = cb; }
    void SetCloseCallback(const EventCallback &cb) { _close_callback = cb; }
    void SetEventCallback(const EventCallback &cb) { _event_callback = cb; }
    // 当前是否监控了可读
    bool ReadAble() { return (_events & EPOLLIN); }
    // 当前是否监控了可写
    bool WRITEAble() { return (_events & EPOLLOUT); }
    // 启动读事件监控
    void EnableRead()
    {
        _events |= EPOLLIN;
        Update();
    } // 添加
    // 启动写事件监控
    void EnableWrite()
    {
        _events |= EPOLLOUT;
        Update();
    }
    // 关闭读事件
    void DisableRead()
    {
        _events &= ~EPOLLIN;
        Update();
    } // 去掉
    // 关闭写事件
    void DisableWrite()
    {
        _events &= ~EPOLLOUT;
        Update();
    }
    // 移除监控
    void Remove(); // 后面会调用EventLoop接口来移除监控
    // 事件处理，一旦连接了触发事件，就调用这个函数，自己触发了什么事件如何处理你自己决定
    void Update();
    void HandleEvent()
    {
        if ((_revents & EPOLLIN) || (_revents & EPOLLRDHUP) || (_revents & EPOLLPRI))
        {
            if (_read_callback)
                _read_callback();
            // 不管任何事情都调用回调函数
            if (_event_callback)
                _event_callback();
        }
        // 有可能会释放连接的操作事件，一次只能处理一个
        if (_revents & EPOLLOUT)
        {
            if (_write_callback)
                _write_callback();
            // 不管任何事情都调用回调函数
            if (_event_callback)
                _event_callback(); // 放到事件处理完毕后调用，刷新活跃度
        }
        else if (_revents & EPOLLERR)
        {
            if (_error_callback)
                _error_callback(); // 一旦出错，就会释放连接，因此要放到前面调用任意回调
            if (_event_callback)
                _event_callback();
        }
        if (_revents & EPOLLHUP)
        {
            if (_close_callback)
                _close_callback();
            if (_event_callback)
                _event_callback();
        }
    }
};

#define MAX_EPOLLEVENTS 1024
class Poller
{
private:
    int _epfd;
    struct epoll_event _evs[MAX_EPOLLEVENTS];
    std::unordered_map<int, Channel *> _channels;

private:
    // 对epoll直接进行操作
    void Update(Channel *channel, int op)
    {
        // int epoll_ctl(int epfd,int op,int fd,struct epoll_event *ev)
        int fd = channel->FD();
        struct epoll_event ev;
        ev.data.fd = fd;
        ev.events = channel->Events();
        int ret = epoll_ctl(_epfd, op, fd, &ev);
        if (ret < 0)
        {
            ERR_LOG("EPOLLCTL FAILED!");
        }
        return;
    }
    // 判断一个Channel是否已经添加了事件jiank
    bool HasChannel(Channel *channel)
    {
        auto it = _channels.find(channel->FD()); // 检测任务队列里是否存在该任务的文件描述符
        if (it == _channels.end())
        {
            return false;
        }
        return true;
    }

public:
    Poller()
    {
        _epfd = epoll_create(MAX_EPOLLEVENTS);
        if (_epfd < 0)
        {
            ERR_LOG("epoll create failed!");
            abort(); // 退出程序
        }
    }
    // 添加或者修改监控事件
    void UpdateEvent(Channel *channel)
    {
        bool ret = HasChannel(channel);
        if (ret == false)
        {
            // 不存在则添加
            _channels.insert(std::make_pair(channel->FD(), channel));
            return Update(channel, EPOLL_CTL_ADD);
        }
        return Update(channel, EPOLL_CTL_MOD);
    }
    // 移除监控
    void RemoveEvent(Channel *channel)
    {
        auto it = _channels.find(channel->FD());
        if (it != _channels.end())
        {
            _channels.erase(it);
        }
        Update(channel, EPOLL_CTL_DEL);
    }
    // 开始监控，返回活跃连接
    void Poll(std::vector<Channel *> *active)
    {
        // int epoll_wait(int epfd,struct epoll_event *evs,int maxevents,int timeout)
        int nfds = epoll_wait(_epfd, _evs, MAX_EPOLLEVENTS, -1);
        if (nfds < 0)
        {
            if (errno == EINTR)
            {
                return;
            }
            ERR_LOG("epoll wait error:%s\n", strerror(errno));
            abort(); // 退出程序
        }
        for (int i = 0; i < nfds; i++)
        {
            auto it = _channels.find(_evs[i].data.fd);
            assert(it != _channels.end());
            it->second->SetRevents(_evs[i].events); // 设置实际就绪事件
            active->push_back(it->second);
        }
        return;
    }
};
using TaskFunc = std::function<void()>;
using ReleaseFunc = std::function<void()>;

class TimerTask
{
public:
    TimerTask(uint64_t id, uint32_t timeout, const TaskFunc &cb)
        : _id(id), _timeout(timeout), _task_cb(cb), _canceled(false) {}

    // 设置清理回调
    void SetRelease(const ReleaseFunc &cb) { _release = cb; }
    uint32_t DelayTime() { return _timeout; }
    // 取消任务
    void Cancel() { _canceled = true; }
    ~TimerTask()
    {
        // 如果没有被取消，才执行定时任务
        if (_canceled == false)
            _task_cb();
        // 无论取不取消，都要把映射表里的自己删掉
        _release();
    }

private:
    uint64_t _id;         // 定时器任务对象ID
    uint32_t _timeout;    // 定时任务超时事件
    bool _canceled;       // false表示没有被取消 true表示被取消
    TaskFunc _task_cb;    // 定时器对象要执行的定时任务
    ReleaseFunc _release; // 用于删除TimeWheel中保存的定时器对象信息
};

class TimeWheel
{
private:
    using PtrTask = std::shared_ptr<TimerTask>;//管理任务对象的生死
    using WeakTask = std::weak_ptr<TimerTask>;//用于刷新任务的时候临时借调PtrTask

    int _tick;     // 当前的秒针，走到哪里释放哪里，就相当于执行哪里的任务
    int _capacity; // 表盘最大数量--最大延迟时间
    std::vector<std::vector<PtrTask>> _wheel;
    std::unordered_map<uint64_t, WeakTask> _timers;//int64_t指的是任务ID，WeakTask是任务，用于任务查找的

    EventLoop *_loop;
    int _timerfd; // 定时器描述符--可读事件回调就是读取计数器，执行定时任务
    std::unique_ptr<Channel> _timer_channel;

private:
    void RemoveTimer(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it != _timers.end())
        {
            _timers.erase(it);
        }
    }
    // 创建事件文件描述符
    static int CreateTimerfd()
    {
        int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);//绝对时间，不受认为i修改系统时间的影响
        if (timerfd < 0)
        {
            ERR_LOG("timerfd create failed!");
            abort();
        }
        // int timerfd_settime(int fd, int flags, struct itimerspec *new, struct itimerspec *old);
        struct itimerspec itime;
        //it_value：首次超时时间 it_interval：后续间隔时间
        //tv_sec：秒 tv_nsec：纳秒
        itime.it_value.tv_sec = 1;
        itime.it_value.tv_nsec = 0; // 第一次超时事件为1s后
        itime.it_interval.tv_sec = 1;
        itime.it_interval.tv_nsec = 0; // 第一次超时后，每次超时的时间间隔
        timerfd_settime(timerfd, 0, &itime, NULL);
        return timerfd;
    }
    // 读时间
    int ReadTimefd()
    {
        uint64_t times;
        // 有可能因为其他描述符的事件处理花费事件比较长，然后在处理定时器描述符事件的时候，有可能就已经超时了很多次
        // read读取到的数据times就是从上一次read之后超时的次数
        int ret = read(_timerfd, &times, 8);
        if (ret < 0)
        {
            ERR_LOG("READ TIMEFD FAILED!");
            abort();
        }
        return times;
    }
    // 这个函数应该每秒钟被执行一次，相当于秒针向后走了一步
    void RunTimerTask()
    {
        // 清空指定位置的数组，就会把数组中保存的所有管理定时器对象的shared_ptr释放掉
        _tick = (_tick + 1) % _capacity;
        _wheel[_tick].clear();
    }
    void OnTime()
    {
        // 根据实际超时的次数，执行对应的超时任务
        int times = ReadTimefd();
        for (int i = 0; i < times; i++)
        {
            RunTimerTask();
        }
    }
    // 添加到事件循环中
    void TimerAddInLoop(uint64_t id, uint32_t delay, const TaskFunc &cb)
    {
        PtrTask pt(new TimerTask(id, delay, cb));
        pt->SetRelease(std::bind(&TimeWheel::RemoveTimer, this, id));
        int pos = (_tick + delay) % _capacity;
        _wheel[pos].push_back(pt);
        _timers[id] = WeakTask(pt);
    }
    // 在事件循环中刷新
    void TimerRefreshInLoop(uint64_t id)
    {
        // 通过保存的定时器对象的weak_ptr构造一个shared_ptr出来，添加到轮子中
        auto it = _timers.find(id);
        if (it == _timers.end())
        {
            return; // 没找着定时任务，没法刷新，没法延迟
        }
        PtrTask pt = it->second.lock(); // lock获取weak_ptr管理的对象对应的shared_ptr
        int delay = pt->DelayTime();
        int pos = (_tick + delay) % _capacity;
        _wheel[pos].push_back(pt);
    }
    // 在事件循环中取消
    void TimerCancelInLoop(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it == _timers.end())
        {
            return; // 没找着定时任务，没法刷新，没法延迟
        }
        PtrTask pt = it->second.lock();
        if (pt)
            pt->Cancel();
    }

public:
    TimeWheel(EventLoop* loop) : _capacity(60), _tick(0), _wheel(_capacity),_loop(loop)
                                ,_timerfd(CreateTimerfd()),_timer_channel(new Channel(_loop,_timerfd)){
                                    _timer_channel->SetReadCallback(std::bind(&TimeWheel::OnTime,this));
                                    _timer_channel->EnableRead();//启动读事件
                                }
    //定时器中有个_timers成员，定时器信息的操作有可能在多线程中进行，因此需要考虑线程安全问题
    //如果不想加锁，那就把对定期的所有操作都放在一个线程中进行
    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb);
    //刷新/延迟定时任务                         
    void TimerRefresh(uint64_t id);
    void TimerCancel(uint64_t id);
    //这个接口存在线程安全问题--这个接口实际上不能被外界使用者调用，只能在模块内，在对应的EventLoop内执行
    bool HasTimer(uint64_t id)
    {
        auto it = _timers.find(id);
        if(it==_timers.end())
        {
            return false;
        }
        return true;
    }
};

class EventLoop
{
private:
    using Functor = std::function<void()>;
    std::thread::id _thread_id; // 线程ID
    int _event_fd;              // eventfd唤醒IO事件监控可能导致阻塞
    std::unique_ptr<Channel> _event_channel;
    Poller _poller;              // 进行所有描述符的事件监控
    std::vector<Functor> _tasks; // 任务池
    std::mutex _mutex;           // 实现任务池操作的线程安全
    TimeWheel _timer_wheel;     //定时器模块
public:
    // 执行任务池中的所有任务
    void RunAllTask()
    {
        std::vector<Functor> functor;
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            _tasks.swap(functor);
        }
        for (auto &f : functor)
        {
            f();
        }
        return;
    }
    // 创建文件描述符用于事件通知
    static int CreateEventFd()
    {
        int efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (efd < 0)
        {
            ERR_LOG("create event failed!");
            abort(); // 程序异常退出
        }
        return efd;
    }
    void ReadEventfd()
    {
        uint64_t res = 0;
        int ret = read(_event_fd, &res, sizeof(res));
        if (res < 0)
        {
            // EINTR -- 被信号打断；   EAGAIN -- 表示无数据可读
            if (errno == EINTR || errno == EAGAIN)
            {
                return;
            }
            ERR_LOG("READ EVENTFD FAILED!");
            abort();
        }
        return;
    }
    // 唤醒文件描述符
    void WeakEventFd()
    {
        uint16_t val = 1;
        int ret = write(_event_fd, &val, sizeof(val));
        if (ret < 0)
        {
            if (errno == EINTR)
            {
                return;
            }
            ERR_LOG("read eventfd failed!");
            abort();
        }
        return;
    }

public:
    EventLoop()
        : _thread_id(std::this_thread::get_id()), _event_fd(CreateEventFd()), _event_channel(new Channel(this, _event_fd)),_timer_wheel(this)
    {
        // 给eventfd添加可读事件回调函数，读取eventfd事件通知次数
        _event_channel->SetReadCallback(std::bind(&EventLoop::ReadEventfd, this));
        // 启动eventfd的读事件监控
        _event_channel->EnableRead();
    }
    // 事件监控->就绪事件处理->执行任务
    void Start()
    {
        while (1)
        {
            // 1.监控事件
            std::vector<Channel *> actives;
            _poller.Poll(&actives);
            // 2.就绪事件处理
            for (auto &channel : actives)
            {
                channel->HandleEvent();
            }
            // 3.执行任务
            RunAllTask();
        }
    }
    // 用于判断当前线程是否是EventLoop对应的线程
    bool IsInLoop()
    {
        return (_thread_id == std::this_thread::get_id());
    }
    void AssertInLoop()
    {
        assert(_thread_id == std::this_thread::get_id());
    }

    // 判断将要执行的任务是否处于当前线程中，如果是则执行，不是则压入队列
    void RunInLoop(const Functor &cb)
    {
        if (IsInLoop())
        {
            return cb();
        }
        return QueueInLoop(cb);
    }
    // 将操作压入任务池中
    void QueueInLoop(const Functor &cb)
    {
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            _tasks.push_back(cb);
        }
        // 唤醒有可能是因为事件没有就绪，而导致的epoll阻塞
        // 其实就是给eventfd写入一个数据，eventfd就会触发可读事件
        WeakEventFd();
    }
    // 添加/修改描述符的事件监控
    void UpdateEvent(Channel *channel) { return _poller.UpdateEvent(channel); }
    // 移除描述符监控
    void RemoveEvent(Channel *channel) { return _poller.RemoveEvent(channel); }
    void TimerAdd(uint64_t id,uint32_t delay,const TaskFunc &cb){return _timer_wheel.TimerAdd(id,delay,cb);}
    void TimerRefresh(uint64_t id) {return _timer_wheel.TimerRefresh(id);}
    void TimerCancel(uint64_t id) {return _timer_wheel.TimerCancel(id);}
    bool HasTimer(uint64_t id) {return _timer_wheel.HasTimer(id);}
};
// Channel 的实现必须放在 EventLoop 之后，因为内部调用了 EventLoop 的接口
void Channel::Update()
{
    _loop->UpdateEvent(this);
}

void Channel::Remove()
{
    _loop->RemoveEvent(this);
}
// 添加定时任务
void TimeWheel::TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb) {
    // 注意：这里假设 _loop 已经被正确初始化
    _loop->RunInLoop(std::bind(&TimeWheel::TimerAddInLoop, this, id, delay, cb));
}

// 刷新定时任务
void TimeWheel::TimerRefresh(uint64_t id) {
    _loop->RunInLoop(std::bind(&TimeWheel::TimerRefreshInLoop, this, id));
}

// 取消定时任务
void TimeWheel::TimerCancel(uint64_t id) {
    _loop->RunInLoop(std::bind(&TimeWheel::TimerCancelInLoop, this, id));
}