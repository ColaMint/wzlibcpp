#include "Wz.hpp"
#include "Property.hpp"

u32 wz::get_version_hash(i32 encrypted_version, i32 real_version) {
    i32 version_hash = 0;
    auto version_string = std::to_string(real_version);

    auto len = version_string.size();

    for (size_t i = 0; i < len; ++i) {
        version_hash = (32 * version_hash) + static_cast<i32>(version_string[i]) + 1;
    }

#define HASHING(V, S) ((V >> S##u) & 0xFFu)
#define AUTO_HASH(V) (0xFFu ^ HASHING(V, 24) ^ HASHING(V, 16) ^ HASHING(V, 8) ^ V & 0xFFu)

    i32 decrypted_version_number = AUTO_HASH(static_cast<u32>(version_hash));

    if (encrypted_version == decrypted_version_number) {
        return static_cast<u32>(version_hash);
    }

    return 0;
}
