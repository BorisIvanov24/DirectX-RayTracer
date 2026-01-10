#pragma once
#include "CRTVector.h"

enum class CRTMaterialType
{
	INVALID,
	DIFFUSE,
	REFLECTIVE,
	REFRACTIVE,
	CONSTANT
};

class CRTMaterial
{
public:
	CRTMaterialType getType() const;
	const CRTVector& getAlbedo() const;
	bool isSmoothShading() const;
	float getIor() const;
	bool isTexture() const;
	const std::string& getTextureName() const;

	void setTextureName(const std::string& textureName);
	void setType(CRTMaterialType type);
	void setAlbedo(const CRTVector& albedo);
	void setSmoothShading(bool smoothShading);
	void setIor(float ior);


private:
	CRTMaterialType type;
	CRTVector albedo;
	std::string textureName;
	bool smoothShading;
	float ior;
};

