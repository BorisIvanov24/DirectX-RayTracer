#include "CRTVector.h"
#include <math.h>
#include <iostream>

CRTVector::CRTVector() : x(0.f), y(0.f), z(0.f)
{
}

CRTVector::CRTVector(float x, float y, float z) : x(x), y(y), z(z)
{
}

float CRTVector::length() const
{
	return sqrtf((x * x) + (y * y) + (z * z));
}

void CRTVector::normalise()
{
	float len = length();

	x /= len;
	y /= len;
	z /= len;
}

float CRTVector::getX() const
{
	return x;
}

float CRTVector::getY() const
{
	return y;
}

float CRTVector::getZ() const
{
	return z;
}

float CRTVector::getByIndex(int index) const
{
	if (index == 0)
		return x;

	if (index == 1)
		return y;

	return z;
}

CRTVector operator*(const CRTVector& vec, float scalar)
{
	return CRTVector(
		vec.x * scalar,
		vec.y * scalar,
		vec.z * scalar
	);
}

CRTVector cross(const CRTVector& lhs, const CRTVector& rhs)
{
	return CRTVector(
		lhs.y * rhs.z - lhs.z * rhs.y,
		lhs.z * rhs.x - lhs.x * rhs.z,
		lhs.x * rhs.y - lhs.y * rhs.x
	);
}

float dot(const CRTVector& lhs, const CRTVector& rhs)
{
	return (lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z);
}

bool operator==(const CRTVector& lhs, const CRTVector& rhs) {
	const float epsilon = 1e-6f;
	return std::fabs(lhs.getX() - rhs.getX()) < epsilon &&
		std::fabs(lhs.getY() - rhs.getY()) < epsilon &&
		std::fabs(lhs.getZ() - rhs.getZ()) < epsilon;
}

CRTVector operator+(const CRTVector& lhs, const CRTVector& rhs)
{
	return CRTVector(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z);
}

CRTVector operator-(const CRTVector& lhs, const CRTVector& rhs)
{
	return CRTVector(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z);
}

CRTVector operator*(float scalar, const CRTVector& vec)
{
	return CRTVector(
		vec.x * scalar,
		vec.y * scalar,
		vec.z * scalar
	);
}

void CRTVector::print(std::ostream& os) const
{
	os << "( " << x << ", " << y << ", " << z << " )" << std::endl;
}
