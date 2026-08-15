#include <wz/Node.hpp>

#include <cassert>

int main()
{
    wz::Node root;
    auto *first = new wz::Node();
    auto *second = new wz::Node();
    auto *duplicate = new wz::Node();

    root.append_child(u"z", first);
    root.append_child(u"a", second);
    root.append_child(u"z", duplicate);

    const auto &children = root.get_children();
    assert(children.size() == 3);
    assert(children[0] == first);
    assert(children[1] == second);
    assert(children[2] == duplicate);
    assert(children[0]->get_name() == u"z");
    assert(children[1]->get_name() == u"a");
    assert(root.get_child(u"z") == first);
    assert(root.children_count() == 3);

    const wz::Node &const_root = root;
    assert(*const_root.begin() == first);
    assert(root.find_from_path(u"./a") == second);
    assert(root.find_from_path(u"../a") == nullptr);

    wz::MutableKey zero_key({0, 0, 0, 0}, std::vector<u8>(32));
    assert(zero_key[100] == 0);
}
