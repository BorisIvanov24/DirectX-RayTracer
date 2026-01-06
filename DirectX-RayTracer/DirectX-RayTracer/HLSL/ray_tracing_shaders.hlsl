RaytracingAccelerationStructure sceneBVHAccStruct : register(t0);
RWTexture2D<float4> frameTexture : register(u0);

struct RayPayload
{
    float4 pixelColor;
};

[shader("raygeneration")]
void rayGen()
{
    float width = 800;
    float height = 800;

    RayDesc cameraRay;
    cameraRay.Origin = float3(0.f, 0.f, 0.f);
    
    uint2 pixelRasterCoords = DispatchRaysIndex().xy;
    
    float x = pixelRasterCoords.x;
    float y = pixelRasterCoords.y;
    
    x += 0.5f;
    y += 0.5f;
    
    x /= width;
    y /= height;
    
    x = (2.f * x) - 1.f;
    y = 1.f - (2.f * y);
    
    x *= width / height;
    
    float3 rayDirection = float3(x, y, -1.f);
    float3 rayDirectionNormalize = normalize(rayDirection);
    cameraRay.Direction = rayDirectionNormalize;
    cameraRay.TMin = 0.001;
    cameraRay.TMax = 10000.0;
    
    RayPayload rayPayload;
    rayPayload.pixelColor = float4(0.f, 0.f, 0.f, 1.f);

    TraceRay(
        sceneBVHAccStruct,
        RAY_FLAG_NONE,
        0xFF,
        0, // hit group offset
        1, // hit group stride
        0, // miss shader index
        cameraRay,
        rayPayload
    );

    frameTexture[pixelRasterCoords] = rayPayload.pixelColor;
}

[shader("miss")]
void miss(inout RayPayload payload)
{
    payload.pixelColor = float4(0.0, 1.0, 1.0, 1.0);
}

[shader("closesthit")]
void closestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    payload.pixelColor = float4(1.0, 0.0, 0.0, 1.0);
}
