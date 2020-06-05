/* OpenCL based simple sphere path tracer by Sam Lapere, 2016*/
/* based on smallpt by Kevin Beason */
/* http://raytracey.blogspot.com */
/* interactive camera and depth-of-field code based on GPU path tracer by Karl Li and Peter Kutz */

__constant float EPSILON = 0.00003f; /* req2uired to compensate for limited float precision */
__constant float PI = 3.14159265359f;
__constant int SAMPLES = 4;

__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
CLK_ADDRESS_CLAMP_TO_EDGE |
CLK_FILTER_NEAREST;

typedef struct Ray {
	float3 origin;
	float3 dir;
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

float dai_float_01(uint seed)
{
	seed = wang_hash(seed);
	seed = seed^7 % 50600202;
	seed = (float)seed / 50600202.1;
	return seed;
}

int dai_int_05(uint seed)
{
	seed = wang_hash(seed);
	
	return ((int)seed%10)-5;
}

static float get_random(unsigned int* seed0, unsigned int* seed1) {

	/* hash the seeds using bitwise AND operations and bitshifts */
	*seed0 = 36969 * ((*seed0)^9 & 2147483647) + ((*seed0) >> 16);
	*seed1 = 18000 * ((*seed1)^9 & 2147483647) + ((*seed1) >> 16);

	unsigned int ires = ((*seed0) << 16) + (*seed1);

	/* use union struct to convert int to float */
	union {
		float f;
		unsigned int ui;
	} res;

	res.ui = (ires & 0x007fffff) | 0x40000000;  /* bitwise AND, bitwise OR */
	return (res.f - 2.0f) / 2.0f;
}

Ray createCamRay(const float x_coord, const float y_coord, const int width, const int height, __constant Camera* cam, const int* seed0, const int* seed1) {

	/* create a local coordinate frame for the camera */
	float3 rendercamview = cam->view; rendercamview = normalize(rendercamview);
	float3 rendercamup = cam->up; rendercamup = normalize(rendercamup);
	float3 horizontalAxis = cross(rendercamview, rendercamup); horizontalAxis = normalize(horizontalAxis);
	float3 verticalAxis = cross(horizontalAxis, rendercamview); verticalAxis = normalize(verticalAxis);

	float3 middle = cam->position + rendercamview;
	float3 horizontal = horizontalAxis * tan(cam->fov.x * 0.5f * (PI / 180));
	float3 vertical = verticalAxis * tan(cam->fov.y * -0.5f * (PI / 180));

	/*unsigned int x = x_coord;
	unsigned int y = y_coord;*/

	float pixelx = x_coord;
	float pixely = height - y_coord - 1;

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

struct Ray createCamRay_simple(const int x_coord, const int y_coord, const int width, const int height){

	float fx = (float)x_coord / (float)width;  /* convert int in range [0 - width] to float in range [0-1] */
	float fy = (float)y_coord / (float)height; /* convert int in range [0 - height] to float in range [0-1] */

	/* calculate aspect ratio */
	float aspect_ratio = (float)(width) / (float)(height);
	float fx2 = (fx - 0.5f) * aspect_ratio;
	float fy2 = fy - 0.5f;

	/* determine position of pixel on screen */
	float3 pixel_pos = (float3)(fx2, -fy2, 0.0f);

	/* create camera ray*/
	struct Ray ray;
	ray.origin = (float3)(0.0f, 0.0f, 40.0f); /* fixed camera position */
	ray.dir = normalize(pixel_pos - ray.origin); /* ray direction is vector from camera to pixel */

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

	for (int bounces = 0; bounces < 50; bounces++) {

		float t;   /* distance to intersection */
		int hitsphere_id = 0; /* index of intersected sphere */

		/* update random number seeds for each bounce */
		/*randSeed0 += 1; */
		/*randSeed1 += 345;*/


		/* if ray misses scene, return background colour */
		if (!intersect_scene(spheres, &ray, &t, &hitsphere_id, sphere_count))
			return accum_color += mask * (float3)(0.15f, 0.15f, 0.15f);

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

union Colour { float c; uchar4 components; };

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
	int2 real_pixel = (int2) (x_coord, img_height-y_coord);

	unsigned int seed0 = x_coord * framenumber % 20081931 + (random0 * 97);
	unsigned int seed1 = y_coord * framenumber % 20081931 + (random1 * 97);

	

	float fx = (float)x_coord / (float)width;  /* convert int in range [0 - width] to float in range [0-1] */
	float fy = (float)y_coord / (float)height; /* convert int in range [0 - height] to float in range [0-1] */

	/*create a camera ray */
	struct Ray camray = createCamRay(x_coord, y_coord, width, height, cam, &seed0, &seed1);

	/* add the light contribution of each sample and average over all samples*/
	float3 finalcolor = (float3)(0.0f, 0.0f, 0.0f);
	float invSamples = 1.0f / SAMPLES;

	int supersamplenumber = 10;
	uint seedsupersampling = 2021;
	for(int j = 0; j < supersamplenumber;j++){
		float offsetdaix = dai_float_01(seedsupersampling)*0.113;
		seedsupersampling = wang_hash(seedsupersampling);
		float offsetdaiy = dai_float_01(seedsupersampling)*0.113;
		seedsupersampling = wang_hash(seedsupersampling);
		struct Ray camray = createCamRay(x_coord + offsetdaix, y_coord+offsetdaiy, width, height, cam, &seed0, &seed1);
		for (int i = 0; i < SAMPLES; i++){
			finalcolor += trace(spheres, &camray, sphere_count, &seed0, &seed1) * invSamples;
		}
	}
	finalcolor = finalcolor/supersamplenumber;


	float4 colorf4 = (float4)(0.0f, 0.0f, 0.0f, 1.0f);
	colorf4.x = finalcolor.x;
	colorf4.y = finalcolor.y;
	colorf4.z = finalcolor.z;


	if (reset == 1){
		write_imagef(outputImage, pixel, colorf4);
	}
	else{
		float4 prev_color = read_imagef(inputImage, sampler, pixel);
		int num_passes = prev_color.w;

		colorf4 += (prev_color * num_passes);
		colorf4 /= (num_passes + 1);
		colorf4.w = num_passes + 1;
		write_imagef(outputImage, pixel, colorf4);
	}
}

//
__kernel void render_kernel(
	__write_only image2d_t outputImage, __read_only image2d_t inputImage, int reset,
	__constant Sphere* spheres, const int width, const int height,
	const int sphere_count, const int framenumber, __read_only const Camera* cam,
	float random0, float random1)
{
	const uint hashedframenumber =  wang_hash(framenumber);
	/*int img_width = get_image_width(outputImage);
	int img_height = get_image_height(outputImage);*/
	unsigned int work_item_id = get_global_id(0);	/* the unique global id of the work item for the current pixel */
	unsigned int x_coord = work_item_id % width;			/* x-coordinate of the pixel */
	unsigned int y_coord = work_item_id / width;			/* y-coordinate of the pixel */
	int2 pixel = (int2) (x_coord, y_coord);
	/* seeds for random number generator */

	unsigned int seed0 = x_coord * framenumber % 20080808 + (random0 * 101);
	unsigned int seed1 = y_coord * framenumber % 20080808 + (random1 * 101);


	/* add the light contribution of each sample and average over all samples*/
	float3 finalcolor = (float3)(0.0f, 0.0f, 0.0f);
	float invSamples = 1.0f / SAMPLES;

	for (unsigned int i = 0; i < SAMPLES; i++) {
		Ray camray = createCamRay(x_coord, y_coord, width, height, cam, &seed0, &seed1);
		finalcolor += trace(spheres, &camray, sphere_count, &seed0, &seed1) * invSamples;
	}

	/* add pixel colour to accumulation buffer (accumulates all samples) */
	/* inputImage[work_item_id] += finalcolor;*/
	/* averaged colour: divide colour by the number of calculated frames so far */

	/*union Colour fcolor;
	fcolor.components = (uchar4)(
		(unsigned char)(tempcolor.x * 255),
		(unsigned char)(tempcolor.y * 255),
		(unsigned char)(tempcolor.z * 255),
		1);*/

	/* store the pixelcolour in the output buffer */
	/* output[work_item_id] = (float3)(x_coord, y_coord, fcolor.c);*/
	float4 colorf4 = (float4)(0.0f, 0.0f, 0.0f, 1.0f);
	colorf4.x = finalcolor.x;
	colorf4.y = finalcolor.y;
	colorf4.z = finalcolor.z;
	
	if (reset == 1)
	{
		if(x_coord < 100 || x_coord > 800) colorf4 = (float4)((float)seed0 / 1000, 0.0f, 0.0f, 1.0f);
		write_imagef(outputImage, pixel, colorf4);
	}
	else
	{
		float4 prev_color = read_imagef(inputImage, sampler, pixel);
		int num_passes = prev_color.w;

		colorf4 += (prev_color * num_passes);
		colorf4 /= (num_passes + 1);
		colorf4.w = num_passes + 1;
		if(x_coord < 100 || x_coord > 800) colorf4 = (float4)(0.0f, (float)seed0 / 1000, 0.0f, 1.0f);
		write_imagef(outputImage, pixel, colorf4);
	}
}
