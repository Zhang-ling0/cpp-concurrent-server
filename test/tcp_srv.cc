#include "../source/server.hpp"
//管理所有的连接
std::unordered_map<uint64_t, PtrConnection> _conns;
uint64_t conn_id = 0;
EventLoop loop;

void ConnectionDestroy(const PtrConnection &conn) {
    _conns.erase(conn->Id());
} 
void OnConnected(const PtrConnection &conn) {
    DBG_LOG("NEW CONNECTION:%p", conn.get());
}
void OnMessage(const PtrConnection &conn,Buffer* buf) {
    DBG_LOG("%s", buf->ReadPosition());
    buf->MoveReadOffset(buf->ReadAbleSize());
    std::string str = "Hello World";
    conn->Send(str.c_str(), str.size());
    //conn->Shutdown();
}


void Newconnection(int fd) {
    conn_id++;
    PtrConnection conn(new Connection(&loop, conn_id, fd));
    conn->SetMessageCallback(std::bind(OnMessage, std::placeholders::_1, std::placeholders::_2));
    conn->SetSrvClosedCallback(std::bind(ConnectionDestroy, std::placeholders::_1));
    conn->SetConnectedCallback(std::bind(OnConnected, std::placeholders::_1));
    conn->EnableInactiveRelease(10);//启动非活跃超时销毁
    conn->Established();//就绪初始化
    _conns.insert(std::make_pair(conn_id, conn));
}
int main()
{
    srand(time(NULL));
    //定义事件监控
    Acceptor acceptor(&loop,8500);
    //回调中，获取新连接，为新连接创建Channel并添加监控
    acceptor.SetAcceptCallback(std::bind(Newconnection,std::placeholders::_1));
    acceptor.Listen();
    //将事件启动
    loop.Start();
    return 0;
}
// int main()
// {
//     loop_pool = new LoopThreadPool(&base_loop);
//     loop_pool->SetThreadCount(2);
//     loop_pool->Create();
//     Acceptor acceptor(&base_loop, 8500);
//     acceptor.SetAcceptCallback(std::bind(NewConnection, std::placeholders::_1));
//     acceptor.Listen();   
//     base_loop.Start();
//     return 0;
// }
// //验证监控的移除
// void HandleClose(Channel* channel)
// {
//     std::cout<<"close: "<< channel->FD()<<std::endl;
//     channel->Remove();//移除监控
//     delete channel;
// }
// //验证读
//  void HandleRead(Channel* channel)
//  {
//     int fd = channel->FD();
//     char buf[1024]={0};
//     int ret=recv(fd,buf,1023,0);
//     if(ret<=0)
//     {
//         return HandleClose(channel);
//     }
//     DBG_LOG("%s",buf);
//     channel->EnableWrite();//启动可写事件
//  }
//  //验证写
//  void HandleWrite(Channel* channel)
//  {
//     int fd=channel->FD();
//     const char* data="哈哈哈哈哈";
//     int ret=send(fd,data,strlen(data),0);
//     if(ret<0)
//     {
//         return HandleClose(channel);
//     }
//     channel->DisableWrite(); 
//  }
// void HandleError(Channel* channel)
//  {
//     std::cout << "error: " << channel->FD() << std::endl;
//     HandleClose(channel);
//  }
//  void HandleEvent(EventLoop* loop,Channel *lst_channel,uint64_t timerid)
//  {
//       loop->TimerRefresh(timerid);
//  }
//  void Acceptor(EventLoop *loop,Channel *lst_channel)
//  {
//     int fd=lst_channel->FD();
//     int newfd=accept(fd,NULL,NULL);
//     if(newfd<0) {return;}
//    uint64_t timerid=rand()%10000;
  
//     Channel* channel = new Channel(loop,newfd);
//     channel->SetReadCallback(std::bind(&HandleRead,channel));
//     channel->SetWriteCallback(std::bind(&HandleWrite,channel));
//     channel->SetCloseCallback(std::bind(&HandleClose,channel));
//     channel->SetErrorCallback(std::bind(&HandleError,channel));
//     channel->SetEventCallback(std::bind(&HandleEvent,loop,channel,timerid));
//     loop->TimerAdd(timerid,10,std::bind(HandleClose,channel));
//     channel->EnableRead();
//  }

// int main()
// {
//     srand(time(NULL));
//     //定义事件监控
//     EventLoop eventloop;
//     //定义网络套接字
//     Socket lst_sock;
//     //创建服务端套接字

//     lst_sock.CreateServer(8500);
//     //为了监听套接字，创建Channel进行事件管理
//     Channel channel(&eventloop,lst_sock.Fd());
//     //回调中，获取新连接，为新连接创建Channel并添加监控
//     channel.SetReadCallback(std::bind(&Acceptor,&eventloop,&channel));
//     //将事件启动
//     channel.EnableRead();
//     eventloop.Start();
//     lst_sock.Close();
//     return 0;
// }