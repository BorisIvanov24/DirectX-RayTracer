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

cbuffer DebugCB : register(b1)
{
    uint shadingMode;
}

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

[shader("closesthit")]
void closestHit(inout RayPayload payload,
                in BuiltInTriangleIntersectionAttributes attr)
{
    float3 color = float3(0.f, 1.f, 0.f);

    if (shadingMode == 0)
    {
        uint tri = PrimitiveIndex();
        float r = frac(sin(tri * 12.9898) * 43758.5453);
        float g = frac(sin(tri * 78.233) * 43758.5453);
        float b = frac(sin(tri * 45.164) * 43758.5453);
        color = float3(r, g, b);

    }
    else if (shadingMode == 1)
    {
        float3 worldPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

        uint objID = InstanceID();
        float objR = frac(sin(objID * 12.9898) * 43758.5453);
        float objG = frac(sin(objID * 78.233) * 12345.6789);
        float objB = frac(sin(objID * 39.425) * 34567.8901);
        float3 objectBaseColor = float3(objR, objG, objB);
 
        float cellSize = 2.0;

        int3 cell = int3(floor(worldPos / cellSize));

        uint hash = (uint) (cell.x * 73856093) ^ (uint) (cell.y * 19349663) ^ (uint) (cell.z * 83492791);
        float variation = frac(sin(hash * 12.9898) * 43758.5453);

        float3 finalColor = lerp(objectBaseColor * 0.7, objectBaseColor * 1.3, variation);
        color = finalColor;
    }
    else if (shadingMode == 2)
    {
        uint objID = InstanceID();
        uint triID = PrimitiveIndex();
        float objR = frac(sin(objID * 12.9898) * 43758.5453);
        float objG = frac(sin(objID * 78.233) * 12345.6789);
        float objB = frac(sin(objID * 39.425) * 34567.8901);
        float3 baseColor = float3(objR, objG, objB);
        float shade = frac(sin(triID * 12.9898) * 43758.5453);
        float3 finalColor = baseColor * lerp(0.6, 1.0, shade);
        color = finalColor;
    }
    else if (shadingMode == 3)
    {
        float3 bary = float3(
        1.0 - attr.barycentrics.x - attr.barycentrics.y,
        attr.barycentrics.x,
        attr.barycentrics.y
        );

        color = bary;
    }
    else if (shadingMode == 4)
    {
        float3 worldPos =
        WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

        float h = saturate((worldPos.y + 10.0) / 20.0);

        color = lerp(
        float3(0.1, 0.2, 0.6),
        float3(0.9, 0.9, 0.9),
        h
        );
    }
    else if (shadingMode == 5)
    {
        float dist = RayTCurrent() * 0.05;
        float c = saturate(dist);

        color = float3(c, c, c);
    }
    else
    {
        float3 p =
        WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

        int checker =
        (int(floor(p.x)) ^
         int(floor(p.z))) & 1;

        float c = checker ? 0.9 : 0.2;
        color = float3(c, c, c);
    }

    payload.pixelColor = float4(color, 1.0);
}


