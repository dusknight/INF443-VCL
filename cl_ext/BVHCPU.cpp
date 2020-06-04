#include "BVHCPU.h"

BVHCPU::BVHCPU()
{
    gpu_node.vert_len = -1;
    gpu_node.child_idx = -2;
    gpu_node.vert_list[0] = -1;
    gpu_node.vert_list[1] = -1;
    gpu_node.vert_list[2] = -1;
    gpu_node.vert_list[3] = -1;
    primitives.clear();
}

BVHCPU::BVHCPU(AABB aabb):gpu_node{ aabb, {-1,-1,-1,-1}, -1, -1 }
{
    gpu_node.vert_len = -1;
    gpu_node.child_idx = -2;
    gpu_node.vert_list[0] = -1;
    gpu_node.vert_list[1] = -1;
    gpu_node.vert_list[2] = -1;
    gpu_node.vert_list[3] = -1;
    primitives.clear();
}
