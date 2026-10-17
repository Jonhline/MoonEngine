/* Lossless indexing of the complete native vertex record. Byte keys retain
   signed zero, NaN payloads, normals, UV seams and skin weights separately. */
#ifndef MOON_DEKO_INDEX_H
#define MOON_DEKO_INDEX_H
#include <cstdint>
#include <cstring>
#include <string_view>
#include <unordered_map>
#include <type_traits>
#include <vector>
template <class Vertex>
static void nd_index_vertices(const std::vector<Vertex> &source, std::vector<Vertex> &unique,
                              std::vector<uint32_t> &indices)
{
    static_assert(std::is_trivially_copyable<Vertex>::value, "vertex must be a byte record");
    unique.clear();
    indices.clear();
    std::unordered_map<std::string_view, uint32_t> lookup;
    lookup.reserve(source.size());
    unique.reserve(source.size());
    indices.reserve(source.size());
    for (const auto &vertex : source) {
        std::string_view bytes(reinterpret_cast<const char *>(&vertex), sizeof vertex);
        auto found = lookup.emplace(bytes, static_cast<uint32_t>(unique.size()));
        if (found.second) {
            unique.emplace_back();
            std::memcpy(&unique.back(), &vertex, sizeof vertex);
        }
        indices.push_back(found.first->second);
    }
}
#endif
