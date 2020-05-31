#include "camera.h"

// constructor and default values
InteractiveCamera::InteractiveCamera()
{
	centerPosition = Vector3Df(0, 0, 0);
	yaw = 0;
	pitch = 0.3;
	radius = 4;
	apertureRadius = 0.01; // 0.04
	focalDistance = 4.0f;

	resolution = Vector2Df(512, 512);  // width, height
	fov = Vector2Df(40, 40);
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
	Vector3Df strafeAxis = cross(viewDirection, Vector3Df(0, 1, 0));
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
	Vector3Df directionToCamera = Vector3Df(xDirection, yDirection, zDirection);
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
	resolution = Vector2Df(x, y);
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
	Vector3Df directionToCamera = Vector3Df(xDirection, yDirection, zDirection);
	viewDirection = directionToCamera * (-1.0);
	Vector3Df eyePosition = centerPosition + directionToCamera * radius;
	//Vector3Df eyePosition = centerPosition; // rotate camera from stationary viewpoint

	renderCamera->position = eyePosition;
	renderCamera->view = viewDirection;
	renderCamera->up = Vector3Df(0, 1, 0);
	renderCamera->resolution = Vector2Df(resolution.x, resolution.y);
	renderCamera->fov = Vector2Df(fov.x, fov.y);
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