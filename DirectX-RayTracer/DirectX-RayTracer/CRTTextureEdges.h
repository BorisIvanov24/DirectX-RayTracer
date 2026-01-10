#pragma once
#include "CRTTexture.h"
class CRTTextureEdges : public CRTTexture
{
public:
	CRTTextureEdges(const CRTVector& edgeColor, const CRTVector& innerColor,
		float edgeWidth, const std::string& name);

	CRTVector getColor(float u = 0.f, float v = 0.f) const override;

private:
	CRTVector edgeColor;
	CRTVector innerColor;
	float edgeWidth;
};

