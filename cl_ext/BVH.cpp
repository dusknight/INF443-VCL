#include "cl_ext/BVH.h"

#include <limits>
#include <cmath>
#include <algorithm> // min(); max();

BVH::BVH()
{
    bins = 20;
    leaf_primitives = 10;
    cost_isect = 1;
    cost_trav = 1 / 8.0f;
    clearValues();
    //ctor
}

BVH::~BVH()
{
    //dtor
}

void BVH::clearValues()
{
    bvh_size_kb = 0, bvh_size_mb = 0;
    cpu_node_list.clear();
    gpu_node_list.clear();
}

void BVH::createBVH(AABB root, const std::vector<TriangleCPU>& cpu_tri_list, int bvh_bins)
{
    clearValues();
    bins = bvh_bins;

    cpu_node_list.push_back(BVHCPU(root));
    for (int i = 0; i < cpu_tri_list.size(); i++)
        cpu_node_list[0].primitives.push_back(i);

    for (int i = 0; i < cpu_node_list.size(); i++)
    {
        bool switch_to_median = false;
        BVHCPU parent_node = cpu_node_list[i];
        AABB parent_aabb = parent_node.gpu_node.aabb;

        //If node is empty, no need to split
        if (parent_node.primitives.empty())
            continue;

        if (parent_node.primitives.size() <= leaf_primitives)
        {
            cpu_node_list[i].gpu_node.vert_len = parent_node.primitives.size();
            cpu_node_list[i].gpu_node.child_idx = -1;
            for (int j = 0; j < parent_node.primitives.size(); j++)
                cpu_node_list[i].gpu_node.vert_list[j] = parent_node.primitives[j];
            continue;
        }
        //Find the axis across which to split to form 2 BVH nodes
        BVH::SplitAxis split_axis = findSplitAxis(cpu_tri_list, parent_node.primitives);

        //If the Node is big enough use SAH else just split in middle
        BVHCPU node_c1, node_c2;
        int selection = 0;
        if (bins > 2 && parent_node.primitives.size() > 20 && !switch_to_median)
        {
            float cost = getSurfaceArea(parent_aabb) * cost_isect * parent_node.primitives.size();
            float initial_cost = cost;
            float increment = parent_aabb.p_max.s[split_axis] - parent_aabb.p_min.s[split_axis];
            increment /= bins;

            //Find the best possible split location
            for (int i = 0; i < bins - 1; i++)
            {
                AABB c1_aabb, c2_aabb;

                c1_aabb.p_min = parent_aabb.p_min;
                c1_aabb.p_max = parent_aabb.p_max;
                c1_aabb.p_max.s[split_axis] = parent_aabb.p_min.s[split_axis] + (i + 1) * increment;

                c2_aabb.p_min = parent_aabb.p_min;
                c2_aabb.p_min.s[split_axis] = c1_aabb.p_max.s[split_axis];
                c2_aabb.p_max = parent_aabb.p_max;

                BVHCPU temp_c1(c1_aabb), temp_c2(c2_aabb);

                //Find the child primitives of each temporary node
                populateChildNodes(parent_node, temp_c1, temp_c2, cpu_tri_list);
                float new_cost = cost_trav +
                    (getSurfaceArea(c1_aabb) / getSurfaceArea(parent_aabb)) * (cost_isect * temp_c1.primitives.size()) +
                    getSurfaceArea(c2_aabb) / getSurfaceArea(parent_aabb) * (cost_isect * temp_c2.primitives.size());

                //If new cost is less than previous cost we choose this split location.
                if (new_cost < cost)
                {
                    node_c1.gpu_node.aabb = c1_aabb;
                    node_c2.gpu_node.aabb = c2_aabb;
                    node_c1.primitives = temp_c1.primitives;
                    node_c2.primitives = temp_c2.primitives;
                    cost = new_cost;
                }
            }
            if (cost == initial_cost && parent_node.primitives.size() > leaf_primitives)
                switch_to_median = true;
            else if (cost == initial_cost && parent_node.primitives.size() <= leaf_primitives)
            {
                cpu_node_list[i].gpu_node.vert_len = parent_node.primitives.size();
                cpu_node_list[i].gpu_node.child_idx = -1;
                if (parent_node.primitives.size() > 0)
                {
                    for (int j = 0; j < parent_node.primitives.size(); j++)
                        cpu_node_list[i].gpu_node.vert_list[j] = parent_node.primitives[j];
                }
                continue;
            }
        }
        else
            switch_to_median = true;

        // IF SAH decided not to split and primitives are greater than leaf primitives, we still need to split so switch to median splitting...
        if (switch_to_median)
        {
            cl_float4 center = parent_aabb.p_min + parent_aabb.p_max;
            center = center / 2.0f;

            node_c1.gpu_node.aabb.p_min = parent_aabb.p_min;
            node_c1.gpu_node.aabb.p_max = parent_aabb.p_max;
            node_c1.gpu_node.aabb.p_max.s[split_axis] = center.s[split_axis];

            node_c2.gpu_node.aabb.p_max = parent_aabb.p_max;
            node_c2.gpu_node.aabb.p_min = parent_aabb.p_min;
            node_c2.gpu_node.aabb.p_min.s[split_axis] = center.s[split_axis];

            //Find the child primitives of each temporary node
            populateChildNodes(parent_node, node_c1, node_c2, cpu_tri_list);
        }
        cpu_node_list[i].gpu_node.child_idx = cpu_node_list.size();
        cpu_node_list.push_back(node_c1);
        cpu_node_list.push_back(node_c2);

    }

    resizeBvh(cpu_tri_list);
    for (int i = 0; i < cpu_node_list.size(); i++)
        gpu_node_list.push_back(cpu_node_list[i].gpu_node);

    bvh_size_kb = (float)gpu_node_list.size() * sizeof(BVHNodeGPU) / 1024;
    bvh_size_mb = bvh_size_kb / 1024;
}

void BVH::populateChildNodes(const BVHCPU& parent, BVHCPU& c1, BVHCPU& c2, const std::vector<TriangleCPU>& cpu_tri_list)
{
    int child_empty = -1; // 0 == c1 empty, 1 == c2 empty

    if (getSurfaceArea(c1.gpu_node.aabb) == 0.0f)
        child_empty = 0;
    else if (getSurfaceArea(c2.gpu_node.aabb) == 0.0f)
        child_empty = 1;

    if (child_empty == 0)
    {
        for (int i = 0; i < parent.primitives.size(); i++)
            c2.primitives.push_back(parent.primitives[i]);
    }
    else if (child_empty == 1)
    {
        for (int i = 0; i < parent.primitives.size(); i++)
            c1.primitives.push_back(parent.primitives[i]);
    }
    else
    {
        for (int i = 0; i < parent.primitives.size(); i++)
        {
            cl_float4 center;
            center = cpu_tri_list[parent.primitives[i]].centroid;
            bool in_c1 = true;
            for (int i = 0; i < 3; i++)
            {
                if (center.s[i] < c1.gpu_node.aabb.p_min.s[i] || center.s[i] > c1.gpu_node.aabb.p_max.s[i])
                {
                    in_c1 = false;
                    break;
                }
            }
            if (in_c1)
                c1.primitives.push_back(parent.primitives[i]);
            else
                c2.primitives.push_back(parent.primitives[i]);
        }
    }

}

void BVH::resizeBvh(const std::vector<TriangleCPU>& cpu_tri_list)
{
    float fmax = std::numeric_limits<float>::max();
    float fmin = -std::numeric_limits<float>::max();

    AABB parent;
    parent.p_min = { fmax, fmax, fmax, 1.0f };
    parent.p_max = { fmin, fmin, fmin, 1.0f };

    for (int j = cpu_node_list.size() - 1; j >= 0; j--)
    {
        BVHCPU node = cpu_node_list[j];
        if (node.gpu_node.vert_len > 0)
        {
            for (int i = 0; i < node.primitives.size(); i++)
                parent = getExtent(parent, cpu_tri_list[node.primitives[i]].aabb);

            cpu_node_list[j].gpu_node.aabb.p_min = parent.p_min;
            cpu_node_list[j].gpu_node.aabb.p_max = parent.p_max;

            parent.p_min = { fmax, fmax, fmax, 1.0f };
            parent.p_max = { fmin, fmin, fmin, 1.0f };
        }
        else if (node.gpu_node.child_idx > 0)
        {
            int c_idx = node.gpu_node.child_idx;
            cpu_node_list[j].gpu_node.aabb = getExtent(cpu_node_list[c_idx].gpu_node.aabb, cpu_node_list[c_idx + 1].gpu_node.aabb);
        }
    }
}

void BVH::resizeBvhNode(BVHCPU& node, const std::vector<TriangleCPU>& cpu_tri_list)
{
    float fmax = std::numeric_limits<float>::max();
    float fmin = -std::numeric_limits<float>::max();

    AABB parent;
    parent.p_min = { fmax, fmax, fmax, 1.0f };
    parent.p_max = { fmin, fmin, fmin, 1.0f };

    for (int j = 0; j < node.primitives.size(); j--)
    {
        int idx = node.primitives[j];
        parent = getExtent(parent, cpu_tri_list[idx].aabb);
    }

    node.gpu_node.aabb.p_min = parent.p_min;
    node.gpu_node.aabb.p_max = parent.p_max;
}

AABB BVH::getExtent(const AABB& bb1, const AABB& bb2)
{
    AABB parent = bb1;

    for (int i = 0; i < 3; i++)
    {
        parent.p_min.s[i] = std::min(bb2.p_min.s[i], bb1.p_min.s[i]);
        parent.p_max.s[i] = std::max(bb2.p_max.s[i], bb1.p_max.s[i]);
    }
    return parent;
}

BVH::SplitAxis BVH::findSplitAxis(const std::vector<TriangleCPU>& cpu_tri_list, std::vector<int>& child_list)
{
    AABB parent;
    float fmax = std::numeric_limits<float>::max();
    float fmin = -fmax;
    float diff = -fmax;
    BVH::SplitAxis split_axis = SplitAxis::X;

    parent.p_min = { fmax, fmax, fmax, 1.0f };
    parent.p_max = { fmin, fmin, fmin, 1.0f };

    for (int j = 0; j < child_list.size(); j++)
    {
        int k = child_list[j];
        //Find min/max X
        parent = getExtent(parent, cpu_tri_list[k].aabb);
    }

    cl_float4 diag = parent.p_max - parent.p_min;

    for (int i = 0; i < 3; i++)
    {
        if (diag.s[i] > diff)
        {
            diff = diag.s[i];
            if (i == 0)
                split_axis = SplitAxis::X;
            else if (i == 1)
                split_axis = SplitAxis::Y;
            else
                split_axis = SplitAxis::Z;
        }
    }
    return split_axis;
}

float BVH::getSurfaceArea(AABB aabb)
{
    cl_float4 diag = aabb.p_max - aabb.p_min;
    return 2 * (diag.s[0] * diag.s[1] + diag.s[0] * diag.s[2] + diag.s[1] * diag.s[2]);
}