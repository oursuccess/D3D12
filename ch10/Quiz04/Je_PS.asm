//
// Generated by Microsoft (R) HLSL Shader Compiler 10.1
// Je注释版本
//
//
// Buffer Definitions: 
//
// cbuffer cbMaterial
// {
//
//   float4 gDiffuseAlbedo;             // Offset:    0 Size:    16
//   float3 gFresnelR0;                 // Offset:   16 Size:    12
//   float gRoughness;                  // Offset:   28 Size:     4
//   float4x4 gMatTransform;            // Offset:   32 Size:    64 [unused]
//
// }
//
// cbuffer cbPass
// {
//
//   float4x4 gView;                    // Offset:    0 Size:    64 [unused]
//   float4x4 gInvView;                 // Offset:   64 Size:    64 [unused]
//   float4x4 gProj;                    // Offset:  128 Size:    64 [unused]
//   float4x4 gInvProj;                 // Offset:  192 Size:    64 [unused]
//   float4x4 gViewProj;                // Offset:  256 Size:    64 [unused]
//   float4x4 gInvViewProj;             // Offset:  320 Size:    64 [unused]
//   float3 gEyePosW;                   // Offset:  384 Size:    12
//   float cbPerObjectPad1;             // Offset:  396 Size:     4 [unused]
//   float2 gRenderTargetSize;          // Offset:  400 Size:     8 [unused]
//   float2 gInvRenderTargetSize;       // Offset:  408 Size:     8 [unused]
//   float gNearZ;                      // Offset:  416 Size:     4 [unused]
//   float gFarZ;                       // Offset:  420 Size:     4 [unused]
//   float gTotalTime;                  // Offset:  424 Size:     4 [unused]
//   float gDeltaTime;                  // Offset:  428 Size:     4 [unused]
//   float4 gAmbientLight;              // Offset:  432 Size:    16
//   float4 gFogColor;                  // Offset:  448 Size:    16 [unused]	//由此可见，连雾都没了
//   float gFogStart;                   // Offset:  464 Size:     4 [unused]
//   float gFogRange;                   // Offset:  468 Size:     4 [unused]
//   float2 cbPerObjectPad2;            // Offset:  472 Size:     8 [unused]
//   
//   struct Light
//   {
//       
//       float3 Strength;               // Offset:  480
//       float FalloffStart;            // Offset:  492
//       float3 Direction;              // Offset:  496
//       float FalloffEnd;              // Offset:  508
//       float3 Position;               // Offset:  512
//       float SpotPower;               // Offset:  524
//
//   } gLights[16];                     // Offset:  480 Size:   768
//
// }
//
//
// Resource Bindings:
//
// Name                                 Type  Format         Dim      HLSL Bind  Count
// ------------------------------ ---------- ------- ----------- -------------- ------
// gsamAnisotropicWrap               sampler      NA          NA             s4      1 
// gDiffuseMap                       texture  float4          2d             t0      1 
// cbPass                            cbuffer      NA          NA            cb1      1 
// cbMaterial                        cbuffer      NA          NA            cb2      1 
//
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float       
// POSITION                 0   xyz         1     NONE   float   xyz 
// NORMAL                   0   xyz         2     NONE   float   xyz 
// TEXCOORD                 0   xy          3     NONE   float   xy  
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
//
ps_5_0
dcl_globalFlags refactoringAllowed | skipOptimization
dcl_constantbuffer CB2[2], immediateIndexed
dcl_constantbuffer CB1[41], immediateIndexed
dcl_sampler s4, mode_default
dcl_resource_texture2d (float,float,float,float) t0
dcl_input_ps linear v1.xyz
dcl_input_ps linear v2.xyz
dcl_input_ps linear v3.xy
dcl_output o0.xyzw
dcl_temps 13
dcl_indexableTemp x0[21], 4
//
// Initial variable locations:
//   v0.x <- pin.PosH.x; v0.y <- pin.PosH.y; v0.z <- pin.PosH.z; v0.w <- pin.PosH.w; 	//v0: pin.PosH
//   v1.x <- pin.PosW.x; v1.y <- pin.PosW.y; v1.z <- pin.PosW.z; 						//v1: pin.PosW
//   v2.x <- pin.NormalW.x; v2.y <- pin.NormalW.y; v2.z <- pin.NormalW.z; 				//v2: pin.NormalW 
//   v3.x <- pin.TexC.x; v3.y <- pin.TexC.y; 											//v3: pin.TexC
//   o0.x <- <PS return value>.x; o0.y <- <PS return value>.y; o0.z <- <PS return value>.z; o0.w <- <PS return value>.w		//o0: return[float4]
//
#line 112 "E:\GitRepos\D3D12\ch10\Quiz04\Shaders\Default.hlsl"
sample_indexable(texture2d)(float,float,float,float) r0.xyzw, v3.xyxx, t0.xyzw, s4		//sample, 根据v3(pin.TexC)来采样t0，并将结果存储于r0中; s4为我们使用的采样器(各向异性)
mul r0.xyzw, r0.xyzw, cb2[0].xyzw  // r0.x <- diffuseAlbedo.x; r0.y <- diffuseAlbedo.y; r0.z <- diffuseAlbedo.z; r0.w <- diffuseAlbedo.w // r0 *= gDiffuseAlbedo

#line 119	//pin.NormalW = normalize(pin.NormalW)
dp3 r1.x, v2.xyzx, v2.xyzx	//r1.x = dot(v2.xyzx, v2.xyzx)
rsq r1.x, r1.x	//r1.x = 1 / sqrt(r1.x)
mul r1.xyz, r1.xxxx, v2.xyzx  // r1.x <- pin.NormalW.x; r1.y <- pin.NormalW.y; r1.z <- pin.NormalW.z	//r1.xyz = (v2.x * r1.x, v2.y * r1.x, v2.z * r1.x); 而r1.x是r1长度的倒数

#line 122	//float3 toEyeW = gEyePosW - pin.PosW
mov r2.xyz, -v1.xyzx	//r2.xyz = -v1(pin.PosW).xyz
add r2.xyz, r2.xyzx, cb1[24].xyzx  // r2.x <- toEyeW.x; r2.y <- toEyeW.y; r2.z <- toEyeW.z 	//r2 = r2 + gToEyeW

#line 123	//float distToEye = length(toEyeW)
dp3 r1.w, r2.xyzx, r2.xyzx	//r1.w = dot(r2.xyzx, r2.xyzx)
sqrt r1.w, r1.w  // r1.w <- distToEye	//r1.w = sqrt(r1.w); 即我们求了distToEye

#line 124	//toEyeW /= distToEye
div r2.xyz, r2.xyzx, r1.wwww	//r2 /= r1.w	//toEyeW /= distToEye

#line 126	//float4 ambient = gAmbientLight * gDiffuseAlbedo;
mul r3.xyz, cb2[0].xyzx, cb1[27].xyzx  // r3.x <- ambient.x; r3.y <- ambient.y; r3.z <- ambient.z	//r3 = gAmbientLight * gDiffuseColor

#line 128	//const float shininess = 1.0f - gRoughness
mov r1.w, -cb2[1].w 	//r1.w = -gRoughness
add r4.w, r1.w, l(1.000000)  // r4.w <- shininess	//r4.w = r1.w + 1.0f

#line 130	//Material mat = {diffuseAlbedo, gFresnelR0, shininess};	//Material是8个float
mov r5.xyz, r0.xyzx  // r5.x <- mat.DiffuseAlbedo.x; r5.y <- mat.DiffuseAlbedo.y; r5.z <- mat.DiffuseAlbedo.z
mov r5.w, cb2[1].x  // r5.w <- mat.FresnelR0.x
mov r4.yz, cb2[1].yyzy  // r4.y <- mat.FresnelR0.y; r4.z <- mat.FresnelR0.z
mov r4.w, r4.w  // r4.w <- mat.Shineness

#line 133	//float4 directLight = ComputeLighting(gLights, mat, pin.PosW, pin.NormalW, toEyeW, shadowFactor);
nop 
mov x0[0].xyz, cb1[30].xyzx	//为什么这里是从30开始？ 是因为我们的cbPass里，在gLights之前有29个float4
mov x0[2].xyz, cb1[31].xyzx	//为什么是11个？ 是因为我们这里定义了3个方向光，而每个方向光需要3个。 第4个方向光只传入了实际需要的(direction和strength)
mov x0[4].xyz, cb1[32].xyzx	//为什么存入的地方每次增加2？？？？
mov x0[6].xyz, cb1[33].xyzx
mov x0[8].xyz, cb1[34].xyzx
mov x0[10].xyz, cb1[35].xyzx
mov x0[12].xyz, cb1[36].xyzx
mov x0[14].xyz, cb1[37].xyzx
mov x0[16].xyz, cb1[38].xyzx
mov x0[18].xyz, cb1[39].xyzx
mov x0[20].xyz, cb1[40].xyzx
mov r5.xyzw, r5.xyzw	//接下来依次传入mat
mov r4.yzw, r4.yyzw	//这是mat的第二部分
mov r1.xyz, r1.xyzx	//这是正交化后的pin.NormalW
mov r2.xyz, r2.xyzx //这是toEyeW; 而posW因为没有计算，shadowFactor则可以延迟到后面，因此并没有计算

#line 110 "E:\GitRepos\D3D12\ch10\Quiz04\Shaders\LightingUtil.hlsl"	//从这里开始进入LightingUtil
mov r0.xyz, l(0,0,0,0)  // r0.x <- result.x; r0.y <- result.y; r0.z <- result.z

#line 114
mov r1.w, l(0)  // r1.w <- i
mov r6.xyz, r0.xyzx  // r6.x <- result.x; r6.y <- result.y; r6.z <- result.z
mov r2.w, r1.w  // r2.w <- i
loop 
  ilt r3.w, r2.w, l(3)
  breakc_z r3.w

#line 116
  mov r3.w, l(1.000000)  // r3.w <- shadowFactor.x
  nop 
  imul null, r4.x, r2.w, l(6)
  mov r7.xyz, x0[r4.x + 0].xyzx
  mov r8.xyz, x0[r4.x + 2].xyzx
  mov r9.x, r5.w
  mov r9.yzw, r4.yyzw
  mov r10.xyz, r5.xyzx
  mov r11.xyz, r1.xyzx
  mov r12.xyz, r2.xyzx

#line 59
  mov r8.xyz, -r8.xyzx  // r8.x <- lightVec.x; r8.y <- lightVec.y; r8.z <- lightVec.z

#line 61
  dp3 r4.x, r8.xyzx, r11.xyzx
  max r4.x, r4.x, l(0.000000)  // r4.x <- ndotl

#line 62
  mul r7.xyz, r4.xxxx, r7.xyzx  // r7.x <- lightStrength.x; r7.y <- lightStrength.y; r7.z <- lightStrength.z

#line 64
  nop 
  mov r7.xyz, r7.xyzx
  mov r8.xyz, r8.xyzx
  mov r11.xyz, r11.xyzx
  mov r12.xyz, r12.xyzx
  mov r10.xyz, r10.xyzx
  mov r9.xyzw, r9.xyzw

#line 44
  mul r4.x, r9.w, l(256.000000)  // r4.x <- m

#line 45
  add r12.xyz, r8.xyzx, r12.xyzx
  dp3 r6.w, r12.xyzx, r12.xyzx
  rsq r6.w, r6.w
  mul r12.xyz, r6.wwww, r12.xyzx  // r12.x <- halfVec.x; r12.y <- halfVec.y; r12.z <- halfVec.z

#line 47
  add r6.w, r4.x, l(8.000000)
  dp3 r7.w, r12.xyzx, r11.xyzx
  max r7.w, r7.w, l(0.000000)
  log r7.w, r7.w
  mul r4.x, r4.x, r7.w
  exp r4.x, r4.x
  mul r4.x, r4.x, r6.w
  div r4.x, r4.x, l(8.000000)  // r4.x <- roughnessFactor

#line 48
  nop 
  mov r9.xyz, r9.xyzx
  mov r12.xyz, r12.xyzx
  mov r8.xyz, r8.xyzx

#line 33
  dp3 r6.w, r12.xyzx, r8.xyzx
  max r6.w, r6.w, l(0.000000)
  min r6.w, r6.w, l(1.000000)  // r6.w <- cosIncidentAngle

#line 34
  mov r6.w, -r6.w
  add r6.w, r6.w, l(1.000000)  // r6.w <- f0

#line 35
  mov r8.xyz, -r9.xyzx
  add r8.xyz, r8.xyzx, l(1.000000, 1.000000, 1.000000, 0.000000)
  mul r7.w, r6.w, r6.w
  mul r7.w, r6.w, r7.w
  mul r7.w, r6.w, r7.w
  mul r6.w, r6.w, r7.w
  mul r8.xyz, r6.wwww, r8.xyzx
  add r8.xyz, r8.xyzx, r9.xyzx  // r8.x <- reflectPercent.x; r8.y <- reflectPercent.y; r8.z <- reflectPercent.z

#line 37
  mov r8.xyz, r8.xyzx  // r8.x <- <SchlickFresnel return value>.x; r8.y <- <SchlickFresnel return value>.y; r8.z <- <SchlickFresnel return value>.z

#line 48
  mov r8.xyz, r8.xyzx  // r8.x <- fresnelFactor.x; r8.y <- fresnelFactor.y; r8.z <- fresnelFactor.z

#line 50
  mul r8.xyz, r4.xxxx, r8.xyzx  // r8.x <- specAlbedo.x; r8.y <- specAlbedo.y; r8.z <- specAlbedo.z

#line 52
  add r9.xyz, r8.xyzx, l(1.000000, 1.000000, 1.000000, 0.000000)
  div r8.xyz, r8.xyzx, r9.xyzx

#line 54
  add r8.xyz, r8.xyzx, r10.xyzx
  mul r7.xyz, r7.xyzx, r8.xyzx  // r7.x <- <BlinnPhong return value>.x; r7.y <- <BlinnPhong return value>.y; r7.z <- <BlinnPhong return value>.z

#line 64
  mov r7.xyz, r7.xyzx  // r7.x <- <ComputeDirectionalLight return value>.x; r7.y <- <ComputeDirectionalLight return value>.y; r7.z <- <ComputeDirectionalLight return value>.z

#line 116
  mul r7.xyz, r3.wwww, r7.xyzx
  add r6.xyz, r6.xyzx, r7.xyzx

#line 117
  iadd r2.w, r2.w, l(1)
endloop 

#line 134
mov r6.xyz, r6.xyzx  // r6.x <- <ComputeLighting return value>.x; r6.y <- <ComputeLighting return value>.y; r6.z <- <ComputeLighting return value>.z

#line 133 "E:\GitRepos\D3D12\ch10\Quiz04\Shaders\Default.hlsl" 	//再次回到Default,函数调用结束
mov r6.xyz, r6.xyzx  // r6.x <- directLight.x; r6.y <- directLight.y; r6.z <- directLight.z //计算出了最终的光照强度

#line 135	//float4 litColor = ambient + directLight;
add r0.xyz, r3.xyzx, r6.xyzx  // r0.x <- litColor.x; r0.y <- litColor.y; r0.z <- litColor.z

#line 143	//litColor.a = diffuseAlbedo.a
mov r0.w, r0.w  // r0.w <- litColor.w

#line 145	//return litColor
mov o0.xyz, r0.xyzx
mov o0.w, r0.w
ret 
// Approximately 110 instruction slots used