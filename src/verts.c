#include "vgfx.h"

const vgfx_vertex vertices[] = {
	{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
	{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
	{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
	{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

const size_t vertices_size = sizeof(vertices);
const size_t vertices_len = sizeof(vertices)/sizeof(*vertices);

const uint16_t indices[] = {0, 1, 2, 2, 3, 0};

const size_t indices_size = sizeof(indices);
const size_t indices_len = sizeof(indices)/sizeof(*indices);
