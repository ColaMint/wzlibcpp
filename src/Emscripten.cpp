#ifdef __EMSCRIPTEN__
#include "Emscripten.hpp"

void wz::Emscripten::fetch_file_success(emscripten_fetch_t *fetch)
{
    file_size = fetch->numBytes;
    file_data = (unsigned char *)malloc(file_size);
    memcpy(file_data, fetch->data, fetch->numBytes);
    emscripten_fetch_close(fetch); // 关闭 fetch 对象，释放资源
    fetch_done = true;             // 请求完成
}

// 请求失败时的回调函数
void wz::Emscripten::fetch_file_failure(emscripten_fetch_t *fetch)
{
    emscripten_fetch_close(fetch); // 关闭 fetch 对象，释放资源
    fetch_done = true;             // 请求完成
}

void wz::Emscripten::load_file(std::string &url)
{
    std::cout << "load:" << url << std::endl;
    emscripten_fetch_attr_t fetchAttr;
    emscripten_fetch_attr_init(&fetchAttr);

    // 设置请求方法为 "GET"（注意：直接赋值字符串）
    strcpy(fetchAttr.requestMethod, "GET");

    // 设置成功和失败的回调函数
    fetchAttr.onsuccess = fetch_file_success;
    fetchAttr.onerror = fetch_file_failure;

    fetchAttr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;

    // 启动文件下载
    emscripten_fetch(&fetchAttr, url.c_str());
    // 模拟同步：等待请求完成
    while (!fetch_done)
    {
        emscripten_sleep(1); // 每1毫秒检查一次是否完成
    }
    fetch_done = false;
    return;
}

// void wz::Emscripten::load_file(std::string &url)
// {
//     auto file = fopen(url.c_str(), "rb"); // 使用"rb"模式以二进制读取方式打开文件
//     if (file == NULL)
//     {
//         perror("Error opening file");
//     }
//     // 获取文件大小
//     fseek(file, 0, SEEK_END);     // 移动到文件末尾
//     file_size = ftell(file); // 获取当前位置，即文件大小
//     rewind(file);

//     // 分配内存以存储文件内容
//     file_data = (unsigned char *)malloc(file_size); // +1 为字符串结尾的'\0'

//     // 读取文件内容到buffer
//     fread(file_data, file_size, 1, file); // fread的参数依次是：目标地址，元素大小，元素数量，文件指针
// }

void wz::Emscripten::load_file(std::u16string &url)
{
    auto str = std::string{url.begin(), url.end()};
    return load_file(str);
}
#endif
