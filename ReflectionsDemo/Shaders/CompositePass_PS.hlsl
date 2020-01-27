// Tonemapping methods
#define TM_Linear     0
#define TM_Reinhard   1
#define TM_ReinhardSq 2
#define TM_ACESFilmic 3

#define TM_HDR			0
#define TM_Albedo		1
#define TM_Normal		2
#define TM_Metal		3
#define TM_Roughness	4
#define TM_Emissive		5
#define TM_Occlusion	6
#define TM_Depth		7
#define TM_Shadows		8

struct TonemapParameters
{
	// The method to use to perform tonemapping.
	uint TonemapMethod;
	// Exposure should be expressed as a relative expsure value (-2, -1, 0, +1, +2 )
	float Exposure;

	// The maximum luminance to use for linear tonemapping.
	float MaxLuminance;

	// Reinhard constant. Generlly this is 1.0.
	float K;

	// ACES Filmic parameters
	// See: https://www.slideshare.net/ozlael/hable-john-uncharted2-hdr-lighting/142
	float A; // Shoulder strength
	float B; // Linear strength
	float C; // Linear angle
	float D; // Toe strength
	float E; // Toe Numerator
	float F; // Toe denominator
	// Note E/F = Toe angle.
	float LinearWhite;

	float Gamma;
};

struct OutputMode
{
	uint OutputTexture;
	float NearPlane;
	float FarPlane;
};

// Linear Tonemapping
// Just normalizing based on some maximum luminance
float3 Linear(float3 HDR, float max)
{
	float3 SDR = HDR;
	if (max > 0)
	{
		SDR = saturate(HDR / max);
	}

	return SDR;
}

// Reinhard tone mapping.
// See: http://www.cs.utah.edu/~reinhard/cdrom/tonemap.pdf
float3 Reinhard(float3 HDR, float k)
{
	return HDR / (HDR + k);
}

// Reinhard^2
// See: http://www.cs.utah.edu/~reinhard/cdrom/tonemap.pdf
float3 ReinhardSqr(float3 HDR, float k)
{
	return pow(Reinhard(HDR, k), 2);
}

// ACES Filmic
// See: https://www.slideshare.net/ozlael/hable-john-uncharted2-hdr-lighting/142
float3 ACESFilmic(float3 x, float A, float B, float C, float D, float E, float F)
{
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - (E / F);
}

ConstantBuffer<TonemapParameters> TonemapParametersCB	: register(b0);
ConstantBuffer<OutputMode> OutputModeCB					: register(b1);

Texture2D<float4> OutputTexture							: register(t0);

SamplerState PointClampSampler							: register(s0);

float4 DoHDR(float4 s)
{
	float3 HDR = s.xyz;

	// Add exposure to HDR result.
	HDR *= exp2(TonemapParametersCB.Exposure);

	// Perform tonemapping on HDR image.
	float3 SDR = (float3)0;
	switch (TonemapParametersCB.TonemapMethod)
	{
	case TM_Linear:
		SDR = Linear(HDR, TonemapParametersCB.MaxLuminance);
		break;
	case TM_Reinhard:
		SDR = Reinhard(HDR, TonemapParametersCB.K);
		break;
	case TM_ReinhardSq:
		SDR = ReinhardSqr(HDR, TonemapParametersCB.K);
		break;
	case TM_ACESFilmic:
		SDR = ACESFilmic(HDR, TonemapParametersCB.A, TonemapParametersCB.B, TonemapParametersCB.C, TonemapParametersCB.D, TonemapParametersCB.E, TonemapParametersCB.F) /
			ACESFilmic(TonemapParametersCB.LinearWhite, TonemapParametersCB.A, TonemapParametersCB.B, TonemapParametersCB.C, TonemapParametersCB.D, TonemapParametersCB.E, TonemapParametersCB.F);
		break;
	}

	return float4(pow(abs(SDR), 1.0 / TonemapParametersCB.Gamma), 1.0);
}

float4 DoDepth(float depth)
{
	float z = depth * 2.0 - 1.0;

	float linear_depth = (2.0 * OutputModeCB.NearPlane * OutputModeCB.FarPlane) / (OutputModeCB.FarPlane + OutputModeCB.NearPlane - z * (OutputModeCB.FarPlane - OutputModeCB.NearPlane));
	float linear_z = linear_depth / OutputModeCB.FarPlane;

	return float4(linear_z, linear_z, linear_z, 1.0);
}

float4 main(float2 TexCoord : TEXCOORD) : SV_Target0
{
	float4 texture_sample = OutputTexture.SampleLevel(PointClampSampler, TexCoord, 0);

	// Set default output color.
	float4 output = float4(0.0, 0.0, 0.0, 1.0);

	switch (OutputModeCB.OutputTexture)
	{
	case TM_HDR:
		output = DoHDR(texture_sample);
		break;
	case TM_Albedo:
		output = texture_sample;
		break;
	case TM_Normal:
		output = float4(texture_sample.rgb, 1.0);
		break;
	case TM_Metal:
		output = float4(texture_sample.r, texture_sample.r, texture_sample.r, 1.0);
		break;
	case TM_Roughness:
		output = float4(texture_sample.g, texture_sample.g, texture_sample.g, 1.0);
		break;
	case TM_Emissive:
		output = float4(texture_sample.rgb, 1.0);
		break;
	case TM_Occlusion:
		output = float4(texture_sample.a, texture_sample.a, texture_sample.a, 1.0);
		break;
	case TM_Depth:
		output = DoDepth(texture_sample.r);
		break;
	case TM_Shadows:
		output = float4(texture_sample.r, texture_sample.r, texture_sample.r, 1.0);
		break;
	}

	return output;
}