#pragma once

#include "Node.hpp"
#include "NumTypes.hpp"
#include <memory>

namespace wz {
    class Directory : public Node {
    public:
        explicit Directory(File* root_file, bool is_image_node, int new_size, int new_checksum, unsigned int new_offset);

        [[nodiscard]]
        u32 get_offset() const;

        [[nodiscard]]
        bool is_image() const;

        [[nodiscard]] int get_size() const noexcept { return size; }

        [[nodiscard]] int get_checksum() const noexcept { return checksum; }

        [[maybe_unused]]
        bool parse_image(Node* node);

        [[nodiscard]] Node* get_image();

    private:
        bool image_node;
        int size;
        int checksum;
        unsigned int offset;
        std::unique_ptr<Node> parsed_image;
#ifdef __EMSCRIPTEN__
        std::unique_ptr<Reader> image_reader;
#endif
    };
}
