#include "cl_ext/camera.h"
#include "cl_ext/cl_convert.h"
// constructor and default values
InteractiveCamera::InteractiveCamera()
{
	centerPosition = vcl::vec3(0, 0, 0);
	yaw = 0;
	pitch = 0.3;
	radius = 4;
	apertureRadius = 0.01; // 0.04
	focalDistance = 4.0f;

	resolution = vcl::vec2(512, 512);  // width, height
	fov = vcl::vec2(40, 40);
}

InteractiveCamera::~InteractiveCamera() {}

void InteractiveCamera::changeYaw(float m) {
	yaw += m;
	fixYaw();
}

void InteractiveCamera::changePitch(float m) {
	pitch += m;
	fixPitch();
}

void InteractiveCamera::changeRadius(float m) {
	radius += radius * m; // Change proportional to current radius. Assuming radius isn't allowed to go to zero.
	fixRadius();
}

void InteractiveCamera::changeAltitude(float m) {
	centerPosition.y += m;
	//fixCenterPosition();
}

void InteractiveCamera::goForward(float m) {
	centerPosition += viewDirection * m;
}

void InteractiveCamera::strafe(float m) {
	vcl::vec3 strafeAxis = cross(viewDirection, vcl::vec3(0, 1, 0));
	strafeAxis.normalize();
	centerPosition += strafeAxis * m;
}

void InteractiveCamera::rotateRight(float m) {
	float yaw2 = yaw;
	yaw2 += m;
	float pitch2 = pitch;
	float xDirection = sin(yaw2) * cos(pitch2);
	float yDirection = sin(pitch2);
	float zDirection = cos(yaw2) * cos(pitch2);
	vcl::vec3 directionToCamera = vcl::vec3(xDirection, yDirection, zDirection);
	viewDirection = directionToCamera * (-1.0);
}

void InteractiveCamera::changeApertureDiameter(float m) {
	apertureRadius += (apertureRadius + 0.01) * m; // Change proportional to current apertureRadius.
	fixApertureRadius();
}


void InteractiveCamera::changeFocalDistance(float m) {
	focalDistance += m;
	fixFocalDistance();
}


void InteractiveCamera::setResolution(float x, float y) {
	resolution = vcl::vec2(x, y);
	setFOVX(fov.x);
}

float radiansToDegrees(float radians) {
	float degrees = radians * 180.0 / M_PI;
	return degrees;
}

float degreesToRadians(float degrees) {
	float radians = degrees / 180.0 * M_PI;
	return radians;
}

void InteractiveCamera::setFOVX(float fovx) {
	fov.x = fovx;
	fov.y = radiansToDegrees(atan(tan(degreesToRadians(fovx) * 0.5) * (resolution.y / resolution.x)) * 2.0);
	// resolution float division
}

void InteractiveCamera::buildRenderCamera(Camera* renderCamera) {
	float xDirection = sin(yaw) * cos(pitch);
	float yDirection = sin(pitch);
	float zDirection = cos(yaw) * cos(pitch);
	vcl::vec3 directionToCamera = vcl::vec3(xDirection, yDirection, zDirection);
	viewDirection = directionToCamera * (-1.0);
	vcl::vec3 eyePosition = centerPosition + directionToCamera * radius;
	//Vector3Df eyePosition = centerPosition; // rotate camera from stationary viewpoint

	renderCamera->position = vcl_to_cl_f3(eyePosition);
	renderCamera->view = vcl_to_cl_f3(viewDirection);
	renderCamera->up = { 0, 1, 0 };
	renderCamera->resolution = { resolution.x, resolution.y };
	renderCamera->fov = { fov.x, fov.y };
	renderCamera->apertureRadius = apertureRadius;
	renderCamera->focalDistance = focalDistance;
}

float mod(float x, float y) { // Does this account for -y ???
	return x - y * floorf(x / y);
}

void InteractiveCamera::fixYaw() {
	yaw = mod(yaw, 2 * M_PI); // Normalize the yaw.
}

float clamp2(float n, float low, float high) {
	n = fminf(n, high);
	n = fmaxf(n, low);
	return n;
}

void InteractiveCamera::fixPitch() {
	float padding = 0.05;
	pitch = clamp2(pitch, -PI_OVER_TWO + padding, PI_OVER_TWO - padding); // Limit the pitch.
}

void InteractiveCamera::fixRadius() {
	float minRadius = 0.2;
	float maxRadius = 100.0;
	radius = clamp2(radius, minRadius, maxRadius);
}

void InteractiveCamera::fixApertureRadius() {
	float minApertureRadius = 0.0;
	float maxApertureRadius = 25.0;
	apertureRadius = clamp2(apertureRadius, minApertureRadius, maxApertureRadius);
}

void InteractiveCamera::fixFocalDistance() {
	float minFocalDist = 0.2;
	float maxFocalDist = 100.0;
	focalDistance = clamp2(focalDistance, minFocalDist, maxFocalDist);
}