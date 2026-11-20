// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#pragma once
namespace fb
{
inline constexpr const char *shader = R"hlsl(
cbuffer Settings:register(b0){int2 size;int radius;int protect;}
Texture2D<uint> before:register(t0);
Texture2D<uint> after:register(t1);
Texture2D<int4> flow:register(t2);
RWTexture2D<int4> motion:register(u0);
RWTexture2D<uint> middle:register(u1);
int2 bounds(int2 p){return clamp(p,int2(0,0),size-1);}
int luma(uint v){return int(((v&255)*29+((v>>8)&255)*150+((v>>16)&255)*77)>>8);}
int cost(int2 origin,int2 d){
    int s=abs(d.x)+abs(d.y);
    [unroll]for(int y=2;y<=14;y+=6)[unroll]for(int x=2;x<=14;x+=6){
        int2 p=origin+int2(x,y);
        s+=abs(luma(before[bounds(p-d)])-luma(after[bounds(p+d)]));
    }return s;
}
[numthreads(8,8,1)]void estimate(uint3 gid:SV_DispatchThreadID){
    int2 origin=int2(gid.xy)*16;if(any(origin>=size))return;
    int2 best=0;int e=cost(origin,0);
    if(e>9){
        [loop]for(int j=-radius;j<=radius;j++)[loop]for(int i=-radius;i<=radius;i++){
            int2 d=int2(i,j)*4;int v=cost(origin,d);if(v<e){e=v;best=d;}
        }
        int2 base=best;
        [unroll]for(int j=-2;j<=2;j++)[unroll]for(int i=-2;i<=2;i++){
            int2 d=base+int2(i,j);int v=cost(origin,d);if(v<e){e=v;best=d;}
        }
    }
    motion[gid.xy]=int4(best,e,0);
}
int contrast(uint a,uint b){int3 ca=int3(a&255,(a>>8)&255,(a>>16)&255),cb=int3(b&255,(b>>8)&255,(b>>16)&255);int3 d=abs(ca-cb);return max(d.r,max(d.g,d.b));}
bool ink(uint center,int2 p,int2 q){q=bounds(q);return any(p!=q)&&(((center^before[q])|(center^after[q]))&0xffffffu)==0;}
bool edge(uint center,int2 pt){pt=bounds(pt);return min(contrast(center,before[pt]),contrast(center,after[pt]))>64;}
[numthreads(16,16,1)]void warp(uint3 gid:SV_DispatchThreadID){
    int2 p=gid.xy;if(any(p>=size))return;
    int4 v=flow[p/16];uint a=before[bounds(p-v.xy)],b=after[bounds(p+v.xy)];
    uint center=before[p];bool fixedOverlay=false;
    if((protect&2)!=0 && ((center^after[p])&0xffffffu)==0 && (((a^center)|(b^center))&0xffffffu)!=0)
        fixedOverlay=(ink(center,p,p+int2(1,0))||ink(center,p,p-int2(1,0))||ink(center,p,p+int2(0,1))||ink(center,p,p-int2(0,1))) && (((a^b)&0xf8f8f8u)!=0 || edge(center,p+int2(2,0)) || edge(center,p+int2(0,2)));
    middle[p]=(fixedOverlay||((protect&1)!=0&&v.z>300))?before[p]:(0xff000000u|(((a&b)+(((a^b)&0xfefefe)>>1))&0xffffffu));
}
struct Vertex{float4 pos:SV_Position;float2 uv:TEXCOORD0;};
Vertex vertex(uint id:SV_VertexID){Vertex v;v.uv=float2((id<<1)&2,id&2);v.pos=float4(v.uv*float2(2,-2)+float2(-1,1),0,1);return v;}
float4 pixel(Vertex v):SV_Target{
    uint c=before[bounds(int2(v.uv*size))];
    return float4((c>>16)&255,(c>>8)&255,c&255,255)/255.;
}
// NVOFA writes signed 10.5 fixed-point vectors on a four-pixel grid.
Texture2D<float4> nvBefore:register(t4);
Texture2D<float4> nvAfter:register(t5);
Texture2D<int2> nvForward:register(t6);
Texture2D<int2> nvBackward:register(t7);
SamplerState linearClamp:register(s0);
float2 nvMotion(Texture2D<int2> field, float2 p){
    return field[bounds(int2(round(p))) / 4] / 32.0;
}
[numthreads(16,16,1)] void nvWarp(uint3 gid:SV_DispatchThreadID){
    int2 p=gid.xy;
    if(any(p>=size))return;
    // Suppress subpixel shimmer when both frames contain the same local texture.
    float3 stationary=nvBefore[p].rgb;
    if(all(stationary==nvAfter[p].rgb) &&
       all(nvBefore[bounds(p+int2(1,0))].rgb==nvAfter[bounds(p+int2(1,0))].rgb) &&
       all(nvBefore[bounds(p+int2(0,1))].rgb==nvAfter[bounds(p+int2(0,1))].rgb) &&
       all(nvBefore[bounds(p-int2(1,0))].rgb==nvAfter[bounds(p-int2(1,0))].rgb) &&
       all(nvBefore[bounds(p-int2(0,1))].rgb==nvAfter[bounds(p-int2(0,1))].rgb)){
        uint3 rgb=uint3(round(stationary*255));
        middle[p]=0xff000000u|(rgb.r<<16)|(rgb.g<<8)|rgb.b;
        return;
    }
    float2 a=p,b=p;
    // Invert each endpoint-to-midpoint displacement at the destination pixel.
    [unroll] for(int i=0;i<2;++i){
        a=p-0.5*nvMotion(nvForward,a);
        b=p-0.5*nvMotion(nvBackward,b);
    }
    float3 ca=nvBefore.SampleLevel(linearClamp,(a+0.5)/size,0).rgb;
    float3 cb=nvAfter.SampleLevel(linearClamp,(b+0.5)/size,0).rgb;
    float blend=0.5;
    if((protect&1)!=0){
        float2 f=nvMotion(nvForward,a), r=nvMotion(nvBackward,b);
        float errorA=length(f+nvMotion(nvBackward,a+f));
        float errorB=length(r+nvMotion(nvForward,b+r));
        blend=(1+errorA)/(2+errorA+errorB);
    }
    uint3 rgb=uint3(round(saturate(lerp(ca,cb,blend))*255));
    middle[p]=0xff000000u|(rgb.r<<16)|(rgb.g<<8)|rgb.b;
}
float4 nvOriginal(Vertex v):SV_Target{
    return float4(nvBefore[bounds(int2(v.uv*size))].rgb,1);
}
)hlsl";
}
