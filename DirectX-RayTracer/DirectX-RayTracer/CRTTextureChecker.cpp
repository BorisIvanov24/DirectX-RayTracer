#include "CRTTextureChecker.h"

CRTTextureChecker::CRTTextureChecker(const CRTVector& colorA, const CRTVector& colorB,
    float squareSize, const std::string& name)
    : CRTTexture(name), colorA(colorA), colorB(colorB), squareSize(squareSize)
{
}

CRTVector CRTTextureChecker::getColor(float u, float v) const
{
    int width = static_cast<int>(1.f / squareSize);

    int u2 = static_cast<int>(std::floor(u * width));
    int v2 = static_cast<int>(std::floor(v * width));

    if ((u2 + v2) % 2 == 0)
        return colorA;

    return colorB;
}

const char* CRTTextureChecker::getType() const
{
    return "checker";
}
