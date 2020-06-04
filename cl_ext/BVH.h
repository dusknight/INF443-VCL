#include "cl_ext/cl_defs.h"
#include "cl_ext/TriangleCPU.h"
#include "cl_ext/BVHCPU.h"

class BVH
{
public:
    BVH();
    ~BVH();
    std::vector<BVHNodeGPU> gpu_node_list;
    void createBVH(AABB root, const std::vector<TriangleCPU>& cpu_tri_list, int bvh_bins = 20);
    int bins;
    float bvh_size_kb, bvh_size_mb;

private:
    enum SplitAxis
    {
        X = 0,
        Y = 1,
        Z = 2
    };

    SplitAxis findSplitAxis(const std::vector<TriangleCPU>& cpu_tri_list, std::vector<int>& child_list);
    AABB getExtent(const AABB& bb1, const AABB& bb2);

    void clearValues();
    void populateChildNodes(const BVHCPU& parent, BVHCPU& c1, BVHCPU& c2, const std::vector<TriangleCPU>& cpu_tri_list);
    void resizeBvh(const std::vector<TriangleCPU>& cpu_tri_list);
    void resizeBvhNode(BVHCPU& node, const std::vector<TriangleCPU>& cpu_tri_list);
    float getSurfaceArea(AABB aabb);

    std::vector<BVHCPU> cpu_node_list;
    int leaf_primitives;
    float cost_isect, cost_trav;
};