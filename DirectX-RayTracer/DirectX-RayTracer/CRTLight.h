#pragma once
#include "CRTVector.h"

class CRTLight
{
public:
	CRTLight(const CRTVector& position, float intensity);

	const CRTVector& getPosition() const;

	float getIntensity() const;

private:
	CRTVector position;
	float intensity;
};

