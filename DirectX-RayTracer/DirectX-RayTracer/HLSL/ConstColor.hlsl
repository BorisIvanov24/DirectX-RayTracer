struct PSInput
{
    float4 position : SV_POSITION;
};

cbuffer RootConstants : register(b0)
{
    int frameIdx;
};

float4 PSMain(PSInput input) : SV_TARGET
{
    float2 p = input.position.xy;

    float2 screenSize = float2(800.0, 800.0);
    float2 uv = p / screenSize;

    float t = frameIdx * 0.01;

    float r = uv.x;
    float g = uv.y;
    float b = 0.5 + 0.5 * sin(uv.x * 10 + uv.y * 10 + t);

    return float4(r, g, b, 1.0);
}
