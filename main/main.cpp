#include <iostream>

#include <algorithm>

#include <wz/Node.hpp>
#include <wz/File.hpp>
#include <wz/Directory.hpp>
#include <wz/Property.hpp>
#include <wz/Img.hpp>

#include <iostream>
#include <fstream>

#define U8 static_cast<u8>
#define IV4(A, B, C, D) \
    {                   \
        U8(A), U8(B), U8(C), U8(D)}

int main()
{
    int a = 0;
    const auto iv = IV4(0xb9, 0x7d, 0x63, 0xe9);
    // wz::File file(iv, "100000000.img");
    // 打开文件
    auto file = fopen("100000000.img", "rb"); // 使用"rb"模式以二进制读取方式打开文件
    if (file == NULL)
    {
        perror("Error opening file");
        return -1;
    }

    // 获取文件大小
    fseek(file, 0, SEEK_END);     // 移动到文件末尾
    auto file_size = ftell(file); // 获取当前位置，即文件大小
    rewind(file);

    // 分配内存以存储文件内容
    unsigned char *buffer = (unsigned char *)malloc(file_size + 1); // +1 为字符串结尾的'\0'
    if (buffer == NULL)
    {
        perror("Memory allocation failed");
        fclose(file);
        return -1;
    }

    // 读取文件内容到buffer
    fread(buffer, file_size, 1, file); // fread的参数依次是：目标地址，元素大小，元素数量，文件指针
    buffer[file_size] = '\0';          // 确保字符串正确结束

    wz::Img img(iv, buffer, file_size);
    auto *image = new wz::Node();
    img.parse_img(image, u"100000000.img");
    int ax = 1;
    // auto *image = new wz::Directory::Directory(file,true,1,1,1);
    // auto *dir = dynamic_cast<wz::Directory *>(file.get_root());
    // dir->parse_image(image);
    // auto c = image->children_count();

    return 0;
}