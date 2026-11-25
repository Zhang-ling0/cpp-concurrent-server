#include <iostream>
#include <string>
#include <vector>
#include "http.hpp" // 确保包含 Util::Split 所在的头文件
//Split的测试用例
// int main() {
//     // 测试用例 1：正常情况（标准键值对）
//     {
//         std::vector<std::string> result;
//         size_t count = Util::Split("name=admin&pwd=123", "&", &result);
//         std::cout << "--- 测试用例 1 ---" << std::endl;
//         std::cout << "返回数量: " << count << std::endl;
//         for (auto &s : result) std::cout << "[" << s << "] ";
//         std::cout << std::endl;
//     }
//     // 测试用例 2：连续分隔符（测试情况 2 跳过空串）
//     {
//         std::vector<std::string> result;
//         size_t count = Util::Split("a&&b&&&c", "&", &result);
//         std::cout << "\n--- 测试用例 2 ---" << std::endl;
//         std::cout << "返回数量: " << count << std::endl;
//         for (auto &s : result) std::cout << "[" << s << "] ";
//         std::cout << std::endl;
//     }

//     // 测试用例 3：首尾都有分隔符（测试边界）
//     {
//         std::vector<std::string> result;
//         size_t count = Util::Split("&hello world&", "&", &result);
//         std::cout << "\n--- 测试用例 3 ---" << std::endl;
//         std::cout << "返回数量: " << count << std::endl;
//         for (auto &s : result) std::cout << "[" << s << "] ";
//         std::cout << std::endl;
//     }

//     // 测试用例 4：完全没有分隔符（测试情况 1）
//     {
//         std::vector<std::string> result;
//         size_t count = Util::Split("justoneword", "&", &result);
//         std::cout << "\n--- 测试用例 4 ---" << std::endl;
//         std::cout << "返回数量: " << count << std::endl;
//         for (auto &s : result) std::cout << "[" << s << "] ";
//         std::cout << std::endl;
//     }

//     // 测试用例 5：空字符串
//     {
//         std::vector<std::string> result;
//         size_t count = Util::Split("", "&", &result);
//         std::cout << "\n--- 测试用例 5 ---" << std::endl;
//         std::cout << "返回数量: " << count << std::endl;
//         for (auto &s : result) std::cout << "[" << s << "] ";
//         std::cout << std::endl;
//     }
//     return 0;
// }

//ReadFile测试
// int main()
// {
//     std::string buf;
//     bool test=Util::ReadFile("text.txt",&buf);
//     std::cout << "\n--- ReadFile测试用例 ---" << std::endl;
//     std::cout<<"ReadFile is "<<test<<" about:"<<buf<<std::endl;
//     return 0;
// }

//ReadFile测试
// int main()
// {
//     std::string buf="我吃了你吃了吗？";
//     bool test=Util::WriteFile("text.txt",buf);
//     std::cout << "\n--- WriteFile测试用例 ---" << std::endl;
//     std::cout<<"WriteFile is "<<test<<" about:"<<buf<<std::endl;
//     return 0;
// }

// //UrilCode测试
// int main()
// {
//     // 测试用例 1：带空格，且 convert_space_to_plus = true (预期：hello+world)
//     std::cout << Util::UrlEncode("hello world", true) << std::endl;

//     // 测试用例 2：带空格，且 convert_space_to_plus = false (预期：hello%20world)
//     std::cout << Util::UrlEncode("hello world", false) << std::endl;

//     // 测试用例 3：包含特殊字符 (预期：C%2B%2B)
//     std::cout << Util::UrlEncode("C++", false) << std::endl;

//     // 测试用例 4：包含中文字符 (预期：%E4%B8%AD%E6%96%87)
//     std::cout << Util::UrlEncode("中文", false) << std::endl;

//     // 测试用例 5：空字符串 (预期：输出空)
//     std::cout << "[" << Util::UrlEncode("", false) << "]" << std::endl;

//     return 0;
// }

//UrlDecode测试
// int main() {

//     // 1. 测试普通字符（不需要解码）
//     std::string t1 = Util::UrlDecode("hello", false);
//     std::cout << "1. 普通字符: " << t1 << std::endl;

//     // 2. 测试加号变空格 (convert_plus_to_space = true)
//     std::string t2 = Util::UrlDecode("hello+world", true);
//     std::cout << "2. 加号变空格: [" << t2 << "]" << std::endl;

//     // 3. 测试加号保留 (convert_plus_to_space = false)
//     std::string t3 = Util::UrlDecode("hello+world", false);
//     std::cout << "3. 加号保留: " << t3 << std::endl;

//     // 4. 测试标准的 %HH 编码字符 (如 %2B 是 '+', %20 是空格)
//     std::string t4 = Util::UrlDecode("C%2B%2B%20is%20good", false);
//     std::cout << "4. 特殊符号解码: " << t4 << std::endl;

//     // 5. 测试中文的 UTF-8 编码解码 (中文的 URL 编码通常很长)
//     // "中" -> %E4%B8%AD
//     // "文" -> %E6%96%87
//     std::string t5 = Util::UrlDecode("%E4%B8%AD%E6%96%87", false);
//     std::cout << "5. 中文解码: " << t5 << std::endl;

//     // 6. 测试非法/残缺的百分号数据
//     std::string t6 = Util::UrlDecode("100%", false); // 不够两位
//     std::cout << "6. 残缺数据: " << t6 << std::endl;
//     return 0;
// }

//StatuDesc测试
// int main()
// {
//     std::cout<<Util::StatuDesc(202)<<std::endl;
//     std::cout<<Util::StatuDesc(404)<<std::endl;
//     std::cout<<Util::StatuDesc(500)<<std::endl;
//     return 0;
// }


//ExtMime测试
// int main()
// {
//     std::cout<<Util::ExtMime("test.txt")<<std::endl;
//     std::cout<<Util::ExtMime("http.hpp")<<std::endl;
//     std::cout<<Util::ExtMime("http.iiiiiii")<<std::endl;
//     return 0;
// }

//IsDirectory测试
// int main() {
//     std::cout << Util::IsDirectory("/tmp") << std::endl;     // 预期: 1 (是目录)
//     std::cout << Util::IsDirectory("main.cc") << std::endl;  // 预期: 0 (不是目录)
//     std::cout << Util::IsRegular("main.cc") << std::endl;    // 预期: 1 (是普通文件)
//     std::cout << Util::IsRegular("/tmp") << std::endl;       // 预期: 0 (不是普通文件)
//     return 0;
// }

// //IsRegular测试
// int main() {
//     // 测试 1：测试当前存在的普通文件（假设你当前目录下有 main.cc）
//     std::cout<<Util::IsRegular("main.cc") << std::endl;
    
//     // 测试 2：测试一个目录（当前目录 "." ）
//     std::cout  << Util::IsRegular(".") << std::endl;
    
//     // 测试 3：测试一个不存在的文件
//     std::cout  << Util::IsRegular("unknown.txt") << std::endl;

//     return 0;
// }

//ValidPath测试
int main() {
    std::cout << Util::ValidPath("/image/a.png") << std::endl;             // 1 (正常)
    std::cout << Util::ValidPath("/image/../index.html") << std::endl;     // 1 (正常回退)
    std::cout << Util::ValidPath("/../etc/passwd") << std::endl;           // 0 (越狱拦截)
    std::cout << Util::ValidPath("/image/../../../etc/passwd") << std::endl; // 0 (越狱拦截)
    return 0;
}