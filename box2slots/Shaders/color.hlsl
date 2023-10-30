struct VPosData {
  float3 PosL : POSITION;
};
struct VColorData {
  float4 Color : COLOR;
};
cbuffer cbPerObject : register(b0) {
  float4x4 gWorldViewProj;
  float gTime;
};

// struct VertexIn
//{
//     float3 PosL : POSITION;
//     float4 Color : COLOR;
// };

struct VertexOut {
  float4 PosH : SV_POSITION;
  float4 Color : COLOR;
};
VertexOut VS(VPosData vp, VColorData vc) {
  VertexOut vout;

  // Transform to homogeneous clip space.
  vp.PosL.xy += 0.5f * sin(vp.PosL.x) * sin(3.0f * gTime);
  vp.PosL.z += 0.f + 0.4f * sin(2.0f * gTime);
  vout.PosH = mul(float4(vp.PosL, 1.0f), gWorldViewProj);

  // Just pass vertex color into the pixel shader.
  vout.Color = vc.Color * sin(2.0f * gTime) * 2.9;
  return vout;
}
// VertexOut VS(VertexIn vin)
//{
//     VertexOut vout;

//    // Transform to homogeneous clip space.
//    vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);

//    // Just pass vertex color into the pixel shader.
//    vout.Color = vin.Color;

//    return vout;
//}

float4 PS(VertexOut pin) : SV_Target {
  clip(pin.Color.r - 0.01f);
  return pin.Color;
}