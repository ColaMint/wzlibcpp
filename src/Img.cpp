#include "Img.hpp"

wz::Img::Img(const std::initializer_list<u8> &new_iv, unsigned char *buffer, int size)
    : reader(Reader(key, buffer, size)), key(), iv(nullptr), root(new Node(Type::Image, this))
{
    iv = new u8[4];
    memcpy(iv, new_iv.begin(), 4);
    init_key();
    reader.set_key(key);
}

wz::Img::Img() : reader(Reader(key, nullptr, 0)), root(new Node(Type::Image, this))
{
}

void wz::Img::init_key()
{
    std::vector<u8> aes_key_v(32);
    memcpy(aes_key_v.data(), wz::AesKey2, 32);
    key = MutableKey({iv[0], iv[1], iv[2], iv[3]}, aes_key_v);
}

wz::Img::~Img()
{
    delete[] iv;
    delete root;
}

wz::Node *wz::Img::get_root() const
{
    return root;
}

void wz::Img::parse(const wzstring &name)
{
    root->path = name;
}

bool wz::Img::parse_img(Node *node)
{
    if (reader.is_wz_image())
    {
        return root->parse_property_list2(node, 0);
    }
    else
    {
        return false;
    }
}
