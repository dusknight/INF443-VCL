#include "cl_ext/cl_defs.h"
#include "vcl/math/vec/vec.hpp"

cl_float2 vcl_to_cl_f2(vcl::vec2 v) {
	return { v.x, v.y };
}

cl_float3 vcl_to_cl_f3(vcl::vec3 v) {
	return { v.x, v.y, v.z };
}

cl_float4 vcl_to_cl_f3(vcl::vec4 v) {
	return { v.x, v.y, v.z, v.w};
}