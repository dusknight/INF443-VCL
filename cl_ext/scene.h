#pragma once
#include "cl_ext/cl_defs.h"
#include "cl_ext/camera.h"
#include "cl_ext/TriangleCPU.h"
#include "cl_ext/BVH.h"

#include <string>
#include <vector>
class Scene
{
public:
    Scene();
    ~Scene();
    void setBuffer();
    void loadModel(std::string filepath, std::string filename);
    void loadBVH(int bvh_bins);
    void reloadMatFile();

    Camera main_camera;
    std::vector<TriangleGPU> vert_data;
    std::vector<Material> mat_data;
    std::string scene_file, mat_file, mat_filename;
    AABB root;
    BVH bvh;
    int num_triangles;
    float scene_size_kb, scene_size_mb;

private:
    void clearValues();
    std::string getMatFileName(std::string filepath);
    std::vector<TriangleCPU> cpu_tri_list;
    const char* HDRmapname;
};