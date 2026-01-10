#include "CRTMaterial.h"

CRTMaterialType CRTMaterial::getType() const
{
    return type;
}

const CRTVector& CRTMaterial::getAlbedo() const
{
    return albedo;
}

bool CRTMaterial::isSmoothShading() const
{
    return smoothShading;
}

float CRTMaterial::getIor() const
{
    return ior;
}

bool CRTMaterial::isTexture() const
{
    return !textureName.empty();
}

const std::string& CRTMaterial::getTextureName() const
{
    return textureName;
}

void CRTMaterial::setTextureName(const std::string& textureName)
{
    this->textureName = textureName;
}

void CRTMaterial::setType(CRTMaterialType type)
{
    this->type = type;
}

void CRTMaterial::setAlbedo(const CRTVector& albedo)
{
    this->albedo = albedo;
}

void CRTMaterial::setSmoothShading(bool smoothShading)
{
    this->smoothShading = smoothShading;
}

void CRTMaterial::setIor(float ior)
{
    this->ior = ior;
}
