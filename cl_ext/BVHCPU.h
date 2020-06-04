#pragma  once
#include "cl_ext/cl_defs.h"

#include <vector>

class BVHCPU {
public:
	BVHCPU();
	BVHCPU(AABB aabb);
	BVHNodeGPU gpu_node;
	std::vector<int> primitives;
};