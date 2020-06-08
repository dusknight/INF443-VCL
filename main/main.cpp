
// Include VCL library
#include "vcl/vcl.hpp"

// Include common part for exercises
#include "main/helper_scene/helper_scene.hpp"

// Include exercises
#include "scenes/scenes.hpp"

// for CL
#include "main/gl_interop.h"
#if defined(__APPLE__) || defined(__MACOSX)
#include <OpenCL/opencl.h>
#include <OpenGL/OpenGL.h>
#define CL_GL_SHARING_EXT "cl_apple_gl_sharing"

#else
#if defined (__linux)
#include <GL/glx.h>
#elif defined( __WIN32 )
#include <windows.h>
#endif // (__linux)

#include <CL/cl.hpp>
#include <CL/cl_gl.h>
#define CL_GL_SHARING_EXT "cl_khr_gl_sharing"
#endif // if defined(__APPLE__) || defined(__MACOSX)


#include <vector>
#include <fstream>
#include "cl_ext/camera.h"
#include "cl_ext/geometry.h"
#include "cl_ext/cl_defs.h"
#include "cl_ext/cl_manager.h"
#include "cl_ext/data/HDRloader.h"
// #include "cl_ext/BVHCPU.h"
//-------------CL----------------------

using namespace std;
using namespace cl;

const float camera_move_step = 0.5f;
const std::string cl_kernel_filename = "../../../cl_kernels/simple_fbo.cl";
const int sphere_count = 6;
const int triangle_count = 4;
// const char* HDRmapname = "../../../data/Topanga_Forest_B_3k.hdr";
const char* HDRmapname = "../../../data/Mans_Outside_2k.hdr";

// OpenCL objects
cl_manager cl_mgr(cl_kernel_filename);

GLuint rbo_IDs[2];
GLuint fbo_ID;
// image buffer (not needed with real-time viewport)
// cl_float4* cpu_output;
cl_int err;
unsigned int framenumber = 0;

Camera* hostRendercam = NULL;
InteractiveCamera* interactiveCamera;
Sphere cpu_spheres[sphere_count];
Triangle cpu_triangles[triangle_count];

// image buffer (not needed with real-time viewport)
cl_float4* cpu_output;
int buffer_switch = 1;
int buffer_reset = 0;
int scene_changed = 0;


HDRImage HDRresult;    
int e = HDRLoader::load(HDRmapname, HDRresult);
int HDRwidth = HDRresult.width;
int HDRheight = HDRresult.height;

void initScene(Sphere* cpu_spheres, Triangle* cpu_triangles) {
    //// floor
    cpu_spheres[3].radius = 200.0f;
    cpu_spheres[3].materialPara = 1;
    cpu_spheres[3].position = { 0.0f, -203.3f, 0.0f };
    cpu_spheres[3].color = { 0.1f, 0.3f, 0.7f };
    cpu_spheres[3].emission = { 0.3f, 0.2f, 0.0f };
    
    //// left sphere
    cpu_spheres[1].radius = 0.15f;
    cpu_spheres[1].materialPara = 0;
    cpu_spheres[1].position = { -0.35f, -1.29f, -0.15f };
    cpu_spheres[1].color = { 0.5f, 0.0f, 0.0f };
    cpu_spheres[1].emission = { 0.0f, 0.0f, 0.0f };
   
    // right sphere
    cpu_spheres[2].radius = 0.4f;
    cpu_spheres[2].materialPara = 1;
    cpu_spheres[2].position = { 0.35f, -1.29f, 0.15f };
    cpu_spheres[2].color = { 0.7f, 0.7f, 0.7f };
    cpu_spheres[2].emission = { 0.00f, 0.0f, 0.0f };

    //midle sphere
    cpu_spheres[4].radius = 0.1f;
    cpu_spheres[4].materialPara = 2;
    cpu_spheres[4].position = { 0.0f, -0.85f, 0.0f };
    cpu_spheres[4].color = { 0.2f, 0.7f, 0.2f };
    cpu_spheres[4].emission = { 0.1f, 0.1f, 0.1f };
    
    // lightsource
    cpu_spheres[0].radius = 0.45f;
    cpu_spheres[0].materialPara = 2;
    cpu_spheres[0].position = { 0.0f, 0.00f, -0.0f };
    cpu_spheres[0].color = { 0.9f, 0.9f, 0.9f };
    cpu_spheres[0].emission = { 0.00f, 0.00f, 0.00f };

    // Real lightsource
    cpu_spheres[5].radius = 0.15f;
    cpu_spheres[5].materialPara = 1;
    cpu_spheres[5].position = { 0.0f, 0.62f, -0.0f };
    cpu_spheres[5].color = { 0.85f, 0.7f, 0.1f };
    cpu_spheres[5].emission = { 0.75f, 0.35f, 0.13f };

    cpu_triangles[0].vertex1 = { 1.1f, -0.5f, 0.0f };
    cpu_triangles[0].vertex2 = { -1.1f, -0.5f, 0.0f };
    cpu_triangles[0].vertex3 = { 0.0f, -0.5f, 1.1f };
    cpu_triangles[0].materialPara = 1;
    cpu_triangles[0].color = { 0.5f, 0.0f, 0.5f };
    cpu_triangles[0].emission = { 0.10f, 0.10f, 0.10f };

    cpu_triangles[1].vertex1 = { 1.1f, 0.6f, 0.0f };
    cpu_triangles[1].vertex2 = { 2.1f, 0.4f, 0.0f };
    cpu_triangles[1].vertex3 = { 3.0f, 0.2f, 1.1f };
    cpu_triangles[1].materialPara = 1;
    cpu_triangles[1].color = { 0.5f, 0.2f, 0.5f };
    cpu_triangles[1].emission = { 0.10f, 0.10f, 0.10f };

    cpu_triangles[2].vertex1 = { -1.1f, 0.7f, -0.5f };
    cpu_triangles[2].vertex2 = { -2.5f, 0.8f, 0.0f };
    cpu_triangles[2].vertex3 = { -3.0f, 0.9f, 1.1f };
    cpu_triangles[2].materialPara = 1;
    cpu_triangles[2].color = { 0.5f, 0.5f, 0.0f };
    cpu_triangles[2].emission = { 0.10f, 0.10f, 0.10f };

    cpu_triangles[3].vertex1 = { 1.1f, 0.6f, 0.5f };
    cpu_triangles[3].vertex2 = { 0.9f, 0.4f, 0.0f };
    cpu_triangles[3].vertex3 = { -1.0f, -0.2f, 1.1f };
    cpu_triangles[3].materialPara = 1;
    cpu_triangles[3].color = { 0.0f, 0.6f, 0.5f };
    cpu_triangles[3].emission = { 0.10f, 0.10f, 0.10f };
	
}

void updateScene(Sphere* cpu_spheres, Triangle* cpu_triangles, bool update_spheres, bool update_triangles) 
{
    if (update_spheres || update_triangles) scene_changed = 1;
    if (update_spheres) {
        cpu_spheres[0].position.s[1] += 0.01f;
    }
    if (update_triangles) {
        for (int i = 0; i < triangle_count; i++) {
            cl_float3 center;
            center.s[0] = (cpu_triangles[i].vertex1.s[0] + cpu_triangles[i].vertex2.s[0] + cpu_triangles[i].vertex3.s[0]) / 3;
            center.s[1] = (cpu_triangles[i].vertex1.s[1] + cpu_triangles[i].vertex2.s[1] + cpu_triangles[i].vertex3.s[1]) / 3;
            center.s[2] = (cpu_triangles[i].vertex1.s[2] + cpu_triangles[i].vertex2.s[2] + cpu_triangles[i].vertex3.s[2]) / 3;
            center.s[0] *= 0.01; // for C99 std
            center.s[1] *= 0.01;
            center.s[2] *= 0.01;
            cpu_triangles[i].vertex1 = cpu_triangles[i].vertex1 + center;
            cpu_triangles[i].vertex2 = cpu_triangles[i].vertex2 + center;
            cpu_triangles[i].vertex3 = cpu_triangles[i].vertex3 + center;


            // center * center;
        }
    }
}


//void initHDR(cl_manager cl_mgr) {
//    // initialise HDR environment map
//    // from https://graphics.stanford.edu/wikis/cs148-11-summer/HDRIlluminator
//
//    const char* HDRfile = HDRmapname;
//
//    if (HDRLoader::load(HDRfile, HDRresult))
//        printf("HDR environment map loaded. Width: %d Height: %d\n", HDRresult.width, HDRresult.height);
//    else {
//        printf("HDR environment map not found\nAn HDR map is required as light source. Exiting now...\n");
//        system("PAUSE");
//        exit(0);
//    }
//
//    int HDRwidth = HDRresult.width;
//    int HDRheight = HDRresult.height;
//
//    
//    cl_float4 * cpuHDRenv = new cl_float4[HDRwidth * HDRheight];
//    //_data = new RGBColor[width*height];
//
//    for (int i = 0; i <HDRwidth ; i++) {
//        for (int j = 0; j < HDRheight ; j++) {
//            int idx = 3 * (HDRwidth * j + i);
//            //int idx2 = width*(height-j-1)+i;
//            int idx2 = HDRwidth * (j)+i;
//            cpuHDRenv[idx2] = { HDRresult.colors[idx], HDRresult.colors[idx + 1], HDRresult.colors[idx + 2], 0.0f };
//        }
//    }
//
//    // int err = cl_mgr.setupBUfferHDR(cpuHDRenv, HDRheight, HDRwidth);
//    int err = cl_mgr.setupBUfferHDR(HDRresult.colors, HDRheight, HDRwidth);
//    // copy HDR map to CL
//    
//    // cl_mgr.queue.enqueueWriteBuffer(cl_mgr.hdr_buffer, CL_TRUE, 0, sizeof(cl_float4)*50*50, cpuHDRenv);  // TODO: do we really need it?
//    // cl_mgr.queue.enqueueWriteBuffer(cl_mgr.cl_spheres, CL_TRUE, 0, sphere_count * sizeof(Sphere), cpu_spheres);
//}



void runKernel() {
    std::size_t global_work_size = window_width * window_height;
    std::size_t local_work_size = cl_mgr.kernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(cl_mgr.device);;

    // Ensure the global work size is a multiple of local work size
    if (global_work_size % local_work_size != 0)
        global_work_size = (global_work_size / local_work_size + 1) * local_work_size;

    // setup RBO for reading
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_ID);  // FBO for read back
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);  // default FBO for rendering
    glDrawBuffer(GL_BACK);

    //if (scene_changed) {
    //    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_ID);
    //    glDrawBuffer(GL_COLOR_ATTACHMENT1);
    //    glClear(GL_COLOR_BUFFER_BIT);
    //    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    //    glClear(GL_COLOR_BUFFER_BIT);
    //}
    //Make sure OpenGL is done using the VBOs
    glFinish();

    cl_int err_code;
    //this passes in the vector of VBO buffer objects => RBO objects
    err_code= cl_mgr.queue.enqueueAcquireGLObjects(&cl_mgr.image_buffers);
    if (err_code != CL_SUCCESS) {
        std::cerr << "ERROR in locking texture : " << err_code << std::endl;
    }
    cl_mgr.queue.finish();

    // launch the kernel
    err_code = cl_mgr.queue.enqueueNDRangeKernel(cl_mgr.kernel, NULL, global_work_size, local_work_size); // local_work_size
    if (err_code != CL_SUCCESS) {
        std::cerr << "ERROR in running queue :" << err_code << std::endl;
    }
    cl_mgr.queue.finish();

    //Release the VBOs so OpenGL can play with them
    // err_code = queue.enqueueReleaseGLObjects(&cl_vbos);
    err_code = cl_mgr.queue.enqueueReleaseGLObjects(&cl_mgr.image_buffers);
    if (err_code != CL_SUCCESS) {
        std::cerr << "ERROR in unlocking texture :" << err_code << std::endl;
    }
    cl_mgr.queue.finish();

    // read from FBO, save a copy
    //glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_ID);
    //glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_ID);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_ID);
    if (buffer_switch) {
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glDrawBuffer(GL_COLOR_ATTACHMENT1);
    }
    else {
        glReadBuffer(GL_COLOR_ATTACHMENT1);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
    }
    glClear(GL_COLOR_BUFFER_BIT);
    glBlitFramebuffer(
        0, 0, window_width, window_height,
        0, 0, window_width, window_height,
        GL_COLOR_BUFFER_BIT,
        GL_NEAREST); opengl_debug();
    
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_ID);  // for rendering: FBO
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);  // for rendering: FBO

    // clear previous frame and draw
    if (buffer_switch) glReadBuffer(GL_COLOR_ATTACHMENT0);
    else  glReadBuffer(GL_COLOR_ATTACHMENT1);
    glDrawBuffer(GL_BACK);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBlitFramebuffer(
        0, 0, window_width, window_height,
        0, 0, window_width, window_height,
        GL_COLOR_BUFFER_BIT,
        GL_LINEAR); opengl_debug();
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); opengl_debug();  // back to default
    // glDrawBuffer(GL_BACK); opengl_debug();  // draw to back buffer, waiting for swap

    buffer_switch = 1- buffer_switch;
    
}

//void render() {
//    glReadBuffer(GL_COLOR_ATTACHMENT3);
//    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//    glBlitFramebuffer(
//        0, 0, window_width, window_height,
//        0, 0, window_width, window_height,
//        GL_COLOR_BUFFER_BIT,
//        GL_LINEAR); opengl_debug();
//    //glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); opengl_debug();
//    //glDrawBuffer(GL_BACK); opengl_debug();
//}

bool setupBufferFBO() {
    // clear previous buffers
    glDeleteRenderbuffers(2, rbo_IDs);
    glDeleteFramebuffers(1, &fbo_ID);

    // create FBO, bind
    glGenFramebuffers(1, &fbo_ID);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_ID);

    // add Renderbufer to FBO
    glGenRenderbuffers(2, rbo_IDs);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo_IDs[0]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, window_width, window_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo_IDs[0]);

    glBindRenderbuffer(GL_RENDERBUFFER, rbo_IDs[1]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, window_width, window_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, rbo_IDs[1]);

    //glBindRenderbuffer(GL_RENDERBUFFER, rbo_IDs[2]);
    //glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, window_width, window_height);
    //glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_RENDERBUFFER, rbo_IDs[2]);

    //glBindRenderbuffer(GL_RENDERBUFFER, rbo_IDs[3]);
    //glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, window_width, window_height);
    //glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_RENDERBUFFER, rbo_IDs[3]);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Framebuffer not complete, code: " << status << endl;
    }

    // clear buffer
    glClearColor(0.f, 0.f, 0.f, 1.0f);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glClear(GL_COLOR_BUFFER_BIT);

    glDrawBuffer(GL_COLOR_ATTACHMENT1);
    glClear(GL_COLOR_BUFFER_BIT);

    //glDrawBuffer(GL_COLOR_ATTACHMENT2);
    //glClear(GL_COLOR_BUFFER_BIT);

    //glDrawBuffer(GL_COLOR_ATTACHMENT3);
    //glClear(GL_COLOR_BUFFER_BIT);

    // back to default
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    return true;
}




void cleanUp() {
    //	delete cpu_output;
}

// initialise camera on the CPU
void initCamera()
{
    delete interactiveCamera;
    interactiveCamera = new InteractiveCamera();

    interactiveCamera->setResolution(window_width, window_height);
    interactiveCamera->setFOVX(70);
}


// ************************************** //
// Global data declaration
// ************************************** //

// Storage for shaders indexed by their names
std::map<std::string,GLuint> shaders;

// General shared elements of the scene such as camera and its controler, visual elements, etc
scene_structure scene;

// The graphical interface. Contains Window object and GUI related variables
gui_structure gui;

// Part specific data - you will specify this object in the corresponding exercise part
scene_model scene_current;


// ************************************** //
// GLFW event listeners
// ************************************** //

void window_size_callback(GLFWwindow* /*window*/, int width, int height);
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_click_callback(GLFWwindow* window, int button, int action, int mods);
void mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void keyboard_input_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

// ************************************** //
// Start program
// ************************************** //




int main()
{

    // ************************************** //
    // Initialization and data setup
    // ************************************** //

    // Initialize external libraries and window
    initialize_interface(gui, window_width, window_height, true);
    // glfwSwapInterval(0);
    // Set GLFW events listener
    glfwSetCursorPosCallback(gui.window, cursor_position_callback );
    glfwSetMouseButtonCallback(gui.window, mouse_click_callback);
    glfwSetScrollCallback(gui.window, mouse_scroll_callback);
    glfwSetKeyCallback(gui.window, keyboard_input_callback);
    // glfwSetWindowSizeCallback(gui.window, window_size_callback);

    // load_shaders(shaders);
    // setup_scene(scene, gui, shaders);

    //opengl_debug();
    //std::cout<<"*** Setup Data ***"<<std::endl;
    //scene_current.setup_data(shaders, scene, gui);
    //std::cout<<"\t [OK] Data setup"<<std::endl;
    //opengl_debug();

    //////////////// OPENCL  ////////////////////////////
    // test_cl();
    
    // initialise OpenCL
    cl_mgr.initOpenCL();
;
    // create FBO
    setupBufferFBO();  // first GL
    // createVBO(&vbo);

    cl_int err;
    for (int i = 0; i < 2; i++) {  // Then CL
        cl_mgr.image_buffers.push_back(cl::BufferRenderGL(cl_mgr.context, CL_MEM_READ_WRITE, rbo_IDs[i], &err));
        if (err != CL_SUCCESS) std::cerr << "ERROR allocating RBO : " << err << std::endl;
    }

	////make sure OpenGL is finished before we proceed
	//glFinish();

	// initialise scene
	initScene(cpu_spheres, cpu_triangles);
    interactiveCamera = new InteractiveCamera;
    interactiveCamera->changeYaw(0.1);
    cl_mgr.cl_spheres = Buffer(cl_mgr.context, CL_MEM_READ_ONLY, sphere_count * sizeof(Sphere));
    cl_mgr.cl_triangles = Buffer(cl_mgr.context, CL_MEM_READ_ONLY, triangle_count * sizeof(Triangle));
    cl_mgr.cl_camera = Buffer(cl_mgr.context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, sizeof(Camera), interactiveCamera);
    // create HDR
     //initHDR(cl_mgr);
    cl_float4 * cpuHDRenv = new cl_float4[HDRwidth * HDRheight];
    //_data = new RGBColor[width*height];

    for (int i = 0; i <HDRwidth ; i++) {
        for (int j = 0; j < HDRheight ; j++) {
            int idx = 3 * (HDRwidth * j + i);
            //int idx2 = width*(height-j-1)+i;
            int idx2 = HDRwidth * (j)+i;
            cpuHDRenv[idx2] = { HDRresult.colors[idx], HDRresult.colors[idx + 1], HDRresult.colors[idx + 2], 0.0f };
        }
    }
    cl_mgr.hdr_buffer = cl::Buffer(cl_mgr.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_float4) * HDRwidth * HDRheight, cpuHDRenv, &err);

    // hostRendercam = (Camera*) queue.enqueueWriteBuffer(cl_spheres, CL_TRUE, 0, sphere_count * sizeof(Sphere), cpu_spheres);
    
    // clEnqueueMapBuffer
	// intitialise the kernel
    cl_mgr.initCLKernel(buffer_switch, buffer_reset, window_width, window_height, sphere_count, triangle_count, framenumber);
    // err = cl_mgr.queue.enqueueWriteBuffer(cl_mgr.hdr_buffer, CL_TRUE, 0, sizeof(cl_float) * 5, HDRresult.colors);  // TODO: do we really need it?

    // ************************************** //
    // Animation loop
    // ************************************** //

    std::cout<<"*** Start GLFW animation loop ***"<<std::endl;
    vcl::glfw_fps_counter fps_counter;
    hostRendercam = new Camera;
    cl::Event ev_buffer;
    while( !glfwWindowShouldClose(gui.window) )
    {

        // Clear all color and zbuffer information before drawing on the screen
        clear_screen();opengl_debug();
        // Set a white image texture by default
        // glBindTexture(GL_TEXTURE_2D,scene.texture_white);

        // Create the basic gui structure with ImGui
        gui_start_basic_structure(gui, cl_mgr, interactiveCamera);

        framenumber++;

        // Perform computation and draw calls for each iteration loop
        // scene_current.frame_draw(shaders, scene, gui); opengl_debug();
        // render(gui.window);
        // framenumber++;
        updateScene(cpu_spheres, cpu_triangles, gui.update_spheres, gui.update_triangles);

        // host to device: write
        err = cl_mgr.queue.enqueueWriteBuffer(cl_mgr.cl_spheres, CL_TRUE, 0, sphere_count * sizeof(Sphere), cpu_spheres);
        err = cl_mgr.queue.enqueueWriteBuffer(cl_mgr.cl_triangles, CL_TRUE, 0, triangle_count * sizeof(Triangle), cpu_triangles);
        //if (buffer_reset) {
        //    float arg = 0;
        //    queue.enqueueFillBuffer(cl_accumbuffer, arg, 0, window_width * window_height * sizeof(cl_float3));
        //    framenumber = 0;
        //}
        // buffer_reset = false;

        // update Camera
        interactiveCamera->buildRenderCamera(hostRendercam);
        // copy the host camera to a OpenCL camera
        cl_mgr.queue.enqueueWriteBuffer(cl_mgr.cl_camera, CL_TRUE, 0, sizeof(Camera), hostRendercam, 0);

        if (scene_changed) buffer_reset = 1;

    
        // queue.enqueueMapBuffer(cl_camera, CL_TRUE, CL_MAP_WRITE, 0, sizeof(Camera));
        // update params

        cl_mgr.kernel.setArg(2, buffer_reset);
        cl_mgr.kernel.setArg(3, cl_mgr.cl_spheres);  // in case that spheres move
        cl_mgr.kernel.setArg(4, cl_mgr.cl_triangles);
        cl_mgr.kernel.setArg(9, framenumber);
        cl_mgr.kernel.setArg(10, cl_mgr.cl_camera);
        cl_mgr.kernel.setArg(11, rand());
        cl_mgr.kernel.setArg(12, rand());
        cl_mgr.kernel.setArg(13, cl_mgr.hdr_buffer);

        runKernel();

        // draw

        ////////////////////////////// GUI ////////////////////////////
        // Render GUI and update window
        ImGui::End();
        // scene.camera_control.update = !(ImGui::IsAnyWindowFocused());
        vcl::imgui_render_frame(gui.window);

        update_fps_title(gui.window, gui.window_title, fps_counter);
        buffer_reset = 0;
        scene_changed = 0;
        glfwSwapBuffers(gui.window);
        glfwPollEvents();

    }
    std::cout<<"*** Stop GLFW loop ***"<<std::endl;

    // Cleanup ImGui and GLFW
    vcl::imgui_cleanup();

    glfwDestroyWindow(gui.window);
    glfwTerminate();

    return 0;
}

void window_size_callback(GLFWwindow* /*window*/, int width, int height)
{
    glViewport(0, 0, width, height);
    scene.camera.perspective.image_aspect = width / static_cast<float>(height);;
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    scene.camera_control.update_mouse_move(scene.camera, window, float(xpos), float(ypos));
    scene_current.mouse_move(scene, window);
}
void mouse_click_callback(GLFWwindow* window, int button, int action, int mods)
{
    ImGui::SetWindowFocus(nullptr);
    scene.camera_control.update_mouse_click(scene.camera, window, button, action, mods);
    scene_current.mouse_click(scene, window,button,action,mods);
}
void mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    scene_current.mouse_scroll(scene, window, float(xoffset), float(yoffset));
}
void keyboard_input_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    scene_current.keyboard_input(scene, window, key, scancode, action, mods);
    if (ImGui::GetIO().WantCaptureKeyboard)
        return;
    //GlfwManager* ptr = (GlfwManager*)glfwGetWindowUserPointer(window);

    if (key == GLFW_KEY_UP && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        scene_changed = 1;
        interactiveCamera->changePitch(0.1);
    }

    if (key == GLFW_KEY_DOWN && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        interactiveCamera->changePitch(-0.1);
        scene_changed = 1;
    }

    if (key == GLFW_KEY_LEFT && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        interactiveCamera->changeYaw(-0.05); 
        scene_changed = 1;
    }

    if (key == GLFW_KEY_RIGHT && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        interactiveCamera->changeYaw(0.05); 
        scene_changed = 1;
    }
    if (key == GLFW_KEY_HOME && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        interactiveCamera->changeApertureDiameter(0.2);
        scene_changed = 1;
    }

    if (key == GLFW_KEY_END && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        interactiveCamera->changeApertureDiameter(-0.2);
        scene_changed = 1;
    }

    //if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
    //    ptr->space_flag = true;
    //if (key == GLFW_KEY_SPACE && action == GLFW_RELEASE)
    //    ptr->space_flag = false;

    if (key == GLFW_KEY_W && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        interactiveCamera->goForward(camera_move_step);
        scene_changed = 1;
    }
    if (key == GLFW_KEY_S && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        interactiveCamera->goForward(-camera_move_step);
        scene_changed = 1;
    }
    if (key == GLFW_KEY_D && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        interactiveCamera->rotateRight(10*camera_move_step);
        scene_changed = 1;
    }
    if(key == GLFW_KEY_S && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        interactiveCamera->rotateRight(-10*camera_move_step);
        scene_changed = 1;
    }
}
