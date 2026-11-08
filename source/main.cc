#include "server.hpp"
int main()
{
    Buffer buf;
    std::string str="hello!";
    buf.WriteStringAndPush(str);
    std::string tmp;
    tmp = buf.ReadAsStringAndPop(buf.ReadAbleSize());
    std::cout<<tmp<<std::endl;
    std::cout<<buf.ReadAbleSize()<<std::endl;
    return 0;
}