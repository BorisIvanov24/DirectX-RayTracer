#pragma once
#include <vector>
#include "CRTVector.h"


class CRTMesh
{
public:

	void addVertex(const CRTVector& vertex);
	void addIndex(int index);
	void setMaterialIndex(int index);
	void addUV(const CRTVector& uv);

	void print() const;
	const std::vector<CRTVector>& getVertices() const;
	const std::vector<int>& getIndices() const;
	const std::vector<CRTVector>& getVertexNormals() const;
	const std::vector<CRTVector>& getUV() const;
	int getMaterialIndex() const;


	void calculateVertexNormals();

private:
	std::vector<CRTVector> vertices;
	std::vector<int> indices;
	std::vector<CRTVector> vertexNormals;
	std::vector<CRTVector> uvData;
	int materialIndex;
};

