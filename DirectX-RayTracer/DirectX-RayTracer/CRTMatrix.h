#pragma once
#include "CRTVector.h"

class CRTMatrix
{
public:

	CRTMatrix();
	CRTMatrix(float c00, float c01, float c02,
		float c10, float c11, float c12,
		float c20, float c21, float c22);

	friend CRTMatrix operator*(const CRTMatrix& lhs, const CRTMatrix& rhs);
	friend CRTVector operator*(const CRTVector& lhs, const CRTMatrix& rhs);

	void print() const;
private:
	float m[3][3];
};

