#include "CRTMesh.h"
#include <iostream>
#include "CRTTriangle.h"

void CRTMesh::addVertex(const CRTVector& vertex)
{
	vertices.push_back(vertex);
}

void CRTMesh::addIndex(int index)
{
	indices.push_back(index);
}

void CRTMesh::setMaterialIndex(int index)
{
	materialIndex = index;
}

void CRTMesh::addUV(const CRTVector& uv)
{
	uvData.push_back(uv);
}

void CRTMesh::print() const
{
	for (const auto& obj : vertices)
	{
		obj.print(std::cout);
	}

	for (int i = 0; i < indices.size(); i++)
	{
		if (i % 3 == 0)
			std::cout << std::endl;

		std::cout << indices[i] << ' ';
	}
}

const std::vector<CRTVector>& CRTMesh::getVertices() const
{
	return vertices;
}

const std::vector<int>& CRTMesh::getIndices() const
{
	return indices;
}

const std::vector<CRTVector>& CRTMesh::getVertexNormals() const
{
	return vertexNormals;
}

const std::vector<CRTVector>& CRTMesh::getUV() const
{
	return uvData;
}

int CRTMesh::getMaterialIndex() const
{
	return materialIndex;
}

void CRTMesh::calculateVertexNormals()
{
	size_t vertexCount = vertices.size();

	vertexNormals.resize(vertexCount, CRTVector(0.f, 0.f, 0.f));

	size_t indicesCount = indices.size();

	for (int i = 0; i + 2 < indicesCount; i += 3)
	{
		CRTVector v0 = vertices[indices[i]];
		CRTVector v1 = vertices[indices[i + 1]];
		CRTVector v2 = vertices[indices[i + 2]];

		CRTTriangle triangle(v0, v1, v2);

		CRTVector triangleNormal = triangle.getNormal();

		vertexNormals[indices[i]] = vertexNormals[indices[i]] + triangleNormal;
		vertexNormals[indices[i + 1]] = vertexNormals[indices[i + 1]] + triangleNormal;
		vertexNormals[indices[i + 2]] = vertexNormals[indices[i + 2]] + triangleNormal;
	}

	for (CRTVector& normal : vertexNormals)
	{
		normal.normalise();
	}

}
