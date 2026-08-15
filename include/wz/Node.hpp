#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

#include "Wz.hpp"
#include "Reader.hpp"
#include "Types.hpp"

namespace wz
{

    class Node;
    class File;

    typedef std::vector<Node *> WzList;
    typedef std::unordered_map<wzstring, WzList> WzMap;

    class Node
    {
    public:
        explicit Node();
        explicit Node(const Type &new_type, File *root_file);

        virtual ~Node();

        Node(const Node &) = delete;
        Node &operator=(const Node &) = delete;
        Node(Node &&) = delete;
        Node &operator=(Node &&) = delete;

        Node &operator[](const wzstring &name);

        void append_child(const wzstring &name, Node *node);

        void append_child(const wzstring &name, std::unique_ptr<Node> node);

        Node *get_child(const wzstring &name);

        Node *get_child(const std::string &name);

        [[nodiscard]] const WzList &get_children() const noexcept;

        [[nodiscard]] Node *get_parent() const noexcept;

        [[nodiscard]] size_t children_count() const noexcept;

        WzList::iterator begin() noexcept;

        WzList::iterator end() noexcept;

        WzList::const_iterator begin() const noexcept;

        WzList::const_iterator end() const noexcept;

        [[maybe_unused]] [[nodiscard]] Type get_type() const;

        [[nodiscard]] const wzstring &get_name() const noexcept;

        [[nodiscard]] const wzstring &get_path() const noexcept;

        [[nodiscard]] bool is_property() const;

        Node *find_from_path(const std::u16string &path);

        Node *find_from_path(const std::string &path);

    protected:
        [[nodiscard]] Reader *get_reader() const noexcept;
        [[nodiscard]] wz::MutableKey &get_key() const;

    private:
        Type type;

        Node *parent;
        WzList children;
        WzMap children_by_name;

        File *file;
        Reader *reader = nullptr;

        wzstring name;
        std::u16string path = u"";

        bool parse_property_list(Node *target, size_t offset);
        void parse_extended_prop(const wzstring &name, Node *target, const size_t &offset);
        WzCanvas parse_canvas_property();
        WzSound parse_sound_property();

        [[nodiscard]] const u8 *get_iv() const;
        friend class Directory;
        friend class File;
    };

}
