#include "CRTScene.h"
#include <fstream>
#include <iostream>
#include <assert.h>
#include "CRTSceneParser.h"

CRTScene::CRTScene(const std::string& sceneFileName)
{
	parseSceneFile(sceneFileName);
}

void CRTScene::parseSceneFile(const std::string& sceneFileName)
{
	CRTSceneParser::parseScene(sceneFileName, *this);
}

const CRTSettings& CRTScene::getSettings() const
{
	return settings;
}

const CRTCamera& CRTScene::getCamera() const
{
	return camera;
}

const std::vector<CRTMesh>& CRTScene::getObjects() const
{
	return geometryObjects;
}

const std::vector<CRTLight>& CRTScene::getLights() const
{
	return lights;
}

const std::vector<CRTMaterial>& CRTScene::getMaterials() const
{
	return materials;
}

const std::vector<CRTTexture*>& CRTScene::getTextures() const
{
	return textures;
}

const CRTTexture* CRTScene::getTextureByName(const std::string& name) const
{
	for (int i = 0; i < textures.size(); i++)
	{
		if (textures[i]->getName() == name)
		{
			return textures[i];
		}
	}

	return nullptr;
}


