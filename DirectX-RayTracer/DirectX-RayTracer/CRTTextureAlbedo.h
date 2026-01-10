#pragma once
#include "CRTTexture.h"

class CRTTextureAlbedo : public CRTTexture
{
public:
	CRTTextureAlbedo(const CRTVector& albedo, const std::string& name);

	CRTVector getColor(float u = 0.f, float v = 0.f) const override;
private:
	CRTVector albedo;
};

