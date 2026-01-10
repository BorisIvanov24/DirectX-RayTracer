#include "CRTSceneParser.h"
#include "CRTTexture.h"
#include "CRTTextureAlbedo.h"
#include "CRTTextureBitmap.h"
#include "CRTTextureChecker.h"
#include "CRTTextureEdges.h"

#include <iostream>
#include <fstream>
using namespace rapidjson;

#pragma warning(disable : 4996)

CRTMatrix CRTSceneParser::loadMatrix(const rapidjson::Value::ConstArray& arr)
{
	assert(arr.Size() == 9);

	CRTMatrix matrix(
		static_cast<float>(arr[0].GetDouble()), static_cast<float>(arr[1].GetDouble()), static_cast<float>(arr[2].GetDouble()),
		static_cast<float>(arr[3].GetDouble()), static_cast<float>(arr[4].GetDouble()), static_cast<float>(arr[5].GetDouble()),
		static_cast<float>(arr[6].GetDouble()), static_cast<float>(arr[7].GetDouble()), static_cast<float>(arr[8].GetDouble())
	);

	return matrix;
}

CRTVector CRTSceneParser::loadVector(const Value::ConstArray& arr, int startIndex)
{
	assert(arr.Size() % 3 == 0);
	assert(startIndex < arr.Size() - 2);

	CRTVector vec(
		static_cast<float>(arr[startIndex].GetDouble()),
		static_cast<float>(arr[startIndex + 1].GetDouble()),
		static_cast<float>(arr[startIndex + 2].GetDouble())
	);

	return vec;
}

void CRTSceneParser::parseSettings(const Document& doc, CRTScene& scene)
{
	const Value& settingsVal = doc.FindMember("settings")->value;
	if (!settingsVal.IsNull() && settingsVal.IsObject())
	{
		const Value& backgroundColorVal = settingsVal.FindMember("background_color")->value;
		assert(!backgroundColorVal.IsNull() && backgroundColorVal.IsArray());
		scene.settings.backgroundColor = loadVector(backgroundColorVal.GetArray(), 0);

		const Value& imgSettings = settingsVal.FindMember("image_settings")->value;
		assert(!imgSettings.IsNull());

		const Value& width = imgSettings.FindMember("width")->value;
		assert(!width.IsNull());
		scene.settings.imageWidth = width.GetInt();

		const Value& height = imgSettings.FindMember("height")->value;
		assert(!height.IsNull());
		scene.settings.imageHeight = height.GetInt();
	}

	//std::cout << scene.settings.imageWidth << ' ' << scene.settings.imageHeight << std::endl;
	//scene.settings.backgroundColor.print(std::cout);
}

void CRTSceneParser::parseCamera(const rapidjson::Document& doc, CRTScene& scene)
{
	const Value& cameraVal = doc.FindMember("camera")->value;
	if (!cameraVal.IsNull() && cameraVal.IsObject())
	{
		const Value& matrixVal = cameraVal.FindMember("matrix")->value;
		assert(!matrixVal.IsNull() && matrixVal.IsArray());
		scene.camera.setRotationMatrix(loadMatrix(matrixVal.GetArray()));

		const Value& positionVal = cameraVal.FindMember("position")->value;
		assert(!positionVal.IsNull() && positionVal.IsArray());
		scene.camera.setPosition(loadVector(positionVal.GetArray(), 0));
	}
	//scene.camera.getPosition().print(std::cout);
	//scene.camera.getRotationMatrix().print();
}

void CRTSceneParser::parseMesh(const rapidjson::Value& val, CRTScene& scene)
{
	CRTMesh mesh;

	const Value& uvsVal = val.FindMember("uvs")->value;
	if (!uvsVal.IsNull() && uvsVal.IsArray())
	{
		int uvsCount = uvsVal.GetArray().Size();

		for (int i = 0; i < uvsCount / 3; i++)
		{
			CRTVector uv = loadVector(uvsVal.GetArray(), i * 3);
			mesh.addUV(uv);
		}
	}

	const Value& verticesVal = val.FindMember("vertices")->value;
	if (!verticesVal.IsNull() && verticesVal.IsArray())
	{
		int verticesCount = verticesVal.GetArray().Size();

		for (int i = 0; i < verticesCount / 3; i++)
		{
			CRTVector vertex = loadVector(verticesVal.GetArray(), i * 3);
			mesh.addVertex(vertex);
		}
	}

	const Value& indicesVal = val.FindMember("triangles")->value;
	if (!indicesVal.IsNull() && indicesVal.IsArray())
	{
		int indicesCount = indicesVal.GetArray().Size();

		for (int i = 0; i < indicesCount; i++)
		{
			int index = indicesVal.GetArray()[i].GetInt();
			mesh.addIndex(index);
		}
	}

	int materialIndex;
	const Value& materialVal = val.FindMember("material_index")->value;
	if (!materialVal.IsNull())
	{
		materialIndex = materialVal.GetInt();
	}

	mesh.setMaterialIndex(materialIndex);
	mesh.calculateVertexNormals();

	scene.geometryObjects.push_back(mesh); //possible std::move
}

void CRTSceneParser::parseObjects(const rapidjson::Document& doc, CRTScene& scene)
{
	const Value& objectsVal = doc.FindMember("objects")->value;
	if (!objectsVal.IsNull() && objectsVal.IsArray())
	{
		int objectsCount = objectsVal.GetArray().Size();

		for (int i = 0; i < objectsCount; i++)
		{
			const Value& mesh = objectsVal.GetArray()[i];
			parseMesh(mesh, scene);
		}
	}
}


void CRTSceneParser::parseLights(const rapidjson::Document& doc, CRTScene& scene)
{
	const Value& lightsVal = doc.FindMember("lights")->value;

	if (!lightsVal.IsNull() && lightsVal.IsArray())
	{
		int lightsCount = lightsVal.GetArray().Size();

		for (int i = 0; i < lightsCount; i++)
		{
			const Value& light = lightsVal.GetArray()[i];
			parseLight(light, scene);
		}
	}
}

void CRTSceneParser::parseLight(const rapidjson::Value& val, CRTScene& scene)
{
	CRTVector position;
	float intensity = 0.f;

	const Value& positionVal = val.FindMember("position")->value;
	if (!positionVal.IsNull() && positionVal.IsArray())
	{
		position = loadVector(positionVal.GetArray(), 0);
	}

	const Value& intensityVal = val.FindMember("intensity")->value;
	if (!intensityVal.IsNull())
	{
		intensity = intensityVal.GetFloat();
	}

	CRTLight light(position, intensity);

	/*std::cout << "light:\n";
	position.print(std::cout);
	std::cout << intensity << std::endl;*/

	scene.lights.push_back(light);
}

void CRTSceneParser::parseTextures(const rapidjson::Document& doc, CRTScene& scene)
{
	const Value& texturesVal = doc.FindMember("textures")->value;

	if (!texturesVal.IsNull() && texturesVal.IsArray())
	{
		int texturesCount = texturesVal.GetArray().Size();

		for (int i = 0; i < texturesCount; i++)
		{
			const Value& texture = texturesVal.GetArray()[i];
			parseTexture(texture, scene);
		}
	}
}

void CRTSceneParser::parseTexture(const rapidjson::Value& val, CRTScene& scene)
{
	CRTTexture* textureToAdd = nullptr;
	std::string name;

	const Value& nameVal = val.FindMember("name")->value;
	if (!nameVal.IsNull())
	{
		name = nameVal.GetString();
	}
	std::cout << "name: " << name << std::endl;

	std::string type;

	const Value& typeVal = val.FindMember("type")->value;
	if (!typeVal.IsNull())
	{
		type = typeVal.GetString();
	}

	if (type == "albedo")
	{
		CRTVector albedo;

		const Value& albedoVal = val.FindMember("albedo")->value;
		if (!albedoVal.IsNull())
		{
			albedo = loadVector(albedoVal.GetArray(), 0);
		}

		textureToAdd = new CRTTextureAlbedo(albedo, name);
	}
	else if (type == "edges")
	{
		CRTVector edgeColor, innerColor;
		float edgeWidth;

		const Value& edgeColorVal = val.FindMember("edge_color")->value;
		if (!edgeColorVal.IsNull())
		{
			edgeColor = loadVector(edgeColorVal.GetArray(), 0);
		}

		const Value& innerColorVal = val.FindMember("inner_color")->value;
		if (!innerColorVal.IsNull())
		{
			innerColor = loadVector(innerColorVal.GetArray(), 0);
		}

		const Value& edgeWidthVal = val.FindMember("edge_width")->value;
		if (!edgeWidthVal.IsNull())
		{
			edgeWidth = edgeWidthVal.GetFloat();
		}

		textureToAdd = new CRTTextureEdges(edgeColor, innerColor, edgeWidth, name);
	}
	else if (type == "checker")
	{
		CRTVector colorA, colorB;
		float squareSize;

		const Value& colorAVal = val.FindMember("color_A")->value;
		if (!colorAVal.IsNull())
		{
			colorA = loadVector(colorAVal.GetArray(), 0);
		}

		const Value& colorBVal = val.FindMember("color_B")->value;
		if (!colorBVal.IsNull())
		{
			colorB = loadVector(colorBVal.GetArray(), 0);
		}

		const Value& squareSizeVal = val.FindMember("square_size")->value;
		if (!squareSizeVal.IsNull())
		{
			squareSize = squareSizeVal.GetFloat();
		}

		textureToAdd = new CRTTextureChecker(colorA, colorB, squareSize, name);
	}
	else
	{
		std::string filePath;

		const Value& filePathVal = val.FindMember("file_path")->value;
		if (!filePathVal.IsNull())
		{
			filePath = filePathVal.GetString();
		}

		textureToAdd = new CRTTextureBitmap(filePath, name);
	}

	scene.textures.push_back(textureToAdd);
}

void CRTSceneParser::parseMaterials(const rapidjson::Document& doc, CRTScene& scene)
{
	const Value& materialsVal = doc.FindMember("materials")->value;

	if (!materialsVal.IsNull() && materialsVal.IsArray())
	{
		int materialsCount = materialsVal.GetArray().Size();
		//std::cout << "materialsCount:\n" << materialsCount << std::endl;

		for (int i = 0; i < materialsCount; i++)
		{
			const Value& material = materialsVal.GetArray()[i];
			parseMaterial(material, scene);
		}
	}
}

static CRTMaterialType getMaterialTypeFromString(const std::string& str)
{
	if (str == "diffuse")
	{
		return CRTMaterialType::DIFFUSE;
	}

	if (str == "reflective")
	{
		return CRTMaterialType::REFLECTIVE;
	}

	if (str == "constant")
	{
		return CRTMaterialType::CONSTANT;
	}

	return CRTMaterialType::REFRACTIVE;
}

void CRTSceneParser::parseMaterial(const rapidjson::Value& val, CRTScene& scene)
{
	CRTMaterialType type;
	CRTVector albedo;
	bool smoothShading;
	float ior;
	CRTMaterial material;

	const Value& typeVal = val.FindMember("type")->value;
	if (!typeVal.IsNull())
	{
		type = getMaterialTypeFromString(typeVal.GetString());
		material.setType(type);
	}

	if (type == CRTMaterialType::REFRACTIVE)
	{
		const Value& iorVal = val.FindMember("ior")->value;
		if (!iorVal.IsNull())
		{
			ior = iorVal.GetFloat();
		}
		material.setIor(ior);
		albedo = CRTVector(1.f, 1.f, 1.f);
		material.setAlbedo(albedo);
	}
	else
	{
		const Value& albedoVal = val.FindMember("albedo")->value;
		if (!albedoVal.IsNull() && albedoVal.IsArray())
		{
			albedo = loadVector(albedoVal.GetArray(), 0);
			material.setAlbedo(albedo);

		}
		else
		{
			material.setTextureName(albedoVal.GetString());
		}
	}

	const Value& smoothShadingVal = val.FindMember("smooth_shading")->value;
	if (!smoothShadingVal.IsNull())
	{
		smoothShading = smoothShadingVal.GetBool();
	}
	material.setSmoothShading(smoothShading);

	scene.materials.push_back(material);

	std::cout << "textureName:\n";
	std::cout << material.getTextureName() << std::endl;
	std::cout << "albedo:\n ";
	albedo.print(std::cout);
	std::cout << "smooth_shading:\n";
	std::cout << smoothShading << std::endl;
	std::cout << "type\n";
	std::cout << (int)type << std::endl;
	std::cout << "ior:\n";
	//std::cout << ior << std::endl;
}

void CRTSceneParser::parseScene(const std::string& sceneFileName, CRTScene& scene)
{
	std::ifstream ifs(sceneFileName);
	assert(ifs.is_open());

	rapidjson::IStreamWrapper isw(ifs);
	rapidjson::Document doc;
	doc.ParseStream(isw);

	parseSettings(doc, scene);
	parseCamera(doc, scene);
	parseObjects(doc, scene);
	parseLights(doc, scene);
	parseMaterials(doc, scene);
	parseTextures(doc, scene);

	/*for (auto& obj : scene.geometryObjects)
	{
		obj.print();
	}*/
}
