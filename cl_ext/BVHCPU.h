#progma once
#include "cl_defs.h"

#include <vector>

class BVHCPU {
public:
	BVHCPU();
	BVHCPU(AABB aabb);
	BVHNodeGPU gpu_bvh;
	std::vector<int> primitives;
};