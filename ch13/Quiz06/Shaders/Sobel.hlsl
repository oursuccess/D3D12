//sobel

Texture2D gInput : register(t0);
Texture2D<float4> gOutput : register(u0);

float CalcLuminance(float3 color)
{
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

[numthreads(16, 16, 1)]
void SobelCS(int3 dispatchThreadID : SV_DispatchThreadID)
{
    float4 c[3][3];
    //采样该点周围3x3的格子
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            int2 xy = dispatchThreadID.xy + int2(j - 1, i - 1);
            c[i][j] = gInput[xy];
        }
    }

    //计算x和y的平均
    float4 Gx = -1.0f * c[0][0] - 2.0f * c[1][0] - 1.0f * c[2][0] + 1.0f * c[0][2] + 2.0f * c[1][2] + 1.0f * c[2][2];
    float4 Gy = -1.0f * c[2][0] - 2.0f * c[2][1] - 1.0f * c[2][2] + 1.0f * c[0][0] + 2.0f * c[0][1] + 1.0f * c[0][2];

    //计算标准差
    float4 mag = sqrt(Gx * Gx + Gy * Gy);

    //让边缘为黑色，而其余为白色
    mag = 1.0f - saturate(CalcLuminance(mag.rgb));
    
    gOutput[dispatchThreadID.xy] = mag;
}