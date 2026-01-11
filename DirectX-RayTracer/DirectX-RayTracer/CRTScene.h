#pragma once
#include <string>
#include "CRTMesh.h"
#include "CRTCamera.h"
#include "CRTLight.h"
#include "CRTMaterial.h"
#include "CRTTexture.h"

struct CRTSettings
{
	CRTVector backgroundColor;
	int imageWidth;
	int imageHeight;
};

class CRTScene
{
public:
	friend class CRTSceneParser;

	CRTScene(const std::string& sceneFileName);

	void parseSceneFile(const std::string& sceneFileName);
	const CRTSettings& getSettings() const;
	const CRTCamera& getCamera() const;
	CRTCamera& getCamera();
	const std::vector<CRTMesh>& getObjects() const;
	const std::vector<CRTLight>& getLights() const;
	const std::vector<CRTMaterial>& getMaterials() const;
	const std::vector<CRTTexture*>& getTextures() const;

	const CRTTexture* getTextureByName(const std::string& name) const;

private:
	std::vector<CRTMesh> geometryObjects;
	CRTCamera camera;
	CRTSettings settings;
	std::vector<CRTLight> lights;
	std::vector<CRTMaterial> materials;
	std::vector<CRTTexture*> textures;

};

