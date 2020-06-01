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

bool intersect_sphere_t(const struct Sphere* sphere, const struct Ray* ray, float* t)
{
	float3 rayToCenter = sphere->pos - ray->origin;

	/* calculate coefficients a, b, c from quadratic equation */

	/* float a = dot(ray->dir, ray->dir); // ray direction is normalised, dotproduct simplifies to 1 */ 
	float b = dot(rayToCenter, ray->dir);
	float c = dot(rayToCenter, rayToCenter) - sphere->radius*sphere->radius;
	float disc = b * b - c; /* discriminant of quadratic formula */

	/* solve for t (distance to hitpoint along ray) */

	if (disc < 0.0f) return false;
	else *t = b - sqrt(disc);

	if (*t < 0.0f){
		*t = b + sqrt(disc);
		if (*t < 0.0f) return false; 
	}

	else return true;
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
	int randSeed0 = seed0;
	int randSeed1 = seed1;

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

union Colour { float c; uchar4 components; };

__kernel void 
/*render_kernel(__global float3* output, int width, int height, int rendermode)*/
render_kernel(
	__write_only image2d_t outputImage, __read_only image2d_t inputImage, int reset,
	__constant Sphere* spheres, const int width, const int height,
	const int sphere_count, const int framenumber, __constant const Camera* cam,
	float random0, float random1, const int hashedframenumber)
{
	const int rendermode = 6;
	const int work_item_id = get_global_id(0);		/* the unique global id of the work item for the current pixel */
	int x_coord = work_item_id % width;					/* x-coordinate of the pixel */
	int y_coord = work_item_id / width;					/* y-coordinate of the pixel */
	int2 pixel = (int2) (x_coord, y_coord);

	float fx = (float)x_coord / (float)width;  /* convert int in range [0 - width] to float in range [0-1] */
	float fy = (float)y_coord / (float)height; /* convert int in range [0 - height] to float in range [0-1] */

	/*create a camera ray */
	struct Ray camray = createCamRay_simple(x_coord, y_coord, width, height);

	/* create and initialise a sphere */
	struct Sphere sphere1;
	sphere1.radius = 0.4f;
	sphere1.pos = (float3)(0.0f, 0.0f, 3.0f);
	sphere1.color = (float3)(0.9f, 0.3f, 0.0f);

	/* intersect ray with sphere */
	float t = 1e20;
	intersect_sphere_t(&sphere1, &camray, &t);

	float4 colorf4 = (float4)(0.0f, 0.0f, 0.0f, 1.0f);

	/* if ray misses sphere, return background colour 
	background colour is a blue-ish gradient dependent on image height */
	if (t > 1e19 && rendermode != 1){ 
		colorf4 = (float4)(fy * 0.1f, fy * 0.3f, 0.3f, 1.0f);
		write_imagef(outputImage, pixel, colorf4);
		return;
	}

	/* for more interesting lighting: compute normal 
	and cosine of angle between normal and ray direction */
	float3 hitpoint = camray.origin + camray.dir * t;
	float3 normal = normalize(hitpoint - sphere1.pos);
	float cosine_factor = dot(normal, camray.dir) * -1.0f;
	
	float3 colorf3 = sphere1.color * cosine_factor;

	/* six different rendermodes */
	if (rendermode == 1) colorf3 = (float3)(fx, fy, 0); /* simple interpolated colour gradient based on pixel coordinates */
	else if (rendermode == 2) colorf3 = sphere1.color;  /* raytraced sphere with plain colour */
	else if (rendermode == 3) colorf3 = sphere1.color * cosine_factor; /* with cosine weighted colour */
	else if (rendermode == 4) colorf3 = sphere1.color * cosine_factor * sin(80 * fy); /* with sinusoidal stripey pattern */
	else if (rendermode == 5) colorf3 = sphere1.color * cosine_factor * sin(400 * fy) * sin(400 * fx); /* with grid pattern */
	else colorf3 = normal * 0.5f + (float3)(0.5f, 0.5f, 0.5f); /* with normal colours */

	colorf4.x = colorf3.x;
	colorf4.y = colorf3.y;
	colorf4.z = colorf3.z;
	colorf4.x = 1.0f;

	write_imagef(outputImage, pixel, colorf4);
}

//
__kernel void render_kernel(
	__write_only image2d_t outputImage, __read_only image2d_t inputImage, int reset,
	__constant Sphere* spheres, const int width, const int height,
	const int sphere_count, const int framenumber, __constant const Camera* cam,
	float random0, float random1, const int hashedframenumber)
{
	/*int img_width = get_image_width(outputImage);
	int img_height = get_image_height(outputImage);*/
	unsigned int work_item_id = get_global_id(0);	/* the unique global id of the work item for the current pixel */
	unsigned int x_coord = work_item_id % width;			/* x-coordinate of the pixel */
	unsigned int y_coord = work_item_id / width;			/* y-coordinate of the pixel */
	int2 pixel = (int2) (x_coord, y_coord);
	/* seeds for random number generator */

	unsigned int seed0 = x_coord * framenumber % 1000 + (random0 * 100);
	unsigned int seed1 = y_coord * framenumber % 1000 + (random1 * 100);


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
