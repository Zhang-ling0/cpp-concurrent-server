#include<iostream>
#include<string>
#include<functional>
void print(const std::string& str,int num)
{
    std::cout << str << " " << num << std::endl;
}
int main()
{
   auto Print=bind(print,"hhh",std::placeholders::_1);
   Print(10);
   return 0;
}