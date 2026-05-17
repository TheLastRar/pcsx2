// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "tfx_defines.hlsl"
#include "tfx_ps_resources.hlsl"

#ifndef VS_IIP
#define VS_IIP 0
#endif

#ifndef GS_IIP
#define GS_IIP 0
#define GS_FORWARD_PRIMID 0
#endif

#ifndef ZTST_GEQUAL
#define ZTST_GEQUAL 2
#define ZTST_GREATER 3
#endif

#ifndef AFAIL_KEEP
#define AFAIL_KEEP 0
#define AFAIL_FB_ONLY 1
#define AFAIL_ZB_ONLY 2
#define AFAIL_RGB_ONLY 3
#define AFAIL_RGB_ONLY_DSB 4
#define AFAIL_RGB_ONLY_SW_Z 5
#endif

#ifndef PS_AA1_NONE
#define PS_AA1_NONE 0
#define PS_AA1_LINE 1
#define PS_AA1_TRIANGLE 2
#define PS_AA1_TRIANGLE_SW_Z 3
#endif

#define SW_BLEND (PS_BLEND_A || PS_BLEND_B || PS_BLEND_D)
#define SW_BLEND_NEEDS_RT (SW_BLEND && (PS_BLEND_A == 1 || PS_BLEND_B == 1 || PS_BLEND_C == 1 || PS_BLEND_D == 1))
#define SW_AD_TO_HW (PS_BLEND_C == 1 && PS_A_MASKED)
#define NEEDS_RT_FOR_AFAIL (PS_AFAIL == AFAIL_ZB_ONLY || PS_AFAIL == AFAIL_RGB_ONLY || PS_AFAIL == AFAIL_RGB_ONLY_SW_Z)
#define NEEDS_DEPTH_FOR_AFAIL (PS_AFAIL == AFAIL_FB_ONLY || PS_AFAIL == AFAIL_RGB_ONLY_SW_Z)
#define NEEDS_DEPTH_FOR_ZTST (PS_ZTST == ZTST_GEQUAL || PS_ZTST == ZTST_GREATER)
#define NEEDS_DEPTH_FOR_AA1 (PS_AA1 == PS_AA1_TRIANGLE_SW_Z)
#define SW_DEPTH (NEEDS_DEPTH_FOR_AFAIL || NEEDS_DEPTH_FOR_ZTST || NEEDS_DEPTH_FOR_AA1)
#define ZWRITE (PS_ZFLOOR || PS_ZCLAMP || SW_DEPTH)

struct PS_INPUT
{
	noperspective centroid float4 p : SV_Position;
	float4 t : TEXCOORD0;
	float4 ti : TEXCOORD2;
#if VS_IIP != 0 || GS_IIP != 0 || PS_IIP != 0
	float4 c : COLOR0;
#else
	nointerpolation float4 c : COLOR0;
#endif
	float inv_cov : COLOR1; // We use the inverse to make it simpler to interpolate.
	nointerpolation uint interior : COLOR2; // 1 for triangle interior; 0 for edge;
#if (PS_DATE >= 1 && PS_DATE <= 3) || GS_FORWARD_PRIMID
	uint primid : SV_PrimitiveID;
#endif
};

struct PS_OUTPUT
{
#define NUM_RTS 0
#if !PS_NO_COLOR
#if PS_DATE == 1 || PS_DATE == 2
	float c : SV_Target;
#else
	float4 c0 : SV_Target0;
	#undef NUM_RTS
	#define NUM_RTS 1
#if !PS_NO_COLOR1
	float4 c1 : SV_Target1;
#endif
#endif
#endif
#if ZWRITE
	// In DX12 we do depth feedback loops with a color copy.
	#if SW_DEPTH && PS_NO_COLOR1 && DX12
		#if NUM_RTS > 0
			float depth_color : SV_Target1;
		#else
			float depth_color : SV_Target0;
		#endif
	#endif
	#if PS_HAS_CONSERVATIVE_DEPTH && !SW_DEPTH
		float depth : SV_DepthLessEqual;
	#else
		float depth : SV_Depth;
	#endif
#endif
#undef NUM_RTS
};

float4 fetch_red(int2 xy);
float4 fetch_green(int2 xy);
float4 fetch_blue(int2 xy);
float4 fetch_alpha(int2 xy);
float4 fetch_rgb(int2 xy);
float4 fetch_gXbY(int2 xy);

float4 sample_color(float2 st, float uv_w, int2 xy);
float4 sample_depth(float2 st, float2 pos);

float4 tfx(float4 T, float4 C);

float4 fog(float4 c, float f);

// Uses PS_INPUT
float4 ps_color(PS_INPUT input)
{
#if PS_FST == 0
	float2 st = input.t.xy / input.t.w;
	float2 st_int = input.ti.zw / input.t.w;
#else
	float2 st = input.ti.xy;
	float2 st_int = input.ti.zw;
#endif

#if PS_CHANNEL_FETCH == 1
	float4 T = fetch_red(int2(input.p.xy + ChannelShuffleOffset));
#elif PS_CHANNEL_FETCH == 2
	float4 T = fetch_green(int2(input.p.xy + ChannelShuffleOffset));
#elif PS_CHANNEL_FETCH == 3
	float4 T = fetch_blue(int2(input.p.xy + ChannelShuffleOffset));
#elif PS_CHANNEL_FETCH == 4
	float4 T = fetch_alpha(int2(input.p.xy + ChannelShuffleOffset));
#elif PS_CHANNEL_FETCH == 5
	float4 T = fetch_rgb(int2(input.p.xy + ChannelShuffleOffset));
#elif PS_CHANNEL_FETCH == 6
	float4 T = fetch_gXbY(int2(input.p.xy + ChannelShuffleOffset));
#elif PS_DEPTH_FMT > 0
	float4 T = sample_depth(st_int, input.p.xy);
#else
	float4 T = sample_color(st, input.t.w, int2(input.p.xy));
#endif

	if (PS_SHUFFLE && !PS_SHUFFLE_SAME && !PS_READ16_SRC && !(PS_PROCESS_BA == SHUFFLE_READWRITE && PS_PROCESS_RG == SHUFFLE_READWRITE))
	{
		uint4 denorm_c_before = uint4(T);
		if (PS_PROCESS_BA & SHUFFLE_READ)
		{
			T.r = float((denorm_c_before.b << 3) & 0xF8u);
			T.g = float(((denorm_c_before.b >> 2) & 0x38u) | ((denorm_c_before.a << 6) & 0xC0u));
			T.b = float((denorm_c_before.a << 1) & 0xF8u);
			T.a = float(denorm_c_before.a & 0x80u);
		}
		else
		{
			T.r = float((denorm_c_before.r << 3) & 0xF8u);
			T.g = float(((denorm_c_before.r >> 2) & 0x38u) | ((denorm_c_before.g << 6) & 0xC0u));
			T.b = float((denorm_c_before.g << 1) & 0xF8u);
			T.a = float(denorm_c_before.g & 0x80u);
		}
		
		T.a = (T.a >= 127.5f ? TA.y : !PS_AEM || any(int3(T.rgb) & 0xF8) ? TA.x : 0) * 255.0f;
	}

	float4 C = tfx(T, input.c);

	C = fog(C, input.t.z);

	return C;
}

bool atst(float4 C);

void ps_dither(inout float3 C, float As, float2 pos_xy);
void ps_fbmask(inout float4 C, float2 pos_xy);
void ps_color_clamp_wrap(inout float3 C);
void ps_blend(inout float4 Color, inout float4 As_rgba, float2 pos_xy);

[shader("pixel")]
PS_OUTPUT ps_main(PS_INPUT input)
{
	// Must floor before depth testing.
#if PS_ZFLOOR
	input.p.z = floor(input.p.z * exp2(32.0f)) * exp2(-32.0f);
#endif

#if PS_ZTST == ZTST_GEQUAL
	if (input.p.z < DepthTexture.Load(int3(input.p.xy, 0)).r)
		discard;
#elif PS_ZTST == ZTST_GREATER
	if (input.p.z <= DepthTexture.Load(int3(input.p.xy, 0)).r)
		discard;
#endif
	float4 C = ps_color(input);

#if PS_AA1
	float cov = clamp(1.0f - abs(input.inv_cov), 0.0f, 1.0f);
	#if PS_ABE
		if (floor(C.a) == 128.0f) // The coverage is only used if the fragment alpha is 128.
			C.a = 128.0f * cov;
	#else
		C.a = 128.0f * cov;
	#endif
#elif PS_FIXED_ONE_A
	// AA (Fixed one) will output a coverage of 1.0 as alpha
	C.a = 128.0f;
#endif

	bool atst_pass = atst(C);

#if PS_AFAIL == AFAIL_KEEP
	if (!atst_pass)
		discard;
#endif

	PS_OUTPUT output;

	if (PS_SCANMSK & 2)
	{
		// fail depth test on prohibited lines
		if ((int(input.p.y) & 1) == (PS_SCANMSK & 1))
			discard;
	}

	float4 alpha_blend = (float4)0.0f;
	if (SW_AD_TO_HW)
	{
		float4 RT = PS_RTA_CORRECTION ? trunc(RtTexture.Load(int3(input.p.xy, 0)) * 128.0f + 0.1f) : trunc(RtTexture.Load(int3(input.p.xy, 0)) * 255.0f + 0.1f);
		alpha_blend = (float4)(RT.a / 128.0f);
	}
	else
	{
		alpha_blend = (float4)(C.a / 128.0f);
	}

	// Alpha correction
	if (PS_DST_FMT == FMT_16)
	{
		float A_one = 128.0f; // alpha output will be 0x80
		C.a = PS_FBA ? A_one : step(A_one, C.a) * A_one;
	}
	else if ((PS_DST_FMT == FMT_32) && PS_FBA)
	{
		float A_one = 128.0f;
		if (C.a < A_one) C.a += A_one;
	}

#if PS_DATE >= 5

#if PS_WRITE_RG == 1
	// Pseudo 16 bits access.
	float rt_a = RtTexture.Load(int3(input.p.xy, 0)).g;
#else
	float rt_a = RtTexture.Load(int3(input.p.xy, 0)).a;
#endif

#if (PS_DATE & 3) == 1
	// DATM == 0: Pixel with alpha equal to 1 will failed
	#if PS_RTA_CORRECTION
		bool bad = (254.5f / 255.0f) < rt_a;
	#else
		bool bad = (127.5f / 255.0f) < rt_a;
	#endif
#elif (PS_DATE & 3) == 2
	// DATM == 1: Pixel with alpha equal to 0 will failed
	#if PS_RTA_CORRECTION
		bool bad = rt_a < (254.5f / 255.0f);
	#else
		bool bad = rt_a < (127.5f / 255.0f);
	#endif
#endif

	if (bad)
		discard;

#endif

#if PS_DATE == 3
	// Note gl_PrimitiveID == stencil_ceil will be the primitive that will update
	// the bad alpha value so we must keep it.
	int stencil_ceil = int(PrimMinTexture.Load(int3(input.p.xy, 0)));
	if (int(input.primid) > stencil_ceil)
		discard;
#endif

	// Get first primitive that will write a failling alpha value
#if PS_DATE == 1
	// DATM == 0
	// Pixel with alpha equal to 1 will failed (128-255)
	output.c = (C.a > 127.5f) ? float(input.primid) : float(0x7FFFFFFF);

#elif PS_DATE == 2

	// DATM == 1
	// Pixel with alpha equal to 0 will failed (0-127)
	output.c = (C.a < 127.5f) ? float(input.primid) : float(0x7FFFFFFF);

#else
	// Not primid DATE setup

	ps_blend(C, alpha_blend, input.p.xy);

	if (PS_SHUFFLE)
	{
		if (!PS_SHUFFLE_SAME && !PS_READ16_SRC && !(PS_PROCESS_BA == SHUFFLE_READWRITE && PS_PROCESS_RG == SHUFFLE_READWRITE))
		{
			uint4 denorm_c_after = uint4(C);
			if (PS_PROCESS_BA & SHUFFLE_READ)
			{
				C.b = float(((denorm_c_after.r >> 3) & 0x1Fu) | ((denorm_c_after.g << 2) & 0xE0u));
				C.a = float(((denorm_c_after.g >> 6) & 0x3u) | ((denorm_c_after.b >> 1) & 0x7Cu) | (denorm_c_after.a & 0x80u));
			}
			else
			{
				C.r = float(((denorm_c_after.r >> 3) & 0x1Fu) | ((denorm_c_after.g << 2) & 0xE0u));
				C.g = float(((denorm_c_after.g >> 6) & 0x3u) | ((denorm_c_after.b >> 1) & 0x7Cu) | (denorm_c_after.a & 0x80u));
			}
		}


		// Special case for 32bit input and 16bit output, shuffle used by The Godfather
		if (PS_SHUFFLE_SAME)
		{
			uint4 denorm_c = uint4(C);
			
			if (PS_PROCESS_BA & SHUFFLE_READ)
				C = (float4)(float((denorm_c.b & 0x7Fu) | (denorm_c.a & 0x80u)));
			else
				C.ga = C.rg;
		}
		// Copy of a 16bit source in to this target
		else if (PS_READ16_SRC)
		{
			uint4 denorm_c = uint4(C);
			uint2 denorm_TA = uint2(float2(TA.xy) * 255.0f + 0.5f);
			C.rb = (float2)float((denorm_c.r >> 3) | (((denorm_c.g >> 3) & 0x7u) << 5));
			C.ga = (float2)float((denorm_c.g >> 6) | ((denorm_c.b >> 3) << 2) | (denorm_TA.x & 0x80u));
		}
		else if (PS_SHUFFLE_ACROSS)
		{
			if (PS_PROCESS_BA == SHUFFLE_READWRITE && PS_PROCESS_RG == SHUFFLE_READWRITE)
			{
				C.br = C.rb;
				C.ag = C.ga;
			}
			else if(PS_PROCESS_BA & SHUFFLE_READ)
			{
				C.rb = C.bb;
				C.ga = C.aa;
			}
			else
			{
				C.rb = C.rr;
				C.ga = C.gg;
			}
		}
	}

	ps_dither(C.rgb, alpha_blend.a, input.p.xy);

	// Color clamp/wrap needs to be done after sw blending and dithering
	ps_color_clamp_wrap(C.rgb);

	ps_fbmask(C, input.p.xy);

#if (PS_AFAIL == AFAIL_RGB_ONLY_DSB) && !PS_NO_COLOR1
	// Use alpha blend factor to determine whether to update A.
	alpha_blend.a = float(atst_pass);
#endif

#if !PS_NO_COLOR
	output.c0.a = PS_RTA_CORRECTION ? C.a / 128.0f : C.a / 255.0f;
	output.c0.rgb = PS_COLCLIP_HW ? float3(C.rgb / 65535.0f) : C.rgb / 255.0f;
#if !PS_NO_COLOR1
	output.c1 = alpha_blend;
#endif

	// Alpha test with feedback
#if PS_AFAIL == AFAIL_FB_ONLY
	if (!atst_pass)
		input.p.z = DepthTexture.Load(int3(input.p.xy, 0)).r;
#elif PS_AFAIL == AFAIL_ZB_ONLY
	if (!atst_pass)
		output.c0 = RtTexture.Load(int3(input.p.xy, 0));
#elif PS_AFAIL == AFAIL_RGB_ONLY || PS_AFAIL == AFAIL_RGB_ONLY_SW_Z
	if (!atst_pass)
	{
	output.c0.a = RtTexture.Load(int3(input.p.xy, 0)).a;
	#if PS_AFAIL == AFAIL_RGB_ONLY_SW_Z
		input.p.z = DepthTexture.Load(int3(input.p.xy, 0)).r; 
	#endif
	}
#endif

#endif // !PS_NO_COLOR

#endif // PS_DATE != 1/2

#if PS_ZCLAMP
	input.p.z = min(input.p.z, MaxDepthPS);
#endif

#if PS_AA1 == PS_AA1_TRIANGLE_SW_Z
	if (!bool(input.interior))
		input.p.z = DepthTexture.Load(int3(input.p.xy, 0)).r; // No depth update for triangle edges.
#endif

#if ZWRITE
#if SW_DEPTH && PS_NO_COLOR1 && DX12
	// Output color clone for feedback as well as real depth.
	output.depth_color = input.p.z;
#endif
	output.depth = input.p.z;
#endif

	return output;
}
