#include "../../src/platform/switch/deko_index.h"
#include <cstdio>
struct Vertex {
    uint32_t fields[27];
};
int main(int argc, char **)
{
    std::vector<Vertex> source(6000);
    for (unsigned i = 0; i < source.size(); i++)
        for (unsigned field = 0; field < 27; field++)
            source[i].fields[field] = ((i / 3) % 97) * 31 + field;
    // Equal positions with distinct UV, normal, signed zero, NaN and skin data.
    for (unsigned field = 0; field < 27; field++) {
        source[field] = source[0];
        source[field].fields[field] = 0x80000000u + field;
    }
    source[28].fields[5] = 0x7fc01234u;
    source[29].fields[5] = 0x7fc01235u;
    std::vector<Vertex> unique;
    std::vector<uint32_t> indices;
    nd_index_vertices(source, unique, indices);
    if (argc > 1)
        indices[15] = indices[0];
    if (indices.size() != source.size() || unique.size() >= source.size())
        return 1;
    // Authored triangle/group order and every attribute byte must reconstruct.
    for (unsigned first = 0; first < source.size(); first += 3)
        for (unsigned k = 0; k < 3; k++)
            if (indices[first + k] >= unique.size() ||
                std::memcmp(&source[first + k], &unique[indices[first + k]], sizeof(Vertex))) {
                std::fprintf(stderr, "vertex mismatch at %u\n", first + k);
                return 1;
            }
    auto count = unique.size();
    nd_index_vertices(source, unique, indices);
    if (unique.size() != count)
        return 1;
    std::vector<Vertex> empty;
    nd_index_vertices(empty, unique, indices);
    if (!unique.empty() || !indices.empty())
        return 1;
    std::printf("6000 vertices reconstruct byte-for-byte; %zu unique\n", count);
    return 0;
}
