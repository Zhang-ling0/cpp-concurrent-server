#include<iostream>
#include<regex>
#include<string>
int main()
{
    std::string str = "hhhh123456";
    std::regex e("hhhh(\\d+)");//正则表达式，hhhh后面跟着的一个或者多个数数字
    std::smatch matchs;//匹配结果对象

    //定义好了规则，字符串，和匹配结果的对象，下面就可以进行匹配了
    bool ret =std::regex_match(str,matchs,e);
    if(ret==false)
    {
        std::cout<<"match error"<<std::endl;
    }
    for(auto &s:matchs)
    {
        std::cout<<s<<std::endl;
    }
}