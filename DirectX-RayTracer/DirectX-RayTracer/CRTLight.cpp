#include "CRTLight.h"

CRTLight::CRTLight(const CRTVector& position, float intensity)
{
	this->position = position;
	this->intensity = intensity;
}

const CRTVector& CRTLight::getPosition() const
{
	return position;
}

float CRTLight::getIntensity() const
{
	return intensity;
}
