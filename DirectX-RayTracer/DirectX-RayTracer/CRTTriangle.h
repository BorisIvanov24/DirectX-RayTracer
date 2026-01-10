#pragma once
#include "CRTVector.h"

class CRTTriangle
{
public:

	static constexpr int vertsInTriangle = 3;

	CRTTriangle(const CRTVector& v0, const CRTVector& v1, const CRTVector& v2);
	CRTTriangle() = default;

	const CRTVector& getNormal() const;

	const CRTVector& getVertex(int index) const;

	friend bool operator==(const CRTTriangle& lhs, const CRTTriangle& rhs);

private:
	void calculateNormal();

	CRTVector verts[vertsInTriangle];
	CRTVector normal;
};

