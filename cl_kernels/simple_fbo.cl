/* OpenCL based simple sphere path tracer by Sam Lapere, 2016*/
/* based on smallpt by Kevin Beason */
/* http://raytracey.blogspot.com */
/* interactive camera and depth-of-field code based on GPU path tracer by Karl Li and Peter Kutz */

__constant float EPSILON = 0.00003f; /* req2uired to compensate for limited float precision */
__constant float PI = 3.14159265359f;
__constant int SAMPLES = 4;

__constant int HDRwidth = 3200;
__constant int HDRheight = 1600;

__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
CLK_ADDRESS_CLAMP_TO_EDGE |
CLK_FILTER_NEAREST;

typedef struct Ray {
	float3 origin;
	float3 dir;
} Ray;

typedef struct HITRECORD{
	float3 p;
	float3 normal;
	int materialPara;
	int typePara;
	float hittime;
	float3 color;
	float3 emission;
} HITRECORD;
/* typePara: 0 means Sphere, 1 means triangle. materialPara: 0 means lambertian, 1 means metal*/
typedef struct Sphere {
	float radius;
	float3 pos;
	float3 color;
	float3 emission;
} Sphere;

typedef struct Triangle {
	float3 vertex1;
	float3 vertex2;
	float3 vertex3;
	float3 nvector;
	float3 color;
	float3 emission;
}Triangle;

typedef struct Object {
	Sphere sphere;
	Triangle triangle;
	int materialPara;
	int typePara;
} Object;

typedef struct Camera {
	float3 position;
	float3 view;
	float3 up;
	float2 resolution;
	float2 fov;
	float apertureRadius;
	float focalDistance;
} Camera;

/* Rignt Hand Systeme (Determinant>0)*/


float determinant3d(float3 v1, float3 v2, float3 v3) {
	return v1.x * (v2.y * v3.z - v3.y * v2.z) - v2.x * (v1.y * v3.z - v3.y * v1.z) + v3.x * (v1.y * v2.z - v2.y * v1.z);
}
void changeOrientation(Triangle* triangle) {
	if (determinant3d(triangle->vertex1, triangle->vertex2, triangle->vertex3) < 0) {
		float3 temp = triangle->vertex1;
		triangle->vertex1 = triangle->vertex2;
		triangle->vertex2 = temp;
	}
}

void TriangleInitialize(Triangle* triangle) {
	changeOrientation(triangle);
	triangle->nvector = normalize(cross(triangle->vertex2 - triangle->vertex1, triangle->vertex3 - triangle->vertex1));
}


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
	seed = seed ^ 7 % 50600202;
	seed = (float)seed / 50600202.1;
	return seed;
}

int dai_int_05(uint seed)
{
	seed = wang_hash(seed);

	return ((int)seed % 10) - 5;
}

static float get_random(unsigned int* seed0, unsigned int* seed1) {

	/* hash the seeds using bitwise AND operations and bitshifts */
	*seed0 = 36969 * ((*seed0) & 2147483641) + ((*seed0) >> 16);
	*seed1 = 18000 * ((*seed1) & 2147483641) + ((*seed1) >> 16);

	unsigned int ires = ((*seed0) << 16) + (*seed1);

	/* use union struct to convert int to float */
	union {
		float f;
		unsigned int ui;
	} res;

	res.ui = (ires & 0x007fffff) | 0x40000000;  /* bitwise AND, bitwise OR */
	return (res.f - 2.0f) / 2.0f;
}

Ray createCamRay(const float x_coord, const float y_coord, const int width, const int height, __constant Camera* cam, int* seed0, int* seed1) {

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

void diffuse_sphere(const Sphere sphere, Ray* ray, const float TIMEintersection, HITRECORD* hitrecord) {
	unsigned int randSeed00 = 2020;
	unsigned int randSeed10 = 1996;
	hitrecord->color = sphere.color;
	hitrecord->emission = sphere.emission;

	float3 hitpoint = ray->origin + ray->dir * TIMEintersection;
	hitrecord->hittime = TIMEintersection;
	hitrecord->p = hitpoint;
	/* compute the surface normal and flip it if necessary to face the incoming ray */
	float3 normal = normalize(hitpoint - sphere.pos);
	float3 normal_facing = dot(normal, ray->dir) < 0.0f ? normal : normal * (-1.0f);
	hitrecord->normal = normal_facing;


	float rand1 = get_random(&randSeed00, &randSeed10) * 2.0f * PI;
	float rand2 = get_random(&randSeed10, &randSeed00);
	float rand2s = sqrt(rand2);

	/* create a local orthogonal coordinate frame centered at the hitpoint */
	float3 w = normal_facing;
	float3 axis = fabs(w.x) > 0.1f ? (float3)(0.0f, 1.0f, 0.0f) : (float3)(1.0f, 0.0f, 0.0f);
	float3 u = normalize(cross(axis, w));
	float3 v = cross(w, u);

	/* use the coordinte frame and random numbers to compute the next ray direction */
	float3 newdir = normalize(u * cos(rand1) * rand2s + v * sin(rand1) * rand2s + w * sqrt(1.0f - rand2));

	/* add a very small offset to the hitpoint to prevent self intersection */
	ray->origin = hitpoint + normal_facing * EPSILON;
	ray->dir = newdir;
}

float intersect_triangle(const Triangle* triangle, const Ray* ray)
{
	float3 vec_test = triangle->vertex1 - ray->origin;

	if (dot(vec_test, ray->dir) <= EPSILON) return 0.0f;

	float3 vec_test_normalized = normalize(vec_test);
	float velocity = dot(vec_test_normalized, ray->dir);
	float TIMEintersect = sqrt(dot(vec_test, vec_test)) / velocity;

	float3 POINTintersect = ray->origin + TIMEintersect * ray->dir;

	/* Check step 1*/
	float3 e12 = normalize(triangle->vertex2 - triangle->vertex1);
	float3 e13 = normalize(triangle->vertex3 - triangle->vertex1);
	float cos_angle1 = dot(e12, e13);
	float3 vec_inersetcAnd1 = normalize(POINTintersect - triangle->vertex1);
	if (dot(vec_inersetcAnd1, e12) < cos_angle1 || dot(vec_inersetcAnd1, e13) < cos_angle1) return 0.0f;

	/* Check step 2*/
	float3 e32 = normalize(triangle->vertex2 - triangle->vertex3);
	float3 e31 = normalize(triangle->vertex1 - triangle->vertex3);
	float cos_angle3 = dot(e32, e31);
	float3 vec_inersetcAnd3 = normalize(POINTintersect - triangle->vertex1);
	if (dot(vec_inersetcAnd3, e31) < cos_angle3 || dot(vec_inersetcAnd1, e32) < cos_angle3) return 0.0f;

	return TIMEintersect;

}

bool intersect_scene(__constant Sphere* spheres, Ray* ray, float* t, int* sphere_id, const int sphere_count, HITRECORD* hitrecord)
{
	/* initialise t to a very large number,
	so t will be guaranteed to be smaller
	when a hit with the scene occurs */
	Ray ray_copy;
	ray_copy.dir = ray->dir;
	ray_copy.origin = ray->origin;


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
	if (*t < inf) {
		diffuse_sphere(spheres[*sphere_id], &ray, *t, hitrecord);
	}
	return *t < inf; /* true when ray interesects the scene */
}







/* the path tracing function */
/* computes a path (starting from the camera) with a defined number of bounces, accumulates light/color at each bounce */
/* each ray hitting a surface will be reflected in a random direction (by randomly sampling the hemisphere above the hitpoint) */
/* small optimisation: diffuse ray directions are calculated using cosine weighted importance sampling */

float3 trace(__constant Sphere* spheres, const Ray* camray, const int sphere_count,  int* seed0,  int* seed1, __constant float4* HDRimg) {
	HITRECORD hitrecord;
	Ray ray = *camray;

	float3 accum_color = (float3)(0.0f, 0.0f, 0.0f);
	float3 mask = (float3)(1.0f, 1.0f, 1.0f);
	/*int* randSeed0 = seed0;
	int* randSeed1 = seed1;*/

	for (int bounces = 0; bounces < 20; bounces++) {

		float t;   /* distance to intersection */
		int hitsphere_id = 0; /* index of intersected sphere */

		/* update random number seeds for each bounce */
		/*randSeed0 += 1; */
		/*randSeed1 += 345;*/


		/* if ray misses scene, return background colour */
		if (!intersect_scene(spheres, &ray, &t, &hitsphere_id, sphere_count, &hitrecord))
			//return accum_color += mask * (float3)(0.55f, 0.55f, 0.55f);
		{
			float longlatX = atan2(ray.dir.x, ray.dir.z); // Y is up, swap x for y and z for x
			longlatX = longlatX < 0.f ? longlatX + 2 * PI : longlatX;  // wrap around full circle if negative
			float longlatY = acos(ray.dir.y); // add RotateMap at some point, see Fragmentarium
			// map theta and phi to u and v texturecoordinates in [0,1] x [0,1] range
			float offsetY = 0.5f;
			float u = longlatX / 2 * PI; // +offsetY;
			float v = longlatY / PI;

			// map u, v to integer coordinates
			int u2 = (int)(u * HDRwidth); //% HDRwidth;
			int v2 = (int)(v * HDRheight); // % HDRheight;

			// compute the texel index in the HDR map 
			int HDRtexelidx = u2 + v2 * HDRwidth;

			float4 HDRcol = HDRimg[HDRtexelidx];
			//float4 HDRcol = tex1Dfetch(HDRtexture, HDRtexelidx);  // fetch from texture
			// float3 HDRcol2 = { HDRcol.x, HDRcol.y, HDRcol.z };
			// return (u, v, offsetY);
			return accum_color += mask * (float3)(0.55f, 0.55f, 0.55f);
			return HDRcol.xyz;
			// return accum_color += (mask * HDRcol.xyz);	
		}

		/* else, we've got a hit! Fetch the closest hit sphere */
		//Sphere hitsphere = spheres[hitsphere_id]; /* version with local copy of sphere */

		/* compute the hitpoint using the ray equation */
		// float3 hitpoint = ray.origin + ray.dir * t;

		// /* compute the surface normal and flip it if necessary to face the incoming ray */
		// float3 normal = normalize(hitpoint - hitsphere.pos);
		// float3 normal_facing = dot(normal, ray.dir) < 0.0f ? normal : normal * (-1.0f);

		/* compute two random numbers to pick a random point on the hemisphere above the hitpoint*/
		// float rand1 = get_random(randSeed0, randSeed1) * 2.0f * PI;
		// float rand2 = get_random(randSeed1, randSeed0);
		// float rand2s = sqrt(rand2);

		// /* create a local orthogonal coordinate frame centered at the hitpoint */
		// float3 w = normal_facing;
		// float3 axis = fabs(w.x) > 0.1f ? (float3)(0.0f, 1.0f, 0.0f) : (float3)(1.0f, 0.0f, 0.0f);
		// float3 u = normalize(cross(axis, w));
		// float3 v = cross(w, u);

		// /* use the coordinte frame and random numbers to compute the next ray direction */
		// float3 newdir = normalize(u * cos(rand1) * rand2s + v * sin(rand1) * rand2s + w * sqrt(1.0f - rand2));

		// /* add a very small offset to the hitpoint to prevent self intersection */
		// ray.origin = hitpoint + normal_facing * EPSILON;
		// ray.dir = newdir;

		/* add the colour and light contributions to the accumulated colour */
		accum_color += mask * hitrecord.emission;

		/* the mask colour picks up surface colours at each bounce */
		mask *= hitrecord.color;

		/* perform cosine-weighted importance sampling for diffuse surfaces*/
		mask *= dot(ray.dir, hitrecord.normal);
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
	float random0, float random1, __constant const float4* HDRimg)
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

	int supersamplenumber = 16;
	uint seedsupersampling = 2021;
	for (int j = 0; j < supersamplenumber; j++) {
		float offsetdaix = dai_float_01(seedsupersampling) / (img_width + 0.113);
		seedsupersampling = wang_hash(seedsupersampling);
		float offsetdaiy = dai_float_01(seedsupersampling) / (img_height + 0.113);
		seedsupersampling = wang_hash(seedsupersampling);
		struct Ray camray = createCamRay(x_coord + offsetdaix, y_coord + offsetdaiy, width, height, cam, &seed0, &seed1);
		for (int i = 0; i < SAMPLES; i++) {
			finalcolor += trace(spheres, &camray, sphere_count, &seed0, &seed1, HDRimg) * invSamples;
		}
	}
	finalcolor = finalcolor / supersamplenumber;


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
//__kernel void render_kernel(
//	__write_only image2d_t outputImage, __read_only image2d_t inputImage, int reset,
//	__constant Sphere* spheres, const int width, const int height,
//	const int sphere_count, const int framenumber, __read_only const Camera* cam,
//	float random0, float random1)
//{
//	const uint hashedframenumber =  wang_hash(framenumber);
//	/*int img_width = get_image_width(outputImage);
//	int img_height = get_image_height(outputImage);*/
//	unsigned int work_item_id = get_global_id(0);	/* the unique global id of the work item for the current pixel */
//	unsigned int x_coord = work_item_id % width;			/* x-coordinate of the pixel */
//	unsigned int y_coord = work_item_id / width;			/* y-coordinate of the pixel */
//	int2 pixel = (int2) (x_coord, y_coord);
//	/* seeds for random number generator */
//
//	unsigned int seed0 = x_coord * framenumber % 1000 + (random0 * 100);
//	unsigned int seed1 = y_coord * framenumber % 1000 + (random1 * 100);
//
//
//	/* add the light contribution of each sample and average over all samples*/
//	float3 finalcolor = (float3)(0.0f, 0.0f, 0.0f);
//	float invSamples = 1.0f / SAMPLES;
//
//	for (unsigned int i = 0; i < SAMPLES; i++) {
//		Ray camray = createCamRay(x_coord, y_coord, width, height, cam, &seed0, &seed1);
//		finalcolor += trace(spheres, &camray, sphere_count, &seed0, &seed1) * invSamples;
//	}
//
//	/* add pixel colour to accumulation buffer (accumulates all samples) */
//	/* inputImage[work_item_id] += finalcolor;*/
//	/* averaged colour: divide colour by the number of calculated frames so far */
//
//	/*union Colour fcolor;
//	fcolor.components = (uchar4)(
//		(unsigned char)(tempcolor.x * 255),
//		(unsigned char)(tempcolor.y * 255),
//		(unsigned char)(tempcolor.z * 255),
//		1);*/
//
//	/* store the pixelcolour in the output buffer */
//	/* output[work_item_id] = (float3)(x_coord, y_coord, fcolor.c);*/
//	float4 colorf4 = (float4)(0.0f, 0.0f, 0.0f, 1.0f);
//	colorf4.x = finalcolor.x;
//	colorf4.y = finalcolor.y;
//	colorf4.z = finalcolor.z;
//	
//	if (reset == 1)
//	{
//		if(x_coord < 100 || x_coord > 800) colorf4 = (float4)((float)seed0 / 1000, 0.0f, 0.0f, 1.0f);
//		write_imagef(outputImage, pixel, colorf4);
//	}
//	else
//	{
//		float4 prev_color = read_imagef(inputImage, sampler, pixel);
//		int num_passes = prev_color.w;
//
//		colorf4 += (prev_color * num_passes);
//		colorf4 /= (num_passes + 1);
//		colorf4.w = num_passes + 1;
//		if(x_coord < 100 || x_coord > 800) colorf4 = (float4)(0.0f, (float)seed0 / 1000, 0.0f, 1.0f);
//		write_imagef(outputImage, pixel, colorf4);
//	}
//}
