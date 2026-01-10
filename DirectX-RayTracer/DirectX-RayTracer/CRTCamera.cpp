#define _USE_MATH_DEFINES

#include "CRTCamera.h"
#include <cmath>
#include "CRTLight.h"

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
