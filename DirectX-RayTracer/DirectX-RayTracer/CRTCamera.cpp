#define _USE_MATH_DEFINES

#include "CRTCamera.h"
#include <cmath>
#include "CRTLight.h"
#include <algorithm>

void CRTCamera::pan(const float degrees)
{
	const float rads = degrees * (M_PI / 180.f);
	const CRTMatrix rotateAroundY(
		cosf(rads), 0.f, -sinf(rads),
		0.f, 1.f, 0.f,
		sinf(rads), 0.f, cosf(rads)
	);

	rotationMatrix = rotationMatrix * rotateAroundY;
}

void CRTCamera::tilt(const float degrees)
{
	const float rads = degrees * (M_PI / 180.f);
	const CRTMatrix rotateAroundX(
		1.f, 0.f, 0.f,
		0.f, cosf(rads), -sinf(rads),
		0.f, sinf(rads), cosf(rads)
	);

	rotationMatrix = rotationMatrix * rotateAroundX;
}

void CRTCamera::roll(const float degrees)
{
	const float rads = degrees * (M_PI / 180.f);
	const CRTMatrix rotateAroundZ(
		cosf(rads), -sinf(rads), 0.f,
		sinf(rads), cosf(rads), 0.f,
		0.f, 0.f, 1.f
	);

	rotationMatrix = rotationMatrix * rotateAroundZ;
}

void CRTCamera::rotate(float deltaYawDeg, float deltaPitchDeg)
{
	const float DEG2RAD = 3.14159265359f / 180.0f;
	yaw += deltaYawDeg * DEG2RAD;
	pitch += deltaPitchDeg * DEG2RAD;

	// Clamp pitch to avoid flipping over
	const float maxPitch = 89.f * DEG2RAD;
	const float minPitch = -89.f * DEG2RAD;
	pitch = std::clamp(pitch, minPitch, maxPitch);

	// Forward vector in world space
	float fx = cos(pitch) * sin(yaw);
	float fy = sin(pitch);
	float fz = -cos(pitch) * cos(yaw); // forward = -Z in camera space
	CRTVector forward(fx, fy, fz);
	forward.normalise();

	// Right and up vectors
	CRTVector worldUp(0.f, 1.f, 0.f);
	CRTVector right = cross(worldUp, forward);
	right.normalise();
	CRTVector up = cross(forward, right);

	// Build rotationMatrix columns = right, up, forward
	rotationMatrix = CRTMatrix(
		right.getX(), up.getX(), forward.getX(),
		right.getY(), up.getY(), forward.getY(),
		right.getZ(), up.getZ(), forward.getZ()
	);
}

void CRTCamera::moveForward(float distance)
{
	// Forward = column 2 of rotationMatrix
	CRTVector forward(
		rotationMatrix.get(0, 2),
		rotationMatrix.get(1, 2),
		rotationMatrix.get(2, 2)
	);

	position = position + forward * distance;
}

void CRTCamera::moveRight(float distance)
{
	// Right = column 0 of rotationMatrix
	CRTVector right(
		rotationMatrix.get(0, 0),
		rotationMatrix.get(1, 0),
		rotationMatrix.get(2, 0)
	);

	position = position + right * distance;
}

void CRTCamera::panAroundTarget(const float degrees, const CRTVector& target)
{

	CRTVector toCamera = position - target;

	const float rads = degrees * (M_PI / 180.f);
	const CRTMatrix rotateAroundY(
		cosf(rads), 0.f, -sinf(rads),
		0.f, 1.f, 0.f,
		sinf(rads), 0.f, cosf(rads)
	);

	CRTVector rotated = toCamera * rotateAroundY;

	position = target + rotated;

	rotationMatrix = rotationMatrix * rotateAroundY;
}

const CRTVector& CRTCamera::getPosition() const
{
	return position;
}

const CRTMatrix& CRTCamera::getRotationMatrix() const
{
	return rotationMatrix;
}

void CRTCamera::setRotationMatrix(const CRTMatrix& matrix)
{
	rotationMatrix = matrix;
}

void CRTCamera::setPosition(const CRTVector& position)
{
	this->position = position;
}
