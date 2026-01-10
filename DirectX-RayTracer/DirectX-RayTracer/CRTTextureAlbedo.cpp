#include "CRTTextureAlbedo.h"

CRTTextureAlbedo::CRTTextureAlbedo(const CRTVector& albedo, const std::string& name)
    : CRTTexture(name), albedo(albedo)
{
}

CRTVector CRTTextureAlbedo::getColor(float u, float v) const
{
    return albedo;
}
