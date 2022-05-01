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

/*
StructuredBuffer<InputData> gInput : register(t0);
RWStructuredBuffer<OutputData> gOutput : register(u0);
*/
StructuredBuffer<InputData> gInput : register(t0);
//ConsumeStructuredBuffer<InputData> gInput : register(u0);
AppendStructuredBuffer<OutputData> gOutput : register(u0);

[numthreads(64, 1, 1)]
void CS(int3 dtid : SV_DispatchThreadID)
{
    int idx = dtid.x;

    //float3 pos = gInput.Consume().pos;
    float3 pos = gInput[idx].pos;
    OutputData output;
    output.pos = pos;
    output.len = pos.x * pos.x + pos.y * pos.y + pos.z * pos.z;
    output.inRange = output.len <= 10;
    gOutput.Append(output);

    /*
    float3 pos = gInput[idx].pos;
    gOutput[idx].pos = pos;
    gOutput[idx].len = pos.x * pos.x + pos.y * pos.y + pos.z * pos.z;
    gOutput[idx].inRange = (gOutput[idx].len <= 10);
    */
}