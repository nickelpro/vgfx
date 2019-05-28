#include "vgfx.h"

const vgfx_vertex vertices[] = {
    {
        {0.0f, -0.5},
        {1.0f, 0.0f, 0.0f}
    },
    {
        {0.5f, 0.5f},
        {0.0f, 1.0f, 0.0f}
    },
    {
        {-0.5f, 0.5f},
        {0.0f, 0.0f, 1.0f}
    }
};

const size_t vertices_size = sizeof(vertices);
const size_t vertices_len = sizeof(vertices)/sizeof(*vertices);
