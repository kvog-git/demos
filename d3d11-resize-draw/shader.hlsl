struct VertexDesc
{
	float2 p : POSITION;
	float2 t : TEXCOORD0;
};

struct PixelDesc
{
	float4 p : SV_POSITION;
	float2 t : TEXCOORD0;
};

cbuffer Params : register(b0)
{
    float4 u_color;
	float4x4 u_rot;
};

PixelDesc VsMain(VertexDesc v)
{
	PixelDesc p;
	p.p = mul(u_rot, float4(v.p.x, v.p.y, 0.0f, 1.0f));
	p.t = v.t;
	return p;
}

float4 PsMain(PixelDesc p) : SV_Target
{
	return u_color;
}
