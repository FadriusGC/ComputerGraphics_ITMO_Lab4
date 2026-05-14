struct PS_INPUT { float4 Pos : SV_POSITION; float2 TexC : TEXCOORD; };
Texture2D gAlbedo : register(t0);
Texture2D gNormal : register(t1);
Texture2D gDepth : register(t2);
Texture2DArray gShadowMap : register(t3);
SamplerState gSampler : register(s0);
SamplerComparisonState gShadowSampler : register(s1);

static const uint LIGHT_TYPE_POINT = 0;
static const uint LIGHT_TYPE_DIRECTIONAL = 1;
static const uint LIGHT_TYPE_SPOT = 2;
static const uint MAX_LIGHTS = 100;
static const uint CASCADE_COUNT = 3;

struct GpuLight { float4 PositionWorldAndRange; float4 DirectionAndType; float4 ColorAndIntensity; float4 Params; };
cbuffer cbCompose : register(b0) {
    float4x4 gInvViewProj; float4 gCameraPosition; float4 gScreenSize; float4 gCameraForward;
    float4 gLightCount; float4 gCascadeSplits; float4 gShadowTexelSize; float4x4 gLightViewProj[CASCADE_COUNT];
    GpuLight gLights[MAX_LIGHTS];
};
float3 ReconstructWorldPos(float2 uv, float depth) { float4 ndc=float4(uv.x*2-1,1-uv.y*2,depth,1); float4 w=mul(ndc,gInvViewProj); return w.xyz/max(w.w,1e-6f);} 
float SampleShadowPCF(float3 worldPos, float3 normal, float3 lightDir){
    float dist = dot(worldPos - gCameraPosition.xyz, gCameraForward.xyz);
    uint ci = (dist < gCascadeSplits.x)?0:((dist < gCascadeSplits.y)?1:2);
    float4 lp = mul(float4(worldPos,1), gLightViewProj[ci]);
    float3 proj = lp.xyz/max(lp.w,1e-6f);
    float2 uv = proj.xy*float2(0.5,-0.5)+0.5;
    float depth = proj.z - max(0.0005, 0.002 * (1.0 - saturate(dot(normal, -lightDir))));
    if(any(uv<0)||any(uv>1)||depth>1) return 1.0;
    float2 texel = gShadowTexelSize.xy;
    float s=0;
    [unroll] for(int y=-1;y<=1;y++) [unroll] for(int x=-1;x<=1;x++) s += gShadowMap.SampleCmpLevelZero(gShadowSampler,float3(uv+float2(x,y)*texel,ci),depth);
    return s/9.0;
}

float3 EvaluateLight(uint t, GpuLight l, float3 wp, float3 n, float3 v, float r){
 float3 rad=l.ColorAndIntensity.rgb*l.ColorAndIntensity.a; float3 L=0; float att=1;
 if(t==LIGHT_TYPE_DIRECTIONAL){L=normalize(-l.DirectionAndType.xyz);} else {float3 tl=l.PositionWorldAndRange.xyz-wp; float d=length(tl); if(d<=1e-4)return 0; L=tl/d; float range=max(l.PositionWorldAndRange.w,1e-3); float rf=saturate(1-d/range); att=rf*rf; if(t==LIGHT_TYPE_SPOT){float3 sd=normalize(-l.DirectionAndType.xyz); float c=dot(L,sd); att*=saturate((c-l.Params.y)/max(l.Params.x-l.Params.y,1e-4));}}
 float shadow = (t==LIGHT_TYPE_DIRECTIONAL)?SampleShadowPCF(wp,n,L):1.0;
 float NdotL=saturate(dot(n,L)); float3 diff=rad*NdotL*att*shadow; float3 h=normalize(L+v); float spec=pow(saturate(dot(n,h)),lerp(64,4,saturate(r)))*att*shadow; return diff+rad*spec*lerp(0.25,0.04,saturate(r));}
float4 PS(PS_INPUT i):SV_Target{ float4 a=gAlbedo.Sample(gSampler,i.TexC); float4 ns=gNormal.Sample(gSampler,i.TexC); float d=gDepth.Sample(gSampler,i.TexC).r; float3 n=normalize(ns.xyz*2-1); float rough=saturate(ns.a); float3 wp=ReconstructWorldPos(i.TexC,d); float3 v=normalize(gCameraPosition.xyz-wp); float3 c=a.rgb*0.05; uint lc=min((uint)gLightCount.x,MAX_LIGHTS); [loop] for(uint k=0;k<lc;k++) c+=a.rgb*EvaluateLight((uint)gLights[k].DirectionAndType.w,gLights[k],wp,n,v,rough); return float4(saturate(c),a.a);} 