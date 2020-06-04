#pragma once
#include "cl_ext/TriangleCPU.h"
#include <limits>
#include <algorithm>


TriangleCPU::TriangleCPU()
{
    //ctor
}

TriangleCPU::~TriangleCPU()
{
    //dtor
}

void TriangleCPU::computeCentroid()
{
    centroid = props.v1 + props.v2;
    centroid = centroid + props.v3;
    centroid = centroid / 3.0f;
    centroid.s[3] = 1.0f;

    computeAABB();
}

void TriangleCPU::computeAABB()
{
    cl_float4 p_min = { 0,0,0,1 };
    cl_float4 p_max = { 0,0,0,1 };

    for (int i = 0; i < 3; i++)
    {
        p_min.s[i] = std::min(props.v1.s[i], std::min(props.v2.s[i], props.v3.s[i]));
        p_max.s[i] = std::max(props.v1.s[i], std::max(props.v2.s[i], props.v3.s[i]));
    }
    cl_float4 diff = p_max - p_min;

    for (int i = 0; i < 3; i++)
    {
        if (diff.s[i] == 0.0f)
            p_max.s[i] += 0.2f;
    }

    aabb.p_min = p_min;
    aabb.p_max = p_max;
}