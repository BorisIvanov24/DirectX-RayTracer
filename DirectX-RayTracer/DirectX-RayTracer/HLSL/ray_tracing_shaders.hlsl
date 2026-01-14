RaytracingAccelerationStructure sceneBVHAccStruct : register(t0);
RWTexture2D<float4> frameTexture : register(u0);

cbuffer CameraCB : register(b0)
{
    float3 cameraPosition;
    float _pad0;
    row_major float4x4 cameraRotation;
};

struct RayPayload
{
    float4 pixelColor;
};

[shader("raygeneration")]
void rayGen()
{
    float width = 1920;
    float height = 1080;

    RayDesc cameraRay;
    cameraRay.Origin = float3(0.f, 14.f, 26.f);
    
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
    
    float3 rayDirCamera = normalize(float3(x, y, -1.0f));
    float3 rayDirWorld = normalize(mul(cameraRotation, rayDirCamera));
    
    cameraRay.Origin = cameraPosition;
    cameraRay.Direction = rayDirWorld;
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

//[shader("closesthit")]
//void closestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
//{
//    uint tri = PrimitiveIndex();

//    float r = frac(sin(tri * 12.9898) * 43758.5453);
//    float g = frac(sin(tri * 78.233) * 43758.5453);
//    float b = frac(sin(tri * 45.164) * 43758.5453);

//    payload.pixelColor = float4(r, g, b, 1.0);
//}

[shader("closesthit")]
void closestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    uint tri = PrimitiveIndex();

    // Stable per-triangle random
    float triRand = frac(sin(tri * 12.9898) * 43758.5453);

    float3 worldPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

    // Smooth spatial factor
    float spatial = sin(worldPos.x * 0.15) * 0.5 + 0.5;

    // Blend both
    float red = lerp(0.4 + 0.6 * triRand, 0.3 + 0.7 * spatial, 0.5);

    payload.pixelColor = float4(red, 0.0, 0.0, 1.0);
}

