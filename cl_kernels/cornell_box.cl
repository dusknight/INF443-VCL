/* OpenCL based simple sphere path tracer by Sam Lapere, 2016*/
/* based on smallpt by Kevin Beason */
/* http://raytracey.blogspot.com */
/* interactive camera and depth-of-field code based on GPU path tracer by Karl Li and Peter Kutz */

__constant float EPSILON = 0.00003f; /* req2uired to compensate for limited float precision */
__constant float PI = 3.14159265359f;
__constant int SAMPLES = 4;
__constant intrrr HEAP_SIZE = 1500

__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
CLK_ADDRESS_CLAMP_TO_EDGE |
CLK_FILTER_NEAREST;

//
//***************** DATA STRUCT ******************** //
//
typedef struct Ray {
	float3 origin;
	float3 dir;
	float length;
	bool is_shadow_ray;

} Ray;

typedef struct Sphere {
	float radius;
	float3 pos;
	float3 color;
	float3 emission;
} Sphere;

typedef struct Camera {
	float3 position;
	float3 view;
	float3 up;
	float2 resolution;
	float2 fov;
	float apertureRadius;
	float focalDistance;
} Camera;

typedef struct HitInfo {

	int  triangle_ID, light_ID;
	float4 hit_point;  // point of intersection.
	float4 normal;

} HitInfo;

typedef struct AABB {
	float4 p_min;
	float4 p_max;
}AABB;

typedef struct BVHGPU {
	AABB aabb;          //32 
	int vert_list[10]; //40
	int child_idx;      //4
	int vert_len;       //4 - total 80
} BVHGPU;

typedef struct Triangle {

	float4 v1;
	float4 v2;
	float4 v3;
	float4 vn1;
	float4 vn2;
	float4 vn3;
	int matID;       // total size till here = 100 bytes
	float pad[3];    // padding 12 bytes - to make it 112 bytes (next multiple of 16
} Triangle;
//typedef struct Camera {
//	Mat4x4 view_mat;
//	float view_plane_dist;  // total 68 bytes
//	float pad[3];           // 12 bytes padding to reach 80 (next multiple of 16)
//} Camera;

// ************* HELPER FUNC ****************** //
//

uint wang_hash(uint seed)
/*See http://www.reedbeta.com/blog/2013/01/12/quick-and-easy-gpu-random-numbers-in-d3d11/ */
{
	seed = (seed ^ 61) ^ (seed >> 16);
	seed *= 9;
	seed = seed ^ (seed >> 4);
	seed *= 0x27d4eb2d;
	seed = seed ^ (seed >> 15);
	return seed;
}

static float get_random(unsigned int* seed0, unsigned int* seed1) {

	/* hash the seeds using bitwise AND operations and bitshifts */
	*seed0 = 36969 * ((*seed0) & 65535) + ((*seed0) >> 16);
	*seed1 = 18000 * ((*seed1) & 65535) + ((*seed1) >> 16);

	unsigned int ires = ((*seed0) << 16) + (*seed1);

	/* use union struct to convert int to float */
	union {
		float f;
		unsigned int ui;
	} res;

	res.ui = (ires & 0x007fffff) | 0x40000000;  /* bitwise AND, bitwise OR */
	return (res.f - 2.0f) / 2.0f;
}

// ******************** PATH TRACING ***********************
//

Ray createCamRay(const int x_coord, const int y_coord, const int width, const int height, __constant Camera* cam, const int* seed0, const int* seed1) {

	/* create a local coordinate frame for the camera */
	float3 rendercamview = cam->view; rendercamview = normalize(rendercamview);
	float3 rendercamup = cam->up; rendercamup = normalize(rendercamup);
	float3 horizontalAxis = cross(rendercamview, rendercamup); horizontalAxis = normalize(horizontalAxis);
	float3 verticalAxis = cross(horizontalAxis, rendercamview); verticalAxis = normalize(verticalAxis);

	float3 middle = cam->position + rendercamview;
	float3 horizontal = horizontalAxis * tan(cam->fov.x * 0.5f * (PI / 180));
	float3 vertical = verticalAxis * tan(cam->fov.y * -0.5f * (PI / 180));

	unsigned int x = x_coord;
	unsigned int y = y_coord;

	int pixelx = x_coord;
	int pixely = height - y_coord - 1;

	float sx = (float)pixelx / (width - 1.0f);
	float sy = (float)pixely / (height - 1.0f);

	float3 pointOnPlaneOneUnitAwayFromEye = middle + (horizontal * ((2 * sx) - 1)) + (vertical * ((2 * sy) - 1));
	float3 pointOnImagePlane = cam->position + ((pointOnPlaneOneUnitAwayFromEye - cam->position) * cam->focalDistance); /* cam->focalDistance */

	float3 aperturePoint;

	/* if aperture is non-zero (aperture is zero for pinhole camera), pick a random point on the aperture/lens */
	if (cam->apertureRadius > 0.00001f) {

		float random1 = get_random(seed0, seed1);
		float random2 = get_random(seed1, seed0);

		float angle = 2 * PI * random1;
		float distance = cam->apertureRadius * sqrt(random2);
		float apertureX = cos(angle) * distance;
		float apertureY = sin(angle) * distance;

		aperturePoint = cam->position + (horizontalAxis * apertureX) + (verticalAxis * apertureY);
	}
	else aperturePoint = cam->position;

	float3 apertureToImagePlane = pointOnImagePlane - aperturePoint; apertureToImagePlane = normalize(apertureToImagePlane);

	/* create camera ray*/
	Ray ray;
	ray.origin = aperturePoint;
	ray.dir = apertureToImagePlane;

	return ray;
}


/* (__global Sphere* sphere, const Ray* ray) */
float intersect_sphere(const Sphere* sphere, const Ray* ray) /* version using local copy of sphere */
{
	float3 rayToCenter = sphere->pos - ray->origin;
	float b = dot(rayToCenter, ray->dir);
	float c = dot(rayToCenter, rayToCenter) - sphere->radius * sphere->radius;
	float disc = b * b - c;

	if (disc < 0.0f) return 0.0f;
	else disc = sqrt(disc);

	if ((b - disc) > EPSILON) return b - disc;
	if ((b + disc) > EPSILON) return b + disc;

	return 0.0f;
}

bool intersect_scene(__constant Sphere* spheres, const Ray* ray, float* t, int* sphere_id, const int sphere_count)
{
	/* initialise t to a very large number,
	so t will be guaranteed to be smaller
	when a hit with the scene occurs */

	float inf = 1e20f;
	*t = inf;

	/* check if the ray intersects each sphere in the scene */
	for (int i = 0; i < sphere_count; i++) {

		Sphere sphere = spheres[i]; /* create local copy of sphere */

		/* float hitdistance = intersect_sphere(&spheres[i], ray); */
		float hitdistance = intersect_sphere(&sphere, ray);
		/* keep track of the closest intersection and hitobject found so far */
		if (hitdistance != 0.0f && hitdistance < *t) {
			*t = hitdistance;
			*sphere_id = i;
		}
	}
	return *t < inf; /* true when ray interesects the scene */
}


/* the path tracing function */
/* computes a path (starting from the camera) with a defined number of bounces, accumulates light/color at each bounce */
/* each ray hitting a surface will be reflected in a random direction (by randomly sampling the hemisphere above the hitpoint) */
/* small optimisation: diffuse ray directions are calculated using cosine weighted importance sampling */

float3 trace(__constant Sphere* spheres, const Ray* camray, const int sphere_count, const int* seed0, const int* seed1) {

	Ray ray = *camray;

	float3 accum_color = (float3)(0.0f, 0.0f, 0.0f);
	float3 mask = (float3)(1.0f, 1.0f, 1.0f);
	int* randSeed0 = seed0;
	int*
		randSeed1 = seed1;

	for (int bounces = 0; bounces < 8; bounces++) {

		float t;   /* distance to intersection */
		int hitsphere_id = 0; /* index of intersected sphere */

		/* update random number seeds for each bounce */
		/*randSeed0 += 1; */
		/*randSeed1 += 345;*/


		/* if ray misses scene, return background colour */
		if (!intersect_scene(spheres, &ray, &t, &hitsphere_id, sphere_count))
			return accum_color += mask * (float3)(0.15f, 0.15f, 0.25f);

		/* else, we've got a hit! Fetch the closest hit sphere */
		Sphere hitsphere = spheres[hitsphere_id]; /* version with local copy of sphere */

		/* compute the hitpoint using the ray equation */
		float3 hitpoint = ray.origin + ray.dir * t;

		/* compute the surface normal and flip it if necessary to face the incoming ray */
		float3 normal = normalize(hitpoint - hitsphere.pos);
		float3 normal_facing = dot(normal, ray.dir) < 0.0f ? normal : normal * (-1.0f);

		/* compute two random numbers to pick a random point on the hemisphere above the hitpoint*/
		float rand1 = get_random(randSeed0, randSeed1) * 2.0f * PI;
		float rand2 = get_random(randSeed1, randSeed0);
		float rand2s = sqrt(rand2);

		/* create a local orthogonal coordinate frame centered at the hitpoint */
		float3 w = normal_facing;
		float3 axis = fabs(w.x) > 0.1f ? (float3)(0.0f, 1.0f, 0.0f) : (float3)(1.0f, 0.0f, 0.0f);
		float3 u = normalize(cross(axis, w));
		float3 v = cross(w, u);

		/* use the coordinte frame and random numbers to compute the next ray direction */
		float3 newdir = normalize(u * cos(rand1) * rand2s + v * sin(rand1) * rand2s + w * sqrt(1.0f - rand2));

		/* add a very small offset to the hitpoint to prevent self intersection */
		ray.origin = hitpoint + normal_facing * EPSILON;
		ray.dir = newdir;

		/* add the colour and light contributions to the accumulated colour */
		accum_color += mask * hitsphere.emission;

		/* the mask colour picks up surface colours at each bounce */
		mask *= hitsphere.color;

		/* perform cosine-weighted importance sampling for diffuse surfaces*/
		mask *= dot(newdir, normal_facing);
	}

	return accum_color;
}

bool rayTriangleIntersection(Ray* ray, HitInfo* hit, __global Triangle* scene_data, int idx);
bool rayAabbIntersection(Ray* ray, AABB bb);
bool traverseBVH(Ray* ray, HitInfo* hit, int bvh_size, __global BVHGPU* bvh, __global Triangle* scene_data);
bool traceRay(Ray* ray, HitInfo* hit, int bvh_size, __global BVHGPU* bvh, int scene_size, __global Triangle* scene_data);

bool traceRay(Ray* ray, HitInfo* hit, int bvh_size, __global BVHGPU* bvh, int scene_size, __global Triangle* scene_data)
{
	bool flag = false;

	for (int i = 0; i < LIGHT_SIZE; i++)
	{
		float DdotN = dot(ray->dir, light_sources[i].normal);
		if (fabs(DdotN) > 0.0001)
		{
			float t = dot(light_sources[i].normal, light_sources[i].pos - ray->origin) / DdotN;
			if (t > 0.0 && t < ray->length)
			{
				float proj1, proj2, la, lb;
				float4 temp;

				temp = ray->origin + (ray->dir * t);
				temp = temp - light_sources[i].pos;
				proj1 = dot(temp, light_sources[i].edge_l);
				proj2 = dot(temp, light_sources[i].edge_w);
				la = length(light_sources[i].edge_l);
				lb = length(light_sources[i].edge_w);

				// Projection of the vector from rectangle corner to hitpoint on the edges.
				proj1 /= la;
				proj2 /= lb;

				if ((proj1 >= 0.0 && proj2 >= 0.0) && (proj1 <= la && proj2 <= lb))
				{
					ray->length = t;
					hit->hit_point.xyz = ray->origin + (ray->dir * t);
					hit->light_ID = i;
					hit->triangle_ID = -1;
					flag = true;
				}
			}
		}
	}
	//Traverse BVH if present, else brute force intersect all triangles...
	if (bvh_size > 0)
		flag |= traverseBVH(ray, hit, bvh_size, bvh, scene_data);
	else
	{
		for (int i = 0; i < scene_size; i++)
			flag |= rayTriangleIntersection(ray, hit, scene_data, i);
	}
	return flag;
}

bool traverseBVH(Ray* ray, HitInfo* hit, int bvh_size, __global BVHGPU* bvh, __global Triangle* scene_data)
{
	int candidate_list[HEAP_SIZE];
	candidate_list[0] = 0;
	int len = 1;
	bool intersect = false;

	if (!rayAabbIntersection(ray, bvh[0].aabb))
		return intersect;

	for (int i = 0; i < len && len < HEAP_SIZE; i++)
	{
		float c_idx = bvh[candidate_list[i]].child_idx;
		if (c_idx == -1 && bvh[candidate_list[i]].vert_len > 0)
		{
			for (int j = 0; j < bvh[candidate_list[i]].vert_len; j++)
			{
				intersect |= rayTriangleIntersection(ray, hit, scene_data, bvh[candidate_list[i]].vert_list[j]);
				//If shadow ray don't need to compute further intersections...
				if (ray->is_shadow_ray && intersect)
					return true;
			}
			continue;
		}

		for (int j = c_idx; j < c_idx + 2; j++)
		{
			AABB bb = { bvh[j].aabb.p_min, bvh[j].aabb.p_max };
			if ((bvh[j].vert_len > 0 || bvh[j].child_idx > 0) && rayAabbIntersection(ray, bb))
			{
				candidate_list[len] = j;
				len++;
			}
		}
	}
	return intersect;
}

bool rayTriangleIntersection(Ray* ray, HitInfo* hit, __global Triangle* scene_data, int idx)
{
	float4 v1v2 = scene_data[idx].v2 - scene_data[idx].v1;
	float4 v1v3 = scene_data[idx].v3 - scene_data[idx].v1;

	float4 pvec;
	pvec.xyz = cross(ray->dir, v1v3.xyz);
	float det = dot(v1v2, pvec);

	float inv_det = 1.0f / det;
	float4 dist;
	dist.xyz = ray->origin - scene_data[idx].v1.xyz;
	// dist.xyz = ray->origin - scene_data[idx].v1;
	float u = dot(pvec, dist) * inv_det;

	if (u < 0.0 || u > 1.0f)
		return false;

	float4 qvec = cross(dist, v1v2);
	float v = dot(qvec.xyz, ray->dir) * inv_det;

	if (v < 0.0 || u + v > 1.0)
		return false;

	float t = dot(v1v3, qvec) * inv_det;

	//BackFace Culling Algo
	/*
	if(det < EPSILON)
		continue;

	float4 dist = ray->origin - scene_data[idx].v1;
	float u = dot(pvec, dist);

	if(u < 0.0 || u > det)
		continue;

	float4 qvec = cross(dist, v1v2);
	float v = dot(qvec, ray->dir);

	if(v < 0.0 || u+v > det)
		continue;

	float t = dot(v1v3, qvec);

	float inv_det = 1.0f/det;
	t *= inv_det;
	u *= inv_det;
	v *= inv_det;*/

	if (t > 0 && t < ray->length)
	{
		ray->length = t;

		float4 N1 = normalize(scene_data[idx].vn1);
		float4 N2 = normalize(scene_data[idx].vn2);
		float4 N3 = normalize(scene_data[idx].vn3);

		float w = 1 - u - v;
		hit->hit_point.xyz = ray->origin + ray->dir * t;
		hit->normal = normalize(N1 * w + N2 * u + N3 * v);

		hit->triangle_ID = idx;
		hit->light_ID = -1;
		return true;
	}
	return false;
}

bool rayAabbIntersection(Ray* ray, AABB bb)
{
	float t_max = INFINITY, t_min = -INFINITY;
	float3 dir_inv = 1 / ray->dir.xyz;

	float3 min_diff = (bb.p_min.xyz - ray->origin).xyz * dir_inv;
	float3 max_diff = (bb.p_max.xyz - ray->origin).xyz * dir_inv;

	if (!isnan(min_diff.x))
	{
		t_min = fmax(min(min_diff.x, max_diff.x), t_min);
		t_max = min(fmax(min_diff.x, max_diff.x), t_max);
	}

	if (!isnan(min_diff.y))
	{
		t_min = fmax(min(min_diff.y, max_diff.y), t_min);
		t_max = min(fmax(min_diff.y, max_diff.y), t_max);
	}
	if (t_max < t_min)
		return false;

	if (!isnan(min_diff.z))
	{
		t_min = fmax(min(min_diff.z, max_diff.z), t_min);
		t_max = min(fmax(min_diff.z, max_diff.z), t_max);
	}
	return (t_max > fmax(t_min, 0.0f));
}











// union Colour { float c; uchar4 components; };

__kernel void
/*render_kernel(__global float3* output, int width, int height, int rendermode)*/
render_kernel(
	__write_only image2d_t outputImage, __read_only image2d_t inputImage, int reset,
	__constant Sphere* spheres, const int width, const int height,
	const int sphere_count, const int framenumber, __constant const Camera* cam,
	float random0, float random1)
{
	const uint hashedframenumber = wang_hash(framenumber);
	const int img_width = get_image_width(outputImage);
	const int img_height = get_image_height(outputImage);

	const int work_item_id = get_global_id(0);		/* the unique global id of the work item for the current pixel */
	int x_coord = work_item_id % img_width;					/* x-coordinate of the pixel */
	int y_coord = work_item_id / img_width;					/* y-coordinate of the pixel */
	int2 pixel = (int2) (x_coord, y_coord);
	int2 real_pixel = (int2) (x_coord, img_height - y_coord);

	unsigned int seed0 = x_coord * framenumber % 1000 + (random0 * 100);
	unsigned int seed1 = y_coord * framenumber % 1000 + (random1 * 100);

	float fx = (float)x_coord / (float)width;  /* convert int in range [0 - width] to float in range [0-1] */
	float fy = (float)y_coord / (float)height; /* convert int in range [0 - height] to float in range [0-1] */

	/*create a camera ray */
	struct Ray camray = createCamRay(x_coord, y_coord, width, height, cam, &seed0, &seed1);

	/* add the light contribution of each sample and average over all samples*/
	float3 finalcolor = (float3)(0.0f, 0.0f, 0.0f);
	float invSamples = 1.0f / SAMPLES;

	for (int i = 0; i < SAMPLES; i++)
		finalcolor += trace(spheres, &camray, sphere_count, &seed0, &seed1) * invSamples;


	float4 colorf4 = (float4)(0.0f, 0.0f, 0.0f, 1.0f);
	colorf4.x = finalcolor.x;
	colorf4.y = finalcolor.y;
	colorf4.z = finalcolor.z;


	if (reset == 1) {
		write_imagef(outputImage, pixel, colorf4);
	}
	else {
		float4 prev_color = read_imagef(inputImage, sampler, pixel);
		int num_passes = prev_color.w;

		colorf4 += (prev_color * num_passes);
		colorf4 /= (num_passes + 1);
		colorf4.w = num_passes + 1;
		write_imagef(outputImage, pixel, colorf4);
	}
}
