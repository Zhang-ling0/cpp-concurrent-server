#include "server.hpp"


int main()
{
    //验证是否能正确扩容
    Buffer buf;
    for(int i=0;i<300;i++)
    {
        std::string str = "hello!"+std::to_string(i)+'\n';
        buf.WriteStringAndPush(str);
    }
    std::cout << "可读数据大小: " << buf.ReadAbleSize() << std::endl;
    while(buf.ReadAbleSize()>0)
    {
        std::string line=buf.GetLineAndPop();//将数据按行取出
        INF_LOG("hello");
    }
    // std::string tmp;
    // tmp = buf.ReadAsStringAndPop(buf.ReadAbleSize());
    // std::cout<<tmp<<std::endl;
    //验证是否能正确读写
    // std::string str="hello!";
    // buf.WriteStringAndPush(str);

    // Buffer buf1;
    // buf1.WriteBufferAndPush(buf);
    // std::string tmp;
    // tmp = buf1.ReadAsStringAndPop(buf.ReadAbleSize());
    // std::cout<<tmp<<std::endl;
    // std::cout<<buf.ReadAbleSize()<<std::endl;
    // std::cout<<buf1.ReadAbleSize()<<std::endl;
    return 0;
}