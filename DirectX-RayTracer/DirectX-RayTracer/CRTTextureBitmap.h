#pragma once
#include "CRTTexture.h"
class CRTTextureBitmap : public CRTTexture
{
public:
	CRTTextureBitmap(const std::string& filepath, const std::string& name);
	CRTVector getColor(float u, float v) const override;

	const char* getType() const override;

	~CRTTextureBitmap();
private:
	int width, height, channels;
	unsigned char* buffer = nullptr;
};

