# INF443: 3D Computer Graphics Project
An OpenCL based Real-time Path Tracer

by Shikang DU and Mingyu ZHANG

## Features

- Real-time ray tracing
- Progressive rendering
- Reflection and refraction material
- Movement of objects and interactive camara control.

## Compilation

The compilation can be done using  `CMAKE`. Tested in Windows 10 with VS2019.


The library has one external dependency: 
- [GLFW](https://www.glfw.org/) which can be installed through standard packages in Linux/MacOS (see the provided detailed tutorials).
- OpenCL, above v1.2, which is provided by CUDA or AMD equivalant. 
  - **WARNING**: Intel is not supported. (specially for Linux)
  - If `cl.hpp` is not provided by your OpenCL SDK, please download it from [Khronos website](https://www.khronos.org/registry/OpenCL/api/2.1/cl.hpp).
  - If `FindOpenCL.cmake` does not work, please set `OpenCL_INCLUDE_DIRS` and `OpenCL_LIBRARY` manually, as showed in `CMakeList.txt`

Instructions could be found here:
* [Command lines to compile in Linux/MacOS](doc/compilation.md#command_line)
* **Detailed tutorials** to set up your system and compile on
  * [Linux/Ubuntu](doc/compilation.md#Ubuntu)
  * [MacOS](doc/compilation.md#MacOS)
  * [Windows](doc/visual_studio.md)

## Run
- Put the kerenl files in the right directory
  - CL kernel: `../../../cl_kernels/simple_fbo.cl` from the working dir.
  - HDR image: `../../../data/Mans_Outside_2k.hdr` from the working dir.
- Execute program
- Indicate OpenCL platform and device
- Try the program, control your camara with keyboard 

## Reference
* [Course description](https://moodle.polytechnique.fr/course/view.php?id=7745)
* [Original code](https://github.com/drohmer/inf443_vcl)
* [GPU Ray tracing tutorial](http://raytracey.blogspot.com/2015/10/gpu-path-tracing-tutorial-1-drawing.html)
* [Ray tracing in one weekend](https://raytracing.github.io/books/RayTracingInOneWeekend.html)
* [Yune](https://github.com/gallickgunner/Yune)
