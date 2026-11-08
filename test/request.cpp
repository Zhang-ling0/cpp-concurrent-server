#include<iostream>
#include<regex>
#include<string>
int main()
{
    //HTTP请求行格式：  GET /bitejiuyeke/login?user=xiaoming&pass=123123 HTTP/1.1\r\n
    //HTTP请求行格式：  GET /bitejiuyeke/login? HTTP/1.1\r\n
    //std::string str = "GET /bitejiuyeke/login?user=xiaoming&pass=123123 HTTP/1.1\r\n";
    std::string str = "GET /bitejiuyeke/ HTTP/1.1\r\n";
    std::regex e("(GET|POST|PUT|DELETE ) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)?",std::regex::icase);
    std::smatch matchs;//匹配结果对象
    //定义好了规则，字符串，和匹配结果的对象，下面就可以进行匹配了
    bool ret = std::regex_search(str,matchs,e);
    if(ret==false)
    {
        std::cout<<"match error"<<std::endl;
    }

    std::string method = matchs[1];
    std::transform(method.begin(),method.end(),method.begin(),::toupper);
    std::cout<<"method:"<<method<<std::endl;
    for(int i=0;i<matchs.size();i++)
    {
        std::cout<<i<<":";
        std::cout<<matchs[i]<<std::endl;
    }
    return 0;
}

