#pragma once
#include "CRTVector.h"
#include "CRTMatrix.h"

class CRTCamera
{
public:
	void pan(const float degrees);
	void tilt(const float degrees);
	void roll(const float degrees);

	void panAroundTarget(const float degrees, const CRTVector& target);

	const CRTVector& getPosition() const;
	const CRTMatrix& getRotationMatrix() const;

	void setRotationMatrix(const CRTMatrix& matrix);
	void setPosition(const CRTVector& position);
private:
	CRTMatrix rotationMatrix;

	CRTVector position;
};

