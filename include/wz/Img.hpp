#pragma once

#include "Node.hpp"
#include "Reader.hpp"
#include "Wz.hpp"
#include "Keys.hpp"
#include "NumTypes.hpp"

namespace wz
{
    class Img final
    {
    public:
        explicit Img(const std::initializer_list<u8> &new_iv, unsigned char *buffer, int size);

        explicit Img();

        ~Img();

        [[maybe_unused]] [[nodiscard]] Node *get_root() const;

        [[maybe_unused]] void parse(const wzstring &name = u"");

        [[maybe_unused]]
        bool parse_img(Node *node);

        void init_key();

    public:
        Node *root;

        Reader reader;
        MutableKey key;
        u8 *iv;
    };
}