#include "CRTTextureEdges.h"

CRTTextureEdges::CRTTextureEdges(const CRTVector& edgeColor, const CRTVector& innerColor,
    float edgeWidth, const std::string& name)
    : CRTTexture(name), edgeColor(edgeColor), innerColor(innerColor), edgeWidth(edgeWidth)
{
}

CRTVector CRTTextureEdges::getColor(float u, float v) const
{
    if (u < edgeWidth || v < edgeWidth || (1 - u - v) < edgeWidth)
        return edgeColor;

    return innerColor;
}
