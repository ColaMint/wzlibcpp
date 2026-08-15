#pragma once

#include "Node.hpp"
#include "Reader.hpp"
#include "Wz.hpp"
#include "Keys.hpp"
#include <array>
#include <memory>

namespace wz
{
    class File final
    {

    public:
        [[maybe_unused]] explicit File(const std::initializer_list<u8> &new_iv, const char *path);

        [[maybe_unused]] explicit File(const u8 *new_iv, const char *path);

        ~File();

        [[maybe_unused]] bool parse(const wzstring &name = u"");

        [[maybe_unused]] [[nodiscard]] Node *get_root() const;
        Node &get_child(const wzstring &name);

    private:
        MutableKey key;
        std::array<u8, 4> iv{};
        Description desc{};
        Reader reader;
        std::unique_ptr<Node> root;

        bool parse_directories(Node *node);

        u32 get_wz_offset();

        void init_key();

        friend class Node;
    };
}
