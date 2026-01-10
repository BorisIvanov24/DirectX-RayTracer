#pragma once
#include "CRTScene.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/document.h"

class CRTSceneParser
{
private:
	static CRTMatrix loadMatrix(const rapidjson::Value::ConstArray& arr);
	static CRTVector loadVector(const rapidjson::Value::ConstArray& arr, int startIndex);
	static void parseSettings(const rapidjson::Document& doc, CRTScene& scene);
	static void parseCamera(const rapidjson::Document& doc, CRTScene& scene);
	static void parseMesh(const rapidjson::Value& val, CRTScene& scene);
	static void parseObjects(const rapidjson::Document& doc, CRTScene& scene);
	static void parseLights(const rapidjson::Document& doc, CRTScene& scene);
	static void parseLight(const rapidjson::Value& val, CRTScene& scene);

	static void parseTextures(const rapidjson::Document& doc, CRTScene& scene);
	static void parseTexture(const rapidjson::Value& val, CRTScene& scene);

	static void parseMaterials(const rapidjson::Document& doc, CRTScene& scene);
	static void parseMaterial(const rapidjson::Value& val, CRTScene& scene);

public:
	static void parseScene(const std::string& sceneFileName, CRTScene& scene);
};

