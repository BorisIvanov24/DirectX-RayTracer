#include "CRTTextureBitmap.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"
#include <iostream>

CRTTextureBitmap::CRTTextureBitmap(const std::string& filepath, const std::string& name)
    : CRTTexture(name)
{

    buffer = stbi_load(filepath.c_str(), &width, &height, &channels, 0);
}

CRTVector CRTTextureBitmap::getColor(float u, float v) const
{
    // Clamp UVs to [0,1] to avoid out-of-bounds access
    u = std::fmin(std::fmax(u, 0.0f), 1.0f);
    v = std::fmin(std::fmax(v, 0.0f), 1.0f);

    // Flip v if image origin is bottom-left (common in UVs)
    int rowIdx = static_cast<int>((1.0f - v) * (height - 1));
    int colIdx = static_cast<int>(u * (width - 1));

    // Compute pixel index in 1D buffer
    int index = (rowIdx * width + colIdx) * channels;


    // Extract RGB and normalize to [0,1]
    float r = buffer[index] / 255.0f;

    float g = buffer[index + 1] / 255.0f;

    float b = (channels > 2) ? buffer[index + 2] / 255.0f : 0.0f;


    return CRTVector(r, g, b);
}

const char* CRTTextureBitmap::getType() const
{
    return "bitmap";
}

CRTTextureBitmap::~CRTTextureBitmap()
{
    stbi_image_free(buffer);
}
