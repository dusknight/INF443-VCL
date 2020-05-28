
#include "modeling.hpp"
#include <ctime>

#ifdef SCENE_3D_GRAPHICS

// Add vcl namespace within the current one - Allows to use function from vcl library without explicitely preceeding their name with vcl::
using namespace vcl;
using namespace std;

float evaluate_terrain_z(float u, float v);
vec3 evaluate_terrain(float u, float v);
mesh create_terrain();
mesh create_cylinder(float radius, float height);
mesh create_cone(float radius, float height, float offset_z);
mesh create_tree_foliage(float radius, float height, float z_offset);
mesh create_grass();
//mesh create_sky();
hierarchy_mesh_drawable create_bird();

/** This function is called before the beginning of the animation loop
    It is used to initialize all part-specific data */
void scene_model::setup_data(std::map<std::string,GLuint>& , scene_structure& scene, gui_structure& )
{
    // Create visual terrain surface
    terrain = create_terrain();
    // terrain.uniform.color = {0.6f,0.85f,0.5f};
    terrain.uniform.shading.specular = 0.0f; // non-specular terrain material

    // Exercise 3.2 add texture
    texture_id = create_texture_gpu( image_load_png("scenes/3D_graphics/02_texture/assets/lawn.png") );

    // Exercise 3.3 Billboards
    billboard_id = create_texture_gpu( image_load_png("scenes/3D_graphics/02_texture/assets/billboard_grass.png"), GL_REPEAT, GL_REPEAT );
    update_grass_position();
    grass = create_grass();

    // Exercise 2.3 plant trees
    update_tree_position();
    trunk = create_cylinder(0.2f, 2.0f);
    trunk.uniform.color = {0.85f,0.6f,0.0f};
    foliage = create_tree_foliage(1.0f, 1.0f, 0.5f);
    foliage.uniform.color = {0.6f,0.85f,0.5f};

    // Exercise 4.2, 4.3 bird
    bird = create_bird();
    initialize_bird();
    K = 0.5f;

    // Exercise 5, rope
    initialize_rope(keyframes[0].p);
    seg_drawer.init();
    seg_drawer.uniform_parameter.color = {0,0,1};
    rope_node = mesh_primitive_sphere(0.02f);
    rope_node.uniform.color = {1,0,0};

    // Setup initial camera mode and position
    scene.camera.camera_type = camera_control_spherical_coordinates;
    scene.camera.scale = 10.0f;
    scene.camera.apply_rotation(0,0,0,1.2f);

}

/** This function is called at each frame of the animation loop.
    It is used to compute time-varying argument and perform data data drawing */
void scene_model::frame_draw(std::map<std::string,GLuint>& shaders, scene_structure& scene, gui_structure& )
{
    const float dt = timer.update();
    set_gui();

    // Current time
    const float t = timer.t;

    glEnable( GL_POLYGON_OFFSET_FILL ); // avoids z-fighting when displaying wireframe

    // Exercise 3.2 add texture
    glBindTexture(GL_TEXTURE_2D, texture_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    // Display terrain
    glPolygonOffset( 1.0, 1.0 );
    draw(terrain, scene.camera, shaders["mesh"]);

    glBindTexture(GL_TEXTURE_2D, scene.texture_white);

    // Exercise 2.3 plant trees
    glPolygonOffset( 1.0, 1.0 );
    for(size_t i=0;i<tree_position.size();i++){
        vec3 P = tree_position[i];
        trunk.uniform.transform.translation = P;
        foliage.uniform.transform.translation = P+vec3(0,0,1.8);
        draw(trunk, scene.camera, shaders["mesh"]);
        draw(foliage, scene.camera, shaders["mesh"]);
    }

    // Exercise 3.3 plant grass
    // *********************************
    // Enable use of alpha component as color blending for transparent elements
    //  new color = previous color + (1-alpha) current color
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Disable depth buffer writing
    //  - Transparent elements cannot use depth buffer
    //  - They are supposed to be display from furthest to nearest elements
    glDepthMask(false);
    glBindTexture(GL_TEXTURE_2D, billboard_id);

    for(size_t i=0;i<grass_position.size();i++){
        grass.uniform.transform.translation = grass_position[i];
        grass.uniform.transform.rotation = scene.camera.orientation;
        draw(grass, scene.camera, shaders["mesh"]);
        if(gui_scene.wireframe)
            draw(grass, scene.camera, shaders["wireframe"]);
    }

    glBindTexture(GL_TEXTURE_2D, scene.texture_white);
    glDepthMask(true);
    // ************************************

    // Exercise 4.2, 4.3 bird
    vec3 bird_p = update_global_position(bird["body"], t, keyframes);
    update_bird_inner(t);

    bird.update_local_to_global_coordinates();
    bird.set_shader_for_all_elements(shaders["mesh"]);
    draw(bird, scene.camera);
    // ************************************

    // Exercise 5, rope
    update_rope(dt);
    rope_p[N_rope_node+1] = bird_p;
    for(int i=0;i<=N_rope_node;i++){
        vec3 &p0 = rope_p[i];
        vec3 &p1 = rope_p[i+1];

        seg_drawer.uniform_parameter.p1 = p0;
        seg_drawer.uniform_parameter.p2 = p1;
        seg_drawer.draw(shaders["segment_im"], scene.camera);

        rope_node.uniform.transform.translation = p0;
        draw(rope_node, scene.camera, shaders["mesh"]);
    }

    if( gui_scene.wireframe ){ // wireframe if asked from the GUI
        glPolygonOffset( 1.0, 1.0 );
        draw(terrain, scene.camera, shaders["wireframe"]);
    }
}


/*********** Utilities***********/
// Evaluate height of the terrain for any (u,v) \in [0,1]
float evaluate_terrain_z(float u, float v)
{
    // const vec2 u0 = {0.5f, 0.5f};
    // const float h0 = 2.0f;
    // const float sigma0 = 0.15f;

    // Exercise 2.1, modify the height function
    const vec2 p[4] ={{0,0},{0.5,0.5},{0.2,0.7},{0.8,0.7}};
    const float h[4] = {3,-1.5,1,2};
    const float sigma[4] = {0.5,0.15,0.2,0.2};

    float z = 0;
    for(int i=0;i<4;i++){
        vec2 u0 = p[i];
        float h0 = h[i];
        float sigma0 = sigma[i];
        float d = norm(vec2(u,v)-u0)/sigma0;
        z += h0*std::exp(-d*d);
    }

    // Exercise 3.3, Perlin noise
    const float scaling = 1;
    const int octave = 8;
    const float persistency = 0.5;
    const float height = 1;

    const float noise = perlin(scaling*u, scaling*v, octave, persistency);

    z += height*noise;

    return z;
}

// Evaluate 3D position of the terrain for any (u,v) \in [0,1]
vec3 evaluate_terrain(float u, float v)
{
    const float x = 20*(u-0.5f);
    const float y = 20*(v-0.5f);
    const float z = evaluate_terrain_z(u,v);

    return {x,y,z};
}

// Generate terrain mesh
mesh create_terrain()
{
    // Number of samples of the terrain is N x N
    const size_t N = 100;

    mesh terrain; // temporary terrain storage (CPU only)
    terrain.position.resize(N*N);
    terrain.texture_uv.resize(N*N); // Exercise 3.2, add texture
    terrain.color.resize(N*N); // Exercise 3.2, adapt color according to height

    // Fill terrain geometry
    for(size_t ku=0; ku<N; ++ku)
    {
        for(size_t kv=0; kv<N; ++kv)
        {
            // Compute local parametric coordinates (u,v) \in [0,1]
            const float u = ku/(N-1.0f);
            const float v = kv/(N-1.0f);

            // Compute coordinates
            terrain.position[kv+N*ku] = evaluate_terrain(u,v);

            // Exercise 3.2, add texture
            terrain.texture_uv[kv+N*ku] = {u, v};

            // Exercise 3.2, adapt color according to height
            const float c = 0.3f+0.7f*evaluate_terrain_z(u, v);
            terrain.color[kv+N*ku]  = {c,c,c,1.0f};
        }
    }

    // Generate triangle organization
    //  Parametric surface with uniform grid sampling: generate 2 triangles for each grid cell
    const unsigned int Ns = N;
    for(unsigned int ku=0; ku<Ns-1; ++ku)
    {
        for(unsigned int kv=0; kv<Ns-1; ++kv)
        {
            const unsigned int idx = kv + N*ku; // current vertex offset

            const uint3 triangle_1 = {idx, idx+1+Ns, idx+1};
            const uint3 triangle_2 = {idx, idx+Ns, idx+1+Ns};

            terrain.connectivity.push_back(triangle_1);
            terrain.connectivity.push_back(triangle_2);
        }
    }

    return terrain;
}

// Exercise 2.1, create cylinder
mesh create_cylinder(float radius, float height){
    mesh cylinder;

    unsigned int N = 20;

    //gemometry
    cylinder.position.resize(2*N);
    for(unsigned int i=0;i<N;i++){
        cylinder.position[2*i] = {radius*std::cos(2*i*3.14f/N), radius*std::sin(2*i*3.14f/N), 0};
        cylinder.position[2*i+1] = {radius*std::cos(2*i*3.14f/N), radius*std::sin(2*i*3.14f/N), height};
    }

    //connectivity
    for(size_t k=0; k<N; ++k)
    {
        const unsigned int u00 = 2*k;
        const unsigned int u01 = (2*k+1)%(2*N);
        const unsigned int u10 = (2*(k+1))%(2*N);
        const unsigned int u11 = (2*(k+1)+1) % (2*N);

        const uint3 t1 = {u00, u10, u11};
        const uint3 t2 = {u00, u11, u01};
        cylinder.connectivity.push_back(t1);
        cylinder.connectivity.push_back(t2);
    }

    return cylinder;
}

// Exercise 2.2, create cone
mesh create_cone(float radius, float height, float offset_z){
    mesh cone;
    unsigned int N = 20;

    // geometry
    cone.position.resize(N+2);
    cone.position[N] = {0,0,offset_z};
    cone.position[N+1] = {0,0,height+offset_z};
    for(unsigned int i=0;i<N;i++)
        cone.position[i] = {radius*std::cos(2*i*3.14f/N), radius*std::sin(2*i*3.14f/N), offset_z};

    // connectivity
    for(unsigned int i=0;i<N;i++){
        cone.connectivity.push_back({i%N,N,(i+1)%N});
        cone.connectivity.push_back({N+1,i%N,(i+1)%N});
    }

    return cone;
}

// Exercise 2.2, create foillage
mesh create_tree_foliage(float radius, float height, float z_offset)
{
    mesh m = create_cone(radius, height, 0);
    m.push_back( create_cone(radius, height, z_offset) );
    m.push_back( create_cone(radius, height, 2*z_offset) );

    return m;
}

// Exercise 2.3, function to initialize tree positions
void scene_model::update_tree_position(){
    srand((unsigned int)(time(NULL)));
    int N = rand()%10;
    tree_position.resize(N);
    for(int i=0;i<N;i++){
        float u = rand_interval();
        float v = rand_interval();
        tree_position[i] = evaluate_terrain(u, v) - vec3(0,0,0.1);
    }
}

// Exercise 3.3, Billboards
mesh create_grass(){
    mesh surface_cpu;
    surface_cpu.position     = {{-0.2f,0,0}, { 0.2f,0,0}, { 0.2f, 0.4f,0}, {-0.2f, 0.4f,0}};
    surface_cpu.texture_uv   = {{0,1}, {1,1}, {1,0}, {0,0}};
    surface_cpu.connectivity = {{0,1,2}, {0,2,3}};

    return surface_cpu;
}

// Exercise 3.3, function to initialize grass positions
void scene_model::update_grass_position(){
    srand((unsigned int)(time(NULL)));
    int N = rand()%50;
    grass_position.resize(N);
    for(int i=0;i<N;i++){
        float u = rand_interval();
        float v = rand_interval();
        grass_position[i] = evaluate_terrain(u, v) - vec3(0,0,0.05);
    }
}

// Exercise 3.4, Sky 
// mesh create_sky(){
//     mesh sky;
//     sky.position = {{-0.5,-0.5,-0.5},{-0.5,1.5,-0.5},{1.5,1.5,-0.5},{-0.5,1.5,-0.5},
//                     {-0.5,-0.5, 1.5},{-0.5,1.5, 1.5},{1.5,1.5, 1.5},{-0.5,1.5, 1.5}};
//     sky.connectivity = {{0,1,2},{2,3,0},
//                         {1,0,4},{1,4,5},
//                         {1,5,2},{2,5,6},
//                         {2,7,3},{2,6,7},
//                         {0,3,7},{0,7,4}};
//     sky.texture_uv = {{1/4,0},{}}
// } 

// Exercise 4.2, bird
hierarchy_mesh_drawable create_bird(){
    hierarchy_mesh_drawable bird;

    // set up body parts
    const float radius = 0.2f;
    mesh_drawable body = mesh_drawable(mesh_primitive_sphere(radius, {0,0,0}));
    body.uniform.transform.scaling_axis = {2,1,1};

    mesh_drawable head = mesh_drawable(mesh_primitive_sphere(radius*0.8f, {0,0,0}));

    mesh_drawable eye = mesh_drawable(mesh_primitive_sphere(radius*0.15f, {0,0,0}, 20, 20));
    eye.uniform.color = {0,0,0};

    mesh_drawable beak = mesh_drawable(mesh_primitive_cone(radius*0.4f, {0,0,0}, {radius*0.8f,0,0}));
    beak.uniform.color = {1.0,0.6,0};

    mesh_drawable back_wing = mesh_drawable(mesh_primitive_quad(radius*vec3{-1,0,0},
                                                                radius*vec3{-1,2,0},
                                                                radius*vec3{1,2,0},
                                                                radius*vec3{1,0,0}));
    mesh_drawable front_wing = mesh_drawable(mesh_primitive_quad(radius*vec3{-1,0,0},
                                                                 radius*vec3{-0.4,1,0},
                                                                 radius*vec3{ 0.4,1,0},
                                                                 radius*vec3{ 1,0,0}));
    /*************************/

    // build hierarchy
    bird.add(body, "body");

    bird.add(head, "head", "body", radius * vec3{2.0f,0,0.75f});
    bird.add(eye, "eye_left", "head" , radius * vec3(1/2.0f, -1/2.2f, 1/2.8f));
    bird.add(eye, "eye_right", "head", radius * vec3(1/2.0f,  1/2.2f, 1/2.8f));
    bird.add(beak, "beak", "head", {radius * 0.5f,0,0});

    bird.add(back_wing, "back_l", "body", {0,0,0});
    bird.add(front_wing, "front_l", "back_l", {0,radius*2,0});

    bird.add(back_wing, "back_r", "body", {0,0,0});
    bird.add(front_wing, "front_r", "back_r", {0,radius*2,0});
    /**********************/

    return bird;
}

// Exercise 4.3, initialize bird position and timer
void scene_model::initialize_bird(){
    // Initial Keyframe data vector of (position, time)
    // keyframes = { { {-1,1,2}   , 0.0f  },
    //               { {0,1,2}    , 1.0f  },
    //               { {1,1,2}    , 2.0f  },
    //               { {1,2,2}    , 2.5f  },
    //               { {2,2,2}    , 3.0f  },
    //               { {2,2,3}    , 3.5f  },
    //               { {2,0,3.5}  , 3.75f  },
    //               { {1.5,-1,3} , 4.5f  },
    //               { {1.5,-1,2} , 5.0f  },
    //               { {1,-1,2}   , 6.0f  },
    //               { {0,-0.5,2} , 7.0f },
    //               { {-1,-0.5,2}, 8.0f },
    //               { {-1,1,2}   , 9.0f },
    //             }; 
    for(int i=0;i<12;i++){
        keyframes.push_back({{2.0f*sin(2*3.14f/12*i), 2.0f*cos(2*3.14f/12*i), 2.0f+0.2f*sin(2*3.14f/12*i)}, (float)i});
    }
    
    // Initial orientation
    vec3 initial_ori = keyframes[1].p - keyframes[keyframes.size()-1].p;
    bird["body"].transform.rotation = rotation_between_vector_mat3({1,0,0}, initial_ori);

    // Set timer bounds
    if(keyframes.size()<4) exit(1);
    timer.t_min = keyframes[0].t;
    timer.t_max = keyframes[keyframes.size()-1].t;
    timer.t = timer.t_min;
}

 // Exercise 4.2, inner bird animation
void scene_model::update_bird_inner(float t){

    //head nodding
    mat3 const R_head = rotation_from_axis_angle_mat3({0,1,0}, 0.25*std::sin(3.14f*t) );
    bird["head"].transform.rotation = R_head;

    //wing filpping
    mat3 const R_Bwing = rotation_from_axis_angle_mat3({1,0,0}, 0.5*std::sin(2*3.14f*t) );
    bird["back_l"].transform.rotation = R_Bwing;
    bird["back_r"].transform.rotation = mat3{1,0,0, 0,-1,0, 0,0,1}*R_Bwing;

    mat3 const R_Fwing = rotation_from_axis_angle_mat3({1,0,0}, 0.5*std::sin(2*3.14f*t) );
    bird["front_l"].transform.rotation = R_Fwing;
    bird["front_r"].transform.rotation = R_Fwing;
}

// Exercise 4.3, outer bird animation
vec3 scene_model::update_global_position(hierarchy_mesh_drawable_node &obj, float t, vcl::buffer<vec3t> const& v){
    //index_at_value t
    const size_t N = v.size();
    assert(v.size()>=2);
    assert(t>=v[0].t);
    assert(t<v[N-1].t);

    size_t idx=0;
    while( v[idx+1].t<t )
        ++idx;

    const float t0 = keyframes[(idx-1)%N].t; // t_{i-1}
    const float t1 = keyframes[ idx   %N].t; // t_i
    const float t2 = keyframes[(idx+1)%N].t; // t_{i+1}
    const float t3 = keyframes[(idx+2)%N].t; // t_{i+2}

    const vec3& p0 = keyframes[(idx-1)%N].p; // = p_{i-1}
    const vec3& p1 = keyframes[ idx   %N].p; // = p_i
    const vec3& p2 = keyframes[(idx+1)%N].p; // = p_{i+1}
    const vec3& p3 = keyframes[(idx+2)%N].p; // = p_{i+2}

    vec3 p = cardinal_spline_interpolation(t,t0,t1,t2,t3,p0,p1,p2,p3,K);
    obj.transform.translation = p;

    // rotation according to tangent
    vec3 tan0 = cardinal_spline_tangent(t,t0,t1,t2,t3,p0,p1,p2,p3,K);
    obj.transform.rotation = rotation_between_vector_mat3({1,0,0}, tan0);

    return p;
}

vec3 scene_model::cardinal_spline_interpolation(float t, float t0, float t1, float t2, float t3, const vec3& p0, const vec3& p1, const vec3& p2, const vec3& p3, float K)
{
    const float s = (t-t1)/(t2-t1);
    const vec3 d1 = 2*K*(p2-p0)/(t2-t0);
    const vec3 d2 = 2*K*(p3-p1)/(t3-t1);

    return (2*s*s*s-3*s*s+1)*p1+(s*s*s-2*s*s+s)*d1+(-2*s*s*s+3*s*s)*p2+(s*s*s-s*s)*d2;
}

vec3 scene_model::cardinal_spline_tangent(float t, float t0, float t1, float t2, float t3, const vec3& p0, const vec3& p1, const vec3& p2, const vec3& p3, float K)
{
    const float s = (t-t1)/(t2-t1);
    const vec3 d1 = 2*K*(p2-p0)/(t2-t0);
    const vec3 d2 = 2*K*(p3-p1)/(t3-t1);

    return (6*s*s-6*s)*p1+(3*s*s-4*s+1)*d1+(-6*s*s+6*s)*p2+(3*s*s-2*s)*d2;
}

void scene_model::initialize_rope(const vec3& initial_end_point){
    vec3 seg = initial_end_point / N_rope_node;
    L0 = norm(seg);
    for(int i=0;i<=N_rope_node;i++){
        rope_p.push_back(i*seg);
        rope_v.push_back({0,0,0});
    }
}

void scene_model::update_rope(float dt){
    const float K = 10.0f;
    const float mu = 1.0f;
    dt /= 1000;
    for(int i=N_rope_node;i>0;i--){
        for(int j=0;j<1000;j++){
            // force (used as acceleration)
            const vec3 f_spring_1 = spring_force(rope_p[i], rope_p[i+1], L0, K);
            const vec3 f_spring_2 = spring_force(rope_p[i], rope_p[i-1], L0, K);
            const vec3 gravity = {0,0,-0.5f};
            const vec3 drag = -mu*rope_v[i];

            // velocity
            rope_v[i] += (f_spring_1+f_spring_2+gravity+drag)*dt;

            // position
            rope_p[i] += rope_v[i]*dt;

            // stretch limit
            const float L = norm(rope_p[i]-rope_p[i+1]);
            if(L>4*L0){
                rope_p[i] = rope_p[i+1] - normalize(rope_p[i+1]-rope_p[i])*4*L0;
            }
        }
        // collision detection
        const float u = rope_p[i].x/20+0.5;
        const float v = rope_p[i].y/20+0.5; // cf evaluate_terrain
        const float h = evaluate_terrain_z(u, v);
        if(rope_p[i].z < h) rope_p[i].z = h;
    }
}

vec3 scene_model::spring_force(const vec3& pi, const vec3& pj, float L0, float K)
{
    // Exercise 5.2
    vec3 U = pj - pi;
    float L = norm(U);
    return K*(L-L0)*U/L;
}

void scene_model::set_gui()
{
    ImGui::Checkbox("Wireframe", &gui_scene.wireframe);
}



#endif

