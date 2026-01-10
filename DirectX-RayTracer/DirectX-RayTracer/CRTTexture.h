#pragma once
#include "CRTVector.h";

class CRTTexture
{
public:
	CRTTexture(const std::string& name);
	virtual CRTVector getColor(float u = 0.f, float v = 0.f) const = 0;

	const std::string& getName() const;

	virtual ~CRTTexture() = default;

	virtual const char* getType() const;

private:
	std::string name;
};

