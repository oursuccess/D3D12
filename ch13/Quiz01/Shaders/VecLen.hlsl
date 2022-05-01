//copy of VecAdd.hlsl by Frank Luna, ch13

struct InputData
{
    float3 pos;
};

struct OutputData
{
    float3 pos;
    float len;
    int inRange;
    //float3 pad;
};

StructuredBuffer<InputData> gInput : register(t0);
RWStructuredBuffer<OutputData> gOutput : register(u0);

[numthreads(128, 1, 1)]
void CS(int3 dtid : SV_DispatchThreadID)
{
    int idx = dtid.x;
    float3 pos = gInput[idx].pos;
    gOutput[idx].pos = pos;
    gOutput[idx].len = pos.x * pos.x + pos.y * pos.y + pos.z * pos.z;
    gOutput[idx].inRange = (gOutput[idx].len <= 10);
}