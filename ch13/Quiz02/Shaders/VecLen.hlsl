//copy of VecAdd.hlsl by Frank Luna, ch13

struct InputData
{
    float3 pos;
};

struct OutputData
{
    float len;
};

StructuredBuffer<InputData> gInput : register(t0);
RWStructuredBuffer<OutputData> gOutput : register(u0);

[numthreads(64, 1, 1)]
void CS(int3 dtid : SV_DispatchThreadID)
{
    float3 pos = gInput[dtid.x].pos;
    gOutput[dtid.x].len = pos.x * pos.x + pos.y * pos.y + pos.z * pos.z;
}