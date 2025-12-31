RWTexture2D<float4> frameTexture : register(u0);

[shader("raygeneration")]
void rayGen()
{
    uint2 pixelRasterCoords = DispatchRaysIndex().xy;
    frameTexture[pixelRasterCoords] = float4(0.0, 0.0, 1.0, 1.0);
}

struct RayPayload
{
    float4 color;
};

[shader("miss")]
void miss(inout RayPayload payload)
{
    payload.color = float4(0.0, 1.0, 0.0, 1.0);
}