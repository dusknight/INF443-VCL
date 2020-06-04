# pragma once
#include "cl_ext/cl_defs.h"

class TriangleCPU
{
public:
	TriangleCPU();
	~TriangleCPU();
	TriangleGPU props;
	AABB aabb;
	cl_float4 centroid;
	void computeCentroid();
	void computeAABB();
};

// TRIANGLECPU_H
