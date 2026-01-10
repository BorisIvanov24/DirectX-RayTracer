#pragma once
#include "CRTTexture.h"
class CRTTextureChecker : public CRTTexture
{
public:
	CRTTextureChecker(const CRTVector& colorA, const CRTVector& colorB,
		float squareSize, const std::string& name);
	CRTVector getColor(float u = 0.f, float v = 0.f) const override;
	const char* getType() const override;

private:
	CRTVector colorA, colorB;
	float squareSize;
};

