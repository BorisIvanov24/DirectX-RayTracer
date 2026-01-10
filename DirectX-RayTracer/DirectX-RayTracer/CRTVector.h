#pragma once
#include <fstream>

class CRTVector
{
public:
	CRTVector();
	CRTVector(float x, float y, float z);

	float length() const;

	void normalise();

	float getX() const;
	float getY() const;
	float getZ() const;
	float getByIndex(int index) const;

	friend CRTVector operator+(const CRTVector& lhs, const CRTVector& rhs);
	friend CRTVector operator-(const CRTVector& lhs, const CRTVector& rhs);


	friend CRTVector operator*(const CRTVector& vec, float scalar);
	friend CRTVector operator*(float scalar, const CRTVector& vec);

	friend CRTVector cross(const CRTVector& lhs, const CRTVector& rhs);
	friend float dot(const CRTVector& lhs, const CRTVector& rhs);
	friend bool operator==(const CRTVector& lhs, const CRTVector& rhs);

	void print(std::ostream& os) const;
private:
	float x, y, z;
};

