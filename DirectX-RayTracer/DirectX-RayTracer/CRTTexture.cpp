#include "CRTTexture.h"

CRTTexture::CRTTexture(const std::string& name) : name(name)
{
}

const std::string& CRTTexture::getName() const
{
	return name;
}

const char* CRTTexture::getType() const
{
	return "other";
}

