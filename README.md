# wzlibcpp

modern first, more easier, cleaner api and support cross-platform

# Status

⬜️ utf8 formatted string with `std::string`

⬜️ remove all of older (deprecated) api -
`[[deprecated]]` attribute is currently marked.

☑️ modern api

☑️ cross-platform

# Dependencies

* zlib
* mio - for mmap (aka file mapping in windows)

# Usage

```cpp
#include <wz/File.hpp>

int main() {
    const auto iv = IV4(0x45, 0x50, 0x33, 0x01);
    wz::File file(iv, "Character.wz");

    if (!file.parse())
        return 1;

    // Children are returned in their original order in the WZ file.
    for (auto* child : file.get_root()->get_children()) {
        const auto& name = child->get_name();
        // Use child here.
    }

    auto* node = file.get_root()->find_from_path(u"00002000.img");
    return node == nullptr;
}
```

## output
https://gist.github.com/SeaniaTwix/f8b7e7cc34c5761e9679efa491816b63
