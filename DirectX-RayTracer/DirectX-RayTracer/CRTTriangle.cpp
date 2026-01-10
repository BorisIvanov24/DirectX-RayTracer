#include "CRTTriangle.h"

CRTTriangle::CRTTriangle(const CRTVector& v0, const CRTVector& v1, const CRTVector& v2)
{
	verts[0] = v0;
	verts[1] = v1;
	verts[2] = v2;

	calculateNormal();
}

const CRTVector& CRTTriangle::getNormal() const
{
	return normal;
}

const CRTVector& CRTTriangle::getVertex(int index) const
{
	return verts[index];
}

void CRTTriangle::calculateNormal()
{
	CRTVector E0 = verts[1] - verts[0];
	CRTVector E1 = verts[2] - verts[0];

	normal = cross(E0, E1);

	normal.normalise();
}

bool operator==(const CRTTriangle& lhs, const CRTTriangle& rhs)
{
	return (lhs.verts[0] == rhs.verts[0] && lhs.verts[1] == rhs.verts[1] && lhs.verts[2] == rhs.verts[2]);
}
