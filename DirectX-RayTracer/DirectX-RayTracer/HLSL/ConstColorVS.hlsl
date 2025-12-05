struct VSInput
{
    float2 position : POSITION;
};

struct PSInput
{
    float4 position : SV_POSITION;
};

PSInput VSMain(VSInput inputVertex)
{
    PSInput output; // declare a variable
    output.position = float4(inputVertex.position, 0.0, 1.0); // assign member
    return output; // return the struct
}