#include "../source/server.hpp"

int main()
{
    Socket server_sock;
    // 启动服务端，监听 8500 端口（对应你的客户端）
    if (server_sock.CreateServer(8500) == false) {
        ERR_LOG("服务端启动失败");
        return -1;
    }
    DBG_LOG("服务端已启动，监听 8500 端口...");

    while (1)
    {
        // 获取新连接
        int client_fd = server_sock.Accept();
        if (client_fd < 0) {
            usleep(1000); // 非阻塞模式下没连接时，稍微等一下防 CPU 空转
            continue;
        }
        
        DBG_LOG("客户端已连接，fd: %d", client_fd);
        
        // 管理客户端连接
        Socket client_sock(client_fd);
        char buf[1024];
        
        while (1)
        {
            memset(buf, 0, sizeof(buf));
            // 接收数据
            ssize_t ret = client_sock.Recv(buf, 1023);
            
            if (ret > 0) {
                DBG_LOG("收到: %s", buf);
                // 原样发回去
                client_sock.Send(buf, ret);
            } else if (ret == 0) {
                // 在阻塞模式下（如果客户端断开），或者非阻塞下暂时没数据
                usleep(1000); 
            } else {
                // 真的出错了（客户端断开）
                break; 
            }
        }
        DBG_LOG("客户端断开连接");
    }
    return 0;
}