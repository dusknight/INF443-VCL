#pragma once

#include "main/scene_base/base.hpp"

#ifdef SCENE_3D_GRAPHICS

using namespace vcl;

// Stores some parameters that can be set from the GUI
struct gui_scene_structure
{
    bool wireframe;
};

// 4. Descriptive animation
struct vec3t{
    vec3 p; // position
    float t;     // time
};

// 5. Simulation
struct particle_element
{
    vec3 p; // Position
    vec3 v; // Speed
};

struct scene_model : scene_base
{

    /** A part must define two functions that are called from the main function:
     * setup_data: called once to setup data before starting the animation loop
     * frame_draw: called at every displayed frame within the animation loop
     *
     * These two functions receive the following parameters
     * - shaders: A set of shaders.
     * - scene: Contains general common object to define the 3D scene. Contains in particular the camera.
     * - data: The part-specific data structure defined previously
     * - gui: The GUI structure allowing to create/display buttons to interact with the scene.
    */

    void setup_data(std::map<std::string,GLuint>& shaders, scene_structure& scene, gui_structure& gui);
    void frame_draw(std::map<std::string,GLuint>& shaders, scene_structure& scene, gui_structure& gui);

    void set_gui();

    // Exercise 2.3, function to initialize tree positions
    void update_tree_position();

    // Exercise 3.3, function to initialize grass positions
    void update_grass_position();

    // Exercise 4.2, inner bird animation
    void update_bird_inner(float t);

    // Exercise 4.3, initialize bird position and timer
    void initialize_bird();

    // Exercise 4.3, outer bird animation
    vec3 update_global_position(hierarchy_mesh_drawable_node &obj, float t, buffer<vec3t> const& v);
    float K;
    vec3 cardinal_spline_interpolation(float t, float t0, float t1, float t2, float t3, const vec3& p0, const vec3& p1, const vec3& p2, const vec3& p3, float K);
    vec3 cardinal_spline_tangent(float t, float t0, float t1, float t2, float t3, const vec3& p0, const vec3& p1, const vec3& p2, const vec3& p3, float K);

    // Exercise 5, simulate a rope
    const int N_rope_node = 40;
    void initialize_rope(const vec3& initial_end_point);
    /* Compute spring force applied on particle pi from particle pj */
    vec3 spring_force(const vec3& pi, const vec3& pj, float L0, float K);
    void update_rope(float dt);

    // visual representation of a surface
    mesh_drawable terrain;

    std::vector<vec3> tree_position;
    std::vector<vec3> grass_position;
    mesh_drawable trunk;
    mesh_drawable foliage;
    mesh_drawable grass;
    GLuint texture_id;
    GLuint billboard_id;

    hierarchy_mesh_drawable bird;
    // bird's key position
    buffer<vec3t> keyframes; // Given (position,time)

    std::vector<vec3> rope_p;
    std::vector<vec3> rope_v;
    mesh_drawable rope_node;
    float L0;
    segment_drawable_immediate_mode seg_drawer;

    gui_scene_structure gui_scene;
    timer_interval timer;
};

#endif


