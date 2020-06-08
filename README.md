# INF443: 3D Computer Graphics Project
An OpenCL based Real-time Path Tracer

by Shikang DU and Mingyu ZHANG

## Features

- Real-time ray tracing
- Progressive rendering
- Reflection and refraction material
- Movement of objects and interactive camara control.

## Compilation

The compilation can be done using  `CMAKE`.


The library has one external dependency: 
- [GLFW](https://www.glfw.org/) which can be installed through standard packages in Linux/MacOS (see the provided detailed tutorials).
- OpenCL, above v1.2, which is provided by CUDA or AMD equivalant.

Instructions could be found here:
* [Command lines to compile in Linux/MacOS](doc/compilation.md#command_line)
* **Detailed tutorials** to set up your system and compile on
  * [Linux/Ubuntu](doc/compilation.md#Ubuntu)
  * [MacOS](doc/compilation.md#MacOS)
  * [Windows](doc/visual_studio.md)

## Run
- Put the kerenl files in the right directory
  - CL kernel: `../../../cl_kernels/simple_fbo.cl` of the working dir.
  - HDR image: `../../../data/Mans_Outside_2k.hdr`.
- Execute program
- Indicate OpenCL platform and device
- Try the program, control your camara with keyboard 

## Reference
* [Course description](https://moodle.polytechnique.fr/course/view.php?id=7745)
* [Original code](https://github.com/drohmer/inf443_vcl)
* [GPU Ray tracing tutorial](http://raytracey.blogspot.com/2015/10/gpu-path-tracing-tutorial-1-drawing.html)
* [Ray tracing in one weekend](https://raytracing.github.io/books/RayTracingInOneWeekend.html)
* [Yune](https://github.com/gallickgunner/Yune)
