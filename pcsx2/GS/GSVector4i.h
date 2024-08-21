// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Assertions.h"
#include <algorithm>

class alignas(16) GSVector4i
{
	static const GSVector4i m_xff[17];
	static const GSVector4i m_x0f[17];

	struct cxpr_init_tag {};
	static constexpr cxpr_init_tag cxpr_init{};

	constexpr GSVector4i(cxpr_init_tag, int x, int y, int z, int w)
		: I32{x, y, z, w}
	{
	}

	constexpr GSVector4i(cxpr_init_tag, short s0, short s1, short s2, short s3, short s4, short s5, short s6, short s7)
		: I16{s0, s1, s2, s3, s4, s5, s6, s7}
	{
	}

	constexpr GSVector4i(cxpr_init_tag, char b0, char b1, char b2, char b3, char b4, char b5, char b6, char b7, char b8, char b9, char b10, char b11, char b12, char b13, char b14, char b15)
#if !defined(__APPLE__) && !defined(_MSC_VER)
		: U8{b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12, b13, b14, b15}
#else
		: I8{b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12, b13, b14, b15}
#endif
	{
	}

public:
	union
	{
		struct { int x, y, z, w; };
		struct { int r, g, b, a; };
		struct { int left, top, right, bottom; };
		int v[4];
		float F32[4];
		s8  I8[16];
		s16 I16[8];
		s32 I32[4];
		s64 I64[2];
		u8  U8[16];
		u16 U16[8];
		u32 U32[4];
		u64 U64[2];
	};

	GSVector4i() = default;

	constexpr static GSVector4i cxpr(int x, int y, int z, int w)
	{
		return GSVector4i(cxpr_init, x, y, z, w);
	}

	constexpr static GSVector4i cxpr(int x)
	{
		return GSVector4i(cxpr_init, x, x, x, x);
	}

	constexpr static GSVector4i cxpr16(short s0, short s1, short s2, short s3, short s4, short s5, short s6, short s7)
	{
		return GSVector4i(cxpr_init, s0, s1, s2, s3, s4, s5, s6, s7);
	}

	constexpr static GSVector4i cxpr8(char b0, char b1, char b2, char b3, char b4, char b5, char b6, char b7, char b8, char b9, char b10, char b11, char b12, char b13, char b14, char b15)
	{
		return GSVector4i(cxpr_init, b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12, b13, b14, b15);
	}

	__forceinline GSVector4i(int x, int y, int z, int w)
		: I32{x, y, z, w}
	{
	}

	__forceinline GSVector4i(int x, int y)
		: I32{x, y, 0, 0}
	{
	}

	__forceinline GSVector4i(short s0, short s1, short s2, short s3, short s4, short s5, short s6, short s7)
		: I16{s0, s1, s2, s3, s4, s5, s6, s7}
	{
	}

	constexpr GSVector4i(char b0, char b1, char b2, char b3, char b4, char b5, char b6, char b7, char b8, char b9, char b10, char b11, char b12, char b13, char b14, char b15)
#if !defined(__APPLE__) && !defined(_MSC_VER)
		: U8{b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12, b13, b14, b15}
#else
		: I8{b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12, b13, b14, b15}
#endif
	{
	}

	__forceinline explicit GSVector4i(const GSVector2i& v)
		: I32{v.x, v.y, 0, 0}
	{
	}

	// MSVC has bad codegen for the constexpr version when applied to non-constexpr things (https://godbolt.org/z/h8qbn7), so leave the non-constexpr version default
	__forceinline explicit GSVector4i(int i)
	{
		*this = i;
	}

	#ifndef _M_ARM64
	__forceinline explicit GSVector4i(__m128i m)
		: I32{_mm_extract_epi32(m, 0), _mm_extract_epi32(m, 1), _mm_extract_epi32(m, 2), _mm_extract_epi32(m, 3)}
	{
	}
	#else
	__forceinline explicit GSVector4i(int32x4_t m)
		: I32{vgetq_lane_s32(m, 0), vgetq_lane_s32(m, 1), vgetq_lane_s32(m, 2), vgetq_lane_s32(m, 3)}
	{
	}
	#endif

	__forceinline explicit GSVector4i(const GSVector4& v, bool truncate = true);

	__forceinline static GSVector4i cast(const GSVector4& v);

	__forceinline void operator=(int i)
	{
		I32[0] = i;
		I32[1] = i;
		I32[2] = i;
		I32[3] = i;
	}

	#ifndef _M_ARM64
	__forceinline operator __m128i() const
	{
		return _mm_stream_load_si128((__m128i*)U8);
	}
	#else
	__forceinline operator int32x4_t() const
	{
		return vreinterpretq_s32_s64(vld1q_s64((int64_t*)U8));
	}
	#endif

	// rect

	__forceinline int width() const
	{
		return right - left;
	}

	__forceinline int height() const
	{
		return bottom - top;
	}

	__forceinline GSVector4i rsize() const
	{
		return *this - xyxy(); // same as GSVector4i(0, 0, width(), height());
	}

	__forceinline unsigned int rarea() const
	{
		return width() * height();
	}

	__forceinline bool rempty() const
	{
		return (*this < zwzw()).mask() != 0x00ff;
	}

	__forceinline GSVector4i runion(const GSVector4i& a) const
	{
		return min_i32(a).upl64(max_i32(a).srl<8>());
	}

	__forceinline GSVector4i rintersect(const GSVector4i& a) const
	{
		return sat_i32(a);
	}

	__forceinline bool rintersects(const GSVector4i& v) const
	{
		return !rintersect(v).rempty();
	}

	__forceinline bool rcontains(const GSVector4i& v) const
	{
		return rintersect(v).eq(v);
	}

	template <Align_Mode mode>
	GSVector4i _ralign_helper(const GSVector4i& mask) const
	{
		GSVector4i v;

		switch (mode)
		{
			case Align_Inside:  v = *this + mask;        break;
			case Align_Outside: v = *this + mask.zwxy(); break;
			case Align_NegInf:  v = *this;               break;
			case Align_PosInf:  v = *this + mask.xyxy(); break;
			default: pxAssert(0); break;
		}

		return v.andnot(mask.xyxy());
	}

	/// Align the rect using mask values that already have one subtracted (1 << n - 1 aligns to 1 << n)
	template <Align_Mode mode>
	GSVector4i ralign_presub(const GSVector2i& a) const
	{
		return _ralign_helper<mode>(GSVector4i(a));
	}

	template <Align_Mode mode>
	GSVector4i ralign(const GSVector2i& a) const
	{
		// a must be 1 << n

		return _ralign_helper<mode>(GSVector4i(a) - GSVector4i(1, 1));
	}

	GSVector4i fit(int arx, int ary) const;

	GSVector4i fit(int preset) const;

	//

	__forceinline u32 rgba32() const
	{
		GSVector4i v = *this;

		v = v.ps32(v);
		v = v.pu16(v);

		return (u32)store(v);
	}

	__forceinline GSVector4i sat_i8(const GSVector4i& a, const GSVector4i& b) const
	{
		return max_i8(a).min_i8(b);
	}

	__forceinline GSVector4i sat_i8(const GSVector4i& a) const
	{
		return max_i8(a.xyxy()).min_i8(a.zwzw());
	}

	__forceinline GSVector4i sat_i16(const GSVector4i& a, const GSVector4i& b) const
	{
		return max_i16(a).min_i16(b);
	}

	__forceinline GSVector4i sat_i16(const GSVector4i& a) const
	{
		return max_i16(a.xyxy()).min_i16(a.zwzw());
	}

	__forceinline GSVector4i sat_i32(const GSVector4i& a, const GSVector4i& b) const
	{
		return max_i32(a).min_i32(b);
	}

	__forceinline GSVector4i sat_i32(const GSVector4i& a) const
	{
		return max_i32(a.xyxy()).min_i32(a.zwzw());
	}

	__forceinline GSVector4i sat_u8(const GSVector4i& a, const GSVector4i& b) const
	{
		return max_u8(a).min_u8(b);
	}

	__forceinline GSVector4i sat_u8(const GSVector4i& a) const
	{
		return max_u8(a.xyxy()).min_u8(a.zwzw());
	}

	__forceinline GSVector4i sat_u16(const GSVector4i& a, const GSVector4i& b) const
	{
		return max_u16(a).min_u16(b);
	}

	__forceinline GSVector4i sat_u16(const GSVector4i& a) const
	{
		return max_u16(a.xyxy()).min_u16(a.zwzw());
	}

	__forceinline GSVector4i sat_u32(const GSVector4i& a, const GSVector4i& b) const
	{
		return max_u32(a).min_u32(b);
	}

	__forceinline GSVector4i sat_u32(const GSVector4i& a) const
	{
		return max_u32(a.xyxy()).min_u32(a.zwzw());
	}

	__forceinline GSVector4i min_i8(const GSVector4i& a) const
	{
		return GSVector4i(
			std::min(I8[0], a.I8[0]),
			std::min(I8[1], a.I8[1]),
			std::min(I8[2], a.I8[2]),
			std::min(I8[3], a.I8[3]),
			std::min(I8[4], a.I8[4]),
			std::min(I8[5], a.I8[5]),
			std::min(I8[6], a.I8[6]),
			std::min(I8[7], a.I8[7]),
			std::min(I8[8], a.I8[8]),
			std::min(I8[9], a.I8[9]),
			std::min(I8[10], a.I8[10]),
			std::min(I8[11], a.I8[11]),
			std::min(I8[12], a.I8[12]),
			std::min(I8[13], a.I8[13]),
			std::min(I8[14], a.I8[14]),
			std::min(I8[15], a.I8[15]));
	}

	__forceinline GSVector4i max_i8(const GSVector4i& a) const
	{
		return GSVector4i(
			std::max(I8[0], a.I8[0]),
			std::max(I8[1], a.I8[1]),
			std::max(I8[2], a.I8[2]),
			std::max(I8[3], a.I8[3]),
			std::max(I8[4], a.I8[4]),
			std::max(I8[5], a.I8[5]),
			std::max(I8[6], a.I8[6]),
			std::max(I8[7], a.I8[7]),
			std::max(I8[8], a.I8[8]),
			std::max(I8[9], a.I8[9]),
			std::max(I8[10], a.I8[10]),
			std::max(I8[11], a.I8[11]),
			std::max(I8[12], a.I8[12]),
			std::max(I8[13], a.I8[13]),
			std::max(I8[14], a.I8[14]),
			std::max(I8[15], a.I8[15]));
	}

	__forceinline GSVector4i min_i16(const GSVector4i& a) const
	{
		return GSVector4i(
			std::min(I16[0], a.I16[0]),
			std::min(I16[1], a.I16[1]),
			std::min(I16[2], a.I16[2]),
			std::min(I16[3], a.I16[3]),
			std::min(I16[4], a.I16[4]),
			std::min(I16[5], a.I16[5]),
			std::min(I16[6], a.I16[6]),
			std::min(I16[7], a.I16[7]));
	}

	__forceinline GSVector4i max_i16(const GSVector4i& a) const
	{
		return GSVector4i(
			std::max(I16[0], a.I16[0]),
			std::max(I16[1], a.I16[1]),
			std::max(I16[2], a.I16[2]),
			std::max(I16[3], a.I16[3]),
			std::max(I16[4], a.I16[4]),
			std::max(I16[5], a.I16[5]),
			std::max(I16[6], a.I16[6]),
			std::max(I16[7], a.I16[7]));
	}

	__forceinline GSVector4i min_i32(const GSVector4i& a) const
	{
		return GSVector4i(
			std::min(I32[0], a.I32[0]),
			std::min(I32[1], a.I32[1]),
			std::min(I32[2], a.I32[2]),
			std::min(I32[3], a.I32[3]));
	}

	__forceinline GSVector4i max_i32(const GSVector4i& a) const
	{
		return GSVector4i(
			std::max(I32[0], a.I32[0]),
			std::max(I32[1], a.I32[1]),
			std::max(I32[2], a.I32[2]),
			std::max(I32[3], a.I32[3]));
	}

	__forceinline GSVector4i min_u8(const GSVector4i& a) const
	{
		return GSVector4i(
			std::min(U8[0], a.U8[0]),
			std::min(U8[1], a.U8[1]),
			std::min(U8[2], a.U8[2]),
			std::min(U8[3], a.U8[3]),
			std::min(U8[4], a.U8[4]),
			std::min(U8[5], a.U8[5]),
			std::min(U8[6], a.U8[6]),
			std::min(U8[7], a.U8[7]),
			std::min(U8[8], a.U8[8]),
			std::min(U8[9], a.U8[9]),
			std::min(U8[10], a.U8[10]),
			std::min(U8[11], a.U8[11]),
			std::min(U8[12], a.U8[12]),
			std::min(U8[13], a.U8[13]),
			std::min(U8[14], a.U8[14]),
			std::min(U8[15], a.U8[15]));
	}

	__forceinline GSVector4i max_u8(const GSVector4i& a) const
	{
		return GSVector4i(
			std::max(U8[0], a.U8[0]),
			std::max(U8[1], a.U8[1]),
			std::max(U8[2], a.U8[2]),
			std::max(U8[3], a.U8[3]),
			std::max(U8[4], a.U8[4]),
			std::max(U8[5], a.U8[5]),
			std::max(U8[6], a.U8[6]),
			std::max(U8[7], a.U8[7]),
			std::max(U8[8], a.U8[8]),
			std::max(U8[9], a.U8[9]),
			std::max(U8[10], a.U8[10]),
			std::max(U8[11], a.U8[11]),
			std::max(U8[12], a.U8[12]),
			std::max(U8[13], a.U8[13]),
			std::max(U8[14], a.U8[14]),
			std::max(U8[15], a.U8[15]));
	}

	__forceinline GSVector4i min_u16(const GSVector4i& a) const
	{
		return GSVector4i(
			std::min(U16[0], a.U16[0]),
			std::min(U16[1], a.U16[1]),
			std::min(U16[2], a.U16[2]),
			std::min(U16[3], a.U16[3]),
			std::min(U16[4], a.U16[4]),
			std::min(U16[5], a.U16[5]),
			std::min(U16[6], a.U16[6]),
			std::min(U16[7], a.U16[7]));
	}

	__forceinline GSVector4i max_u16(const GSVector4i& a) const
	{
		return GSVector4i(
			std::max(U16[0], a.U16[0]),
			std::max(U16[1], a.U16[1]),
			std::max(U16[2], a.U16[2]),
			std::max(U16[3], a.U16[3]),
			std::max(U16[4], a.U16[4]),
			std::max(U16[5], a.U16[5]),
			std::max(U16[6], a.U16[6]),
			std::max(U16[7], a.U16[7]));
	}

	__forceinline GSVector4i min_u32(const GSVector4i& a) const
	{
		return GSVector4i(
			std::min(U32[0], a.U32[0]),
			std::min(U32[1], a.U32[1]),
			std::min(U32[2], a.U32[2]),
			std::min(U32[3], a.U32[3]));
	}

	__forceinline GSVector4i max_u32(const GSVector4i& a) const
	{
		return GSVector4i(
			std::max(U32[0], a.U32[0]),
			std::max(U32[1], a.U32[1]),
			std::max(U32[2], a.U32[2]),
			std::max(U32[3], a.U32[3]));
	}

	__forceinline s32 minv_s32() const
	{
		return std::min({I32[0], I32[1], I32[2], I32[3]});
	}

	__forceinline u32 minv_u32() const
	{
		return std::min({U32[0], U32[1], U32[2], U32[3]});
	}

	__forceinline u32 maxv_s32() const
	{
		return std::max({I32[0], I32[1], I32[2], I32[3]});
	}

	__forceinline u32 maxv_u32() const
	{
		return std::max({U32[0], U32[1], U32[2], U32[3]});
	}

	__forceinline static int min_i16(int a, int b)
	{
		return store(load(a).min_i16(load(b)));
	}

	__forceinline GSVector4i clamp8() const
	{
		return pu16().upl8();
	}

	__forceinline GSVector4i blend8(const GSVector4i& a, const GSVector4i& mask) const
	{
		u8 ret[16];

		for (int i = 0; i < 16; i++)
		{
			u8 maskB = mask.I8[i] >> 7;
			ret[i] = U8[i] ^ ((U8[i] ^ a.U8[i]) & maskB);
		}
		return GSVector4i(ret[0], ret[1], ret[2], ret[3], ret[4], ret[5], ret[6], ret[7], ret[8], ret[9], ret[10], ret[11], ret[12], ret[13], ret[14], ret[15]);
	}

	template <int mask>
	__forceinline GSVector4i blend16(const GSVector4i& a) const
	{
		const uint16_t _mask[8] = {((mask) & (1 << 0)) ? (uint16_t)-1 : 0x0,
			((mask) & (1 << 1)) ? (uint16_t)-1 : 0x0,
			((mask) & (1 << 2)) ? (uint16_t)-1 : 0x0,
			((mask) & (1 << 3)) ? (uint16_t)-1 : 0x0,
			((mask) & (1 << 4)) ? (uint16_t)-1 : 0x0,
			((mask) & (1 << 5)) ? (uint16_t)-1 : 0x0,
			((mask) & (1 << 6)) ? (uint16_t)-1 : 0x0,
			((mask) & (1 << 7)) ? (uint16_t)-1 : 0x0};

		u16 ret[16];

		for (int i = 0; i < 8; i++)
		{
			ret[i] = U16[i] ^ ((U16[i] ^ a.U16[i]) & _mask[i]);
		}
		return GSVector4i(ret[0], ret[1], ret[2], ret[3], ret[4], ret[5], ret[6], ret[7]);
	}

	template <int mask>
	__forceinline GSVector4i blend32(const GSVector4i& v) const
	{
		constexpr int bit3 = ((mask & 8) * 3) << 3;
		constexpr int bit2 = ((mask & 4) * 3) << 2;
		constexpr int bit1 = ((mask & 2) * 3) << 1;
		constexpr int bit0 = (mask & 1) * 3;
		return blend16<bit3 | bit2 | bit1 | bit0>(v);
	}

	/// Equivalent to blend with the given mask broadcasted across the vector
	/// May be faster than blend in some cases
	template <u32 mask>
	__forceinline GSVector4i smartblend(const GSVector4i& a) const
	{
		if (mask == 0)
			return *this;
		if (mask == 0xffffffff)
			return a;

		if (mask == 0x0000ffff)
			return blend16<0x55>(a);
		if (mask == 0xffff0000)
			return blend16<0xaa>(a);

		for (int i = 0; i < 32; i += 8)
		{
			u8 byte = (mask >> i) & 0xff;
			if (byte != 0xff && byte != 0)
				return blend(a, GSVector4i(mask));
		}

		return blend8(a, GSVector4i(mask));
	}

	__forceinline GSVector4i blend(const GSVector4i& a, const GSVector4i& mask) const
	{
		s8 ret[16];

		for (int i = 0; i < 16; i++)
		{
			ret[i] = (I8[i] & ~mask.I8[i]) | (mask.I8[i] & a.I8[i]);
		}
		return GSVector4i(ret[0], ret[1], ret[2], ret[3], ret[4], ret[5], ret[6], ret[7], ret[8], ret[9], ret[10], ret[11], ret[12], ret[13], ret[14], ret[15]);
	}

	__forceinline GSVector4i mix16(const GSVector4i& a) const
	{
		return blend16<0xaa>(a);
	}

	__forceinline GSVector4i shuffle8(const GSVector4i& mask) const
	{
		s8 ret[16];

		for (int i = 0; i < 16; i++)
		{
			pxAssert(mask.U8[i] < 16);
			ret[i] = I8[mask.U8[i]];
		}
		return GSVector4i(ret[0], ret[1], ret[2], ret[3], ret[4], ret[5], ret[6], ret[7], ret[8], ret[9], ret[10], ret[11], ret[12], ret[13], ret[14], ret[15]);
	}

	__forceinline GSVector4i ps16(const GSVector4i& a) const
	{
		s8 a1[8];
		s8 a2[8];
		for (int i = 0; i < 8; i++)
		{
			a1[i] = std::clamp<s16>(I16[i], INT8_MIN, INT8_MAX);
			a2[i] = std::clamp<s16>(a.I16[i], INT8_MIN, INT8_MAX);
		}

		return GSVector4i(a1[0], a1[1], a1[2], a1[3], a1[4], a1[5], a1[6], a1[7], a2[0], a2[1], a2[2], a2[3], a2[4], a2[5], a2[6], a2[7]);
	}

	__forceinline GSVector4i ps16() const
	{
		s8 a1[8];
		for (int i = 0; i < 8; i++)
		{
			a1[i] = std::clamp<s16>(I16[i], INT8_MIN, INT8_MAX);
		}

		return GSVector4i(a1[0], a1[1], a1[2], a1[3], a1[4], a1[5], a1[6], a1[7], a1[0], a1[1], a1[2], a1[3], a1[4], a1[5], a1[6], a1[7]);
	}

	__forceinline GSVector4i pu16(const GSVector4i& a) const
	{
		u8 a1[8];
		u8 a2[8];
		for (int i = 0; i < 8; i++)
		{
			a1[i] = std::clamp<u16>(U16[i], 0, UINT8_MAX);
			a2[i] = std::clamp<u16>(a.U16[i], 0, UINT8_MAX);
		}

		return GSVector4i(a1[0], a1[1], a1[2], a1[3], a1[4], a1[5], a1[6], a1[7], a2[0], a2[1], a2[2], a2[3], a2[4], a2[5], a2[6], a2[7]);
	}

	__forceinline GSVector4i pu16() const
	{
		u8 a1[8];
		for (int i = 0; i < 8; i++)
		{
			a1[i] = std::clamp<u16>(U16[i], 0, UINT8_MAX);
		}

		return GSVector4i(a1[0], a1[1], a1[2], a1[3], a1[4], a1[5], a1[6], a1[7], a1[0], a1[1], a1[2], a1[3], a1[4], a1[5], a1[6], a1[7]);
	}

	__forceinline GSVector4i ps32(const GSVector4i& a) const
	{
		s16 a1[4];
		s16 a2[4];
		for (int i = 0; i < 4; i++)
		{
			a1[i] = std::clamp<s32>(I32[i], INT16_MIN, INT16_MAX);
			a2[i] = std::clamp<s32>(a.I32[i], INT16_MIN, INT16_MAX);
		}

		return GSVector4i(a1[0], a1[1], a1[2], a1[3], a2[0], a2[1], a2[2], a2[3]);
	}

	__forceinline GSVector4i ps32() const
	{
		u16 a1[4];
		for (int i = 0; i < 4; i++)
		{
			a1[i] = std::clamp<s32>(I32[i], INT16_MIN, INT16_MAX);
		}

		return GSVector4i(a1[0], a1[1], a1[2], a1[3], a1[0], a1[1], a1[2], a1[3]);
	}

	__forceinline GSVector4i pu32(const GSVector4i& a) const
	{
		u16 a1[4];
		u16 a2[4];
		for (int i = 0; i < 4; i++)
		{
			a1[i] = std::clamp<u32>(U32[i], 0, UINT16_MAX);
			a2[i] = std::clamp<u32>(a.U32[i], 0, UINT16_MAX);
		}

		return GSVector4i(a1[0], a1[1], a1[2], a1[3], a2[0], a2[1], a2[2], a2[3]);
	}

	__forceinline GSVector4i pu32() const
	{
		u16 a1[4];
		for (int i = 0; i < 4; i++)
		{
			a1[i] = std::clamp<u32>(U32[i], 0, UINT16_MAX);
		}

		return GSVector4i(a1[0], a1[1], a1[2], a1[3], a1[0], a1[1], a1[2], a1[3]);
	}

	__forceinline GSVector4i upl8(const GSVector4i& a) const
	{
		return GSVector4i(I8[0], a.I8[0], I8[1], a.I8[1], I8[2], a.I8[2], I8[3], a.I8[3], I8[4], a.I8[4], I8[5], a.I8[5], I8[6], a.I8[6], I8[7], a.I8[7]);
	}

	__forceinline GSVector4i uph8(const GSVector4i& a) const
	{
		return GSVector4i(I8[8], a.I8[8], I8[9], a.I8[9], I8[10], a.I8[10], I8[11], a.I8[11], I8[12], a.I8[12], I8[13], a.I8[13], I8[14], a.I8[14], I8[15], a.I8[15]);
	}

	__forceinline GSVector4i upl16(const GSVector4i& a) const
	{
		return GSVector4i(I16[0], a.I16[0], I16[1], a.I16[1], I16[2], a.I16[2], I16[3], a.I16[3]);
	}

	__forceinline GSVector4i uph16(const GSVector4i& a) const
	{
		return GSVector4i(I16[4], a.I16[4], I16[5], a.I16[5], I16[6], a.I16[6], I16[7], a.I16[7]);
	}

	__forceinline GSVector4i upl32(const GSVector4i& a) const
	{
		return GSVector4i(I32[0], a.I32[0], I32[1], a.I32[1]);
	}

	__forceinline GSVector4i uph32(const GSVector4i& a) const
	{
		return GSVector4i(I32[2], a.I32[2], I32[3], a.I32[3]);
	}

	__forceinline GSVector4i upl64(const GSVector4i& a) const
	{
		return GSVector4i(I32[0], I32[1], a.I32[0], a.I32[1]);
	}

	__forceinline GSVector4i uph64(const GSVector4i& a) const
	{
		return GSVector4i(I32[2], I32[3], a.I32[2], a.I32[3]);
	}

	__forceinline GSVector4i upl8() const
	{
		return GSVector4i(I8[0], 0, I8[1], 0, I8[2], 0, I8[3], 0, I8[4], 0, I8[5], 0, I8[6], 0, I8[7], 0);
	}

	__forceinline GSVector4i uph8() const
	{
		return GSVector4i(I8[8], 0, I8[9], 0, I8[10], 0, I8[11], 0, I8[12], 0, I8[13], 0, I8[14], 0, I8[15], 0);
	}

	__forceinline GSVector4i upl16() const
	{
		return GSVector4i(I16[0], 0, I16[1], 0, I16[2], 0, I16[3], 0);
	}

	__forceinline GSVector4i uph16() const
	{
		return GSVector4i(I16[4], 0, I16[5], 0, I16[6], 0, I16[7], 0);
	}

	__forceinline GSVector4i upl32() const
	{
		return GSVector4i(I32[0], 0, I32[1], 0);
	}

	__forceinline GSVector4i uph32() const
	{
		return GSVector4i(I32[1], 0, I32[2], 0);
	}

	__forceinline GSVector4i upl64() const
	{
		return GSVector4i(I32[0], I32[1], 0, 0);
	}

	__forceinline GSVector4i uph64() const
	{
		return GSVector4i(I32[2], I32[3], 0, 0);
	}

	__forceinline GSVector4i i8to16() const
	{
		return GSVector4i(I8[0], I8[1], I8[2], I8[3], I8[4], I8[5], I8[6], I8[7]);
	}

	__forceinline GSVector4i u8to16() const
	{
		return GSVector4i(U8[0], U8[1], U8[2], U8[3], U8[4], U8[5], U8[6], U8[7]);
	}

	__forceinline GSVector4i i8to32() const
	{
		return GSVector4i(I8[0], I8[1], I8[2], I8[3]);
	}

	__forceinline GSVector4i u8to32() const
	{
		return GSVector4i(U8[0], U8[1], U8[2], U8[3]);
	}

	__forceinline GSVector4i i8to64() const
	{
		return GSVector4i(I8[0], 0, I8[1], 0);
	}

	__forceinline GSVector4i u8to64() const
	{
		return GSVector4i(I8[0], 0, I8[1], 0);
	}

	__forceinline GSVector4i i16to32() const
	{
		return GSVector4i(I16[0], I16[1], I16[2], I16[3]);
	}

	__forceinline GSVector4i u16to32() const
	{
		return GSVector4i(U16[0], U16[1], U16[2], U16[3]);
	}

	__forceinline GSVector4i i16to64() const
	{
		return GSVector4i(I16[0], 0, I16[1], 0);
	}

	__forceinline GSVector4i u16to64() const
	{
		return GSVector4i(U16[0], 0, U16[1], 0);
	}

	__forceinline GSVector4i i32to64() const
	{
		return GSVector4i(I32[0], 0, I32[1], 0);
	}

	__forceinline GSVector4i u32to64() const
	{
		return GSVector4i(U32[0], 0, U32[1], 0);
	}

	template <int i>
	__forceinline GSVector4i srl() const
	{
		// For some reason, s8 (type) and s8 (arm reg) will conflict due to GS\Renderers\SW\GSSetupPrimCodeGenerator.arm64.cpp ???
		::s8 ret[16]{};
		std::memcpy(ret, &I8[i], 16 - i);
		return GSVector4i(ret[0], ret[1], ret[2], ret[3], ret[4], ret[5], ret[6], ret[7], ret[8], ret[9], ret[10], ret[11], ret[12], ret[13], ret[14], ret[15]);
	}

	template <int i>
	__forceinline GSVector4i srl(const GSVector4i& v)
	{
		s8 ret[16]{}; // Zero init
		if (i < 16)
		{
			std::memcpy(ret, &I8[i], 16 - i);
			std::memcpy(&ret[16 - i], &v.I8[0], 16 - (16 - i)); 
		}
		else
		{
			std::memcpy(ret, &v.I8[i], 16 - i);
		}

		return GSVector4i(ret[0], ret[1], ret[2], ret[3], ret[4], ret[5], ret[6], ret[7], ret[8], ret[9], ret[10], ret[11], ret[12], ret[13], ret[14], ret[15]);
	}

	template <int i>
	__forceinline GSVector4i sll() const
	{
		s8 ret[16]{};
		std::memcpy(&ret[i], &I8[0], 16 - i);
		return GSVector4i(ret[0], ret[1], ret[2], ret[3], ret[4], ret[5], ret[6], ret[7], ret[8], ret[9], ret[10], ret[11], ret[12], ret[13], ret[14], ret[15]);
	}

	template <s32 i>
	__forceinline GSVector4i sll16() const
	{
		return GSVector4i(
			U16[0] << i,
			U16[1] << i,
			U16[2] << i,
			U16[3] << i,
			U16[4] << i,
			U16[5] << i,
			U16[6] << i,
			U16[7] << i);
	}

	__forceinline GSVector4i sll16(s32 i) const
	{
		return GSVector4i(
			U16[0] << i,
			U16[1] << i,
			U16[2] << i,
			U16[3] << i,
			U16[4] << i,
			U16[5] << i,
			U16[6] << i,
			U16[7] << i);
	}

	__forceinline GSVector4i sllv16(const GSVector4i& v) const
	{
		return GSVector4i(
			U16[0] << v.U16[0],
			U16[1] << v.U16[1],
			U16[2] << v.U16[2],
			U16[3] << v.U16[3],
			U16[4] << v.U16[4],
			U16[5] << v.U16[5],
			U16[6] << v.U16[6],
			U16[7] << v.U16[7]);
	}

	template <s32 i>
	__forceinline GSVector4i srl16() const
	{
		return GSVector4i(
			U16[0] >> i,
			U16[1] >> i,
			U16[2] >> i,
			U16[3] >> i,
			U16[4] >> i,
			U16[5] >> i,
			U16[6] >> i,
			U16[7] >> i);
	}

	__forceinline GSVector4i srl16(s32 i) const
	{
		return GSVector4i(
			U16[0] >> i,
			U16[1] >> i,
			U16[2] >> i,
			U16[3] >> i,
			U16[4] >> i,
			U16[5] >> i,
			U16[6] >> i,
			U16[7] >> i);
	}

	__forceinline GSVector4i srlv16(const GSVector4i& v) const
	{
		return GSVector4i(
			U16[0] >> v.U16[0],
			U16[1] >> v.U16[1],
			U16[2] >> v.U16[2],
			U16[3] >> v.U16[3],
			U16[4] >> v.U16[4],
			U16[5] >> v.U16[5],
			U16[6] >> v.U16[6],
			U16[7] >> v.U16[7]);
	}

	template <s32 i>
	__forceinline GSVector4i sra16() const
	{
		return GSVector4i(
			I16[0] >> i,
			I16[1] >> i,
			I16[2] >> i,
			I16[3] >> i,
			I16[4] >> i,
			I16[5] >> i,
			I16[6] >> i,
			I16[7] >> i);
	}

	__forceinline GSVector4i sra16(s32 i) const
	{
		return GSVector4i(
			I16[0] >> i,
			I16[1] >> i,
			I16[2] >> i,
			I16[3] >> i,
			I16[4] >> i,
			I16[5] >> i,
			I16[6] >> i,
			I16[7] >> i);
	}

	__forceinline GSVector4i srav16(const GSVector4i& v) const
	{
		return GSVector4i(
			I16[0] >> v.U16[0],
			I16[1] >> v.U16[1],
			I16[2] >> v.U16[2],
			I16[3] >> v.U16[3],
			I16[4] >> v.U16[4],
			I16[5] >> v.U16[5],
			I16[6] >> v.U16[6],
			I16[7] >> v.U16[7]);
	}

	template <s32 i>
	__forceinline GSVector4i sll32() const
	{
		return GSVector4i(
			U32[0] << i,
			U32[1] << i,
			U32[2] << i,
			U32[3] << i);
	}

	__forceinline GSVector4i sll32(s32 i) const
	{
		return GSVector4i(
			U32[0] << i,
			U32[1] << i,
			U32[2] << i,
			U32[3] << i);
	}

	__forceinline GSVector4i sllv32(const GSVector4i& v) const
	{
		return GSVector4i(
			U32[0] << v.U32[0],
			U32[1] << v.U32[1],
			U32[2] << v.U32[2],
			U32[3] << v.U32[3]);
	}

	template <s32 i>
	__forceinline GSVector4i srl32() const
	{
		return GSVector4i(
			U32[0] >> i,
			U32[1] >> i,
			U32[2] >> i,
			U32[3] >> i);
	}

	__forceinline GSVector4i srl32(s32 i) const
	{
		return GSVector4i(
			U32[0] >> i,
			U32[1] >> i,
			U32[2] >> i,
			U32[3] >> i);
	}

	__forceinline GSVector4i srlv32(const GSVector4i& v) const
	{
		return GSVector4i(
			U32[0] >> v.U32[0],
			U32[1] >> v.U32[1],
			U32[2] >> v.U32[2],
			U32[3] >> v.U32[3]);
	}

	template <s32 i>
	__forceinline GSVector4i sra32() const
	{
		return GSVector4i(
			I32[0] >> i,
			I32[1] >> i,
			I32[2] >> i,
			I32[3] >> i);
	}

	__forceinline GSVector4i sra32(s32 i) const
	{
		return GSVector4i(
			I32[0] >> i,
			I32[1] >> i,
			I32[2] >> i,
			I32[3] >> i);
	}

	__forceinline GSVector4i srav32(const GSVector4i& v) const
	{
		return GSVector4i(
			I32[0] >> v.U32[0],
			I32[1] >> v.U32[1],
			I32[2] >> v.U32[2],
			I32[3] >> v.U32[3]);
	}

	template <s64 i>
	__forceinline GSVector4i sll64() const
	{
		u64 ret[2]{U64[0] << i, U64[1] << i};
		return GSVector4i(((u32*)ret)[0], ((u32*)ret)[1], ((u32*)ret)[2], ((u32*)ret)[3]);
	}

	__forceinline GSVector4i sll64(s32 i) const
	{
		u64 ret[2]{U64[0] << i, U64[1] << i};
		return GSVector4i(((u32*)ret)[0], ((u32*)ret)[1], ((u32*)ret)[2], ((u32*)ret)[3]);
	}

	__forceinline GSVector4i sllv64(const GSVector4i& v) const
	{
		u64 ret[2]{U64[0] << v.U64[0], U64[1] << v.U64[0]};
		return GSVector4i(((u32*)ret)[0], ((u32*)ret)[1], ((u32*)ret)[2], ((u32*)ret)[3]);
	}

	template <s64 i>
	__forceinline GSVector4i srl64() const
	{
		u64 ret[2]{U64[0] >> i, U64[1] >> i};
		return GSVector4i(((u32*)ret)[0], ((u32*)ret)[1], ((u32*)ret)[2], ((u32*)ret)[3]);
	}

	__forceinline GSVector4i srl64(s32 i) const
	{
		u64 ret[2]{U64[0] >> i, U64[1] >> i};
		return GSVector4i(((u32*)ret)[0], ((u32*)ret)[1], ((u32*)ret)[2], ((u32*)ret)[3]);
	}

	__forceinline GSVector4i srlv64(const GSVector4i& v) const
	{
		u64 ret[2]{U64[0] >> v.U64[0], U64[1] >> v.U64[0]};
		return GSVector4i(((u32*)ret)[0], ((u32*)ret)[1], ((u32*)ret)[2], ((u32*)ret)[3]);
	}

	__forceinline GSVector4i sra64(s32 i) const
	{
		s64 ret[2]{I64[0] >> i, I64[1] >> i};
		return GSVector4i(((s32*)ret)[0], ((s32*)ret)[1], ((s32*)ret)[2], ((s32*)ret)[3]);
	}

	__forceinline GSVector4i srav64(const GSVector4i& v) const
	{
		s64 ret[2]{I64[0] >> v.U64[0], I64[1] >> v.U64[0]};
		return GSVector4i(((s32*)ret)[0], ((s32*)ret)[1], ((s32*)ret)[2], ((s32*)ret)[3]);
	}

	template <int i>
	__forceinline GSVector4i srl64() const
	{
		u64 ret[2]{U64[0] >> i, U64[1] >> i};
		return GSVector4i(((u32*)ret)[0], ((u32*)ret)[1], ((u32*)ret)[2], ((u32*)ret)[3]);
	}

	__forceinline GSVector4i add8(const GSVector4i& v) const
	{
		return GSVector4i(
			I8[0] + v.I8[0],
			I8[1] + v.I8[1],
			I8[2] + v.I8[2],
			I8[3] + v.I8[3],
			I8[4] + v.I8[4],
			I8[5] + v.I8[5],
			I8[6] + v.I8[6],
			I8[7] + v.I8[7],
			I8[8] + v.I8[8],
			I8[9] + v.I8[9],
			I8[10] + v.I8[10],
			I8[11] + v.I8[11],
			I8[12] + v.I8[12],
			I8[13] + v.I8[13],
			I8[14] + v.I8[14],
			I8[15] + v.I8[15]);
	}

	__forceinline GSVector4i add16(const GSVector4i& v) const
	{
		return GSVector4i(
			I16[0] + v.I16[0],
			I16[1] + v.I16[1],
			I16[2] + v.I16[2],
			I16[3] + v.I16[3],
			I16[4] + v.I16[4],
			I16[5] + v.I16[5],
			I16[6] + v.I16[6],
			I16[7] + v.I16[7]);
	}

	__forceinline GSVector4i add32(const GSVector4i& v) const
	{
		return GSVector4i(
			I32[0] + v.I32[0],
			I32[1] + v.I32[1],
			I32[2] + v.I32[2],
			I32[3] + v.I32[3]);
	}

	__forceinline GSVector4i adds8(const GSVector4i& v) const
	{
		return GSVector4i(
			std::clamp<s16>(I8[0] + static_cast<s16>(v.I8[0]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[1] + static_cast<s16>(v.I8[1]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[2] + static_cast<s16>(v.I8[2]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[3] + static_cast<s16>(v.I8[3]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[4] + static_cast<s16>(v.I8[4]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[5] + static_cast<s16>(v.I8[5]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[6] + static_cast<s16>(v.I8[6]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[7] + static_cast<s16>(v.I8[7]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[8] + static_cast<s16>(v.I8[8]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[9] + static_cast<s16>(v.I8[9]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[10] + static_cast<s16>(v.I8[10]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[11] + static_cast<s16>(v.I8[11]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[12] + static_cast<s16>(v.I8[12]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[13] + static_cast<s16>(v.I8[13]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[14] + static_cast<s16>(v.I8[14]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[15] + static_cast<s16>(v.I8[15]), INT8_MIN, INT8_MAX));
	}

	__forceinline GSVector4i adds16(const GSVector4i& v) const
	{
		return GSVector4i(
			std::clamp<s32>(I16[0] + static_cast<s32>(v.I16[0]), INT16_MIN, INT16_MAX),
			std::clamp<s32>(I16[1] + static_cast<s32>(v.I16[1]), INT16_MIN, INT16_MAX),
			std::clamp<s32>(I16[2] + static_cast<s32>(v.I16[2]), INT16_MIN, INT16_MAX),
			std::clamp<s32>(I16[3] + static_cast<s32>(v.I16[3]), INT16_MIN, INT16_MAX),
			std::clamp<s32>(I16[4] + static_cast<s32>(v.I16[4]), INT16_MIN, INT16_MAX),
			std::clamp<s32>(I16[5] + static_cast<s32>(v.I16[5]), INT16_MIN, INT16_MAX),
			std::clamp<s32>(I16[6] + static_cast<s32>(v.I16[6]), INT16_MIN, INT16_MAX),
			std::clamp<s32>(I16[7] + static_cast<s32>(v.I16[7]), INT16_MIN, INT16_MAX));
	}

	__forceinline GSVector4i hadds16(const GSVector4i& v) const
	{
		return GSVector4i(
			std::clamp<s32>(I16[0] + static_cast<s32>(I16[1]), INT16_MIN, INT16_MAX),
			std::clamp<s32>(I16[2] + static_cast<s32>(I16[3]), INT16_MIN, INT16_MAX),
			std::clamp<s32>(I16[4] + static_cast<s32>(I16[5]), INT16_MIN, INT16_MAX),
			std::clamp<s32>(I16[6] + static_cast<s32>(I16[7]), INT16_MIN, INT16_MAX),
			std::clamp<s32>(v.I16[0] + static_cast<s32>(v.I16[1]), INT16_MIN, INT16_MAX),
			std::clamp<s32>(v.I16[2] + static_cast<s32>(v.I16[3]), INT16_MIN, INT16_MAX),
			std::clamp<s32>(v.I16[4] + static_cast<s32>(v.I16[5]), INT16_MIN, INT16_MAX),
			std::clamp<s32>(v.I16[6] + static_cast<s32>(v.I16[7]), INT16_MIN, INT16_MAX));
	}

	__forceinline GSVector4i addus8(const GSVector4i& v) const
	{
		return GSVector4i(
			std::clamp<u16>(U8[0] + static_cast<u16>(v.U8[0]), 0, UINT8_MAX),
			std::clamp<u16>(U8[1] + static_cast<u16>(v.U8[1]), 0, UINT8_MAX),
			std::clamp<u16>(U8[2] + static_cast<u16>(v.U8[2]), 0, UINT8_MAX),
			std::clamp<u16>(U8[3] + static_cast<u16>(v.U8[3]), 0, UINT8_MAX),
			std::clamp<u16>(U8[4] + static_cast<u16>(v.U8[4]), 0, UINT8_MAX),
			std::clamp<u16>(U8[5] + static_cast<u16>(v.U8[5]), 0, UINT8_MAX),
			std::clamp<u16>(U8[6] + static_cast<u16>(v.U8[6]), 0, UINT8_MAX),
			std::clamp<u16>(U8[7] + static_cast<u16>(v.U8[7]), 0, UINT8_MAX),
			std::clamp<u16>(U8[8] + static_cast<u16>(v.U8[8]), 0, UINT8_MAX),
			std::clamp<u16>(U8[9] + static_cast<u16>(v.U8[9]), 0, UINT8_MAX),
			std::clamp<u16>(U8[10] + static_cast<u16>(v.U8[10]), 0, UINT8_MAX),
			std::clamp<u16>(U8[11] + static_cast<u16>(v.U8[11]), 0, UINT8_MAX),
			std::clamp<u16>(U8[12] + static_cast<u16>(v.U8[12]), 0, UINT8_MAX),
			std::clamp<u16>(U8[13] + static_cast<u16>(v.U8[13]), 0, UINT8_MAX),
			std::clamp<u16>(U8[14] + static_cast<u16>(v.U8[14]), 0, UINT8_MAX),
			std::clamp<u16>(U8[15] + static_cast<u16>(v.U8[15]), 0, UINT8_MAX));
	}

	__forceinline GSVector4i addus16(const GSVector4i& v) const
	{
		return GSVector4i(
			std::clamp<u32>(U16[0] + static_cast<u32>(v.U16[0]), 0, UINT16_MAX),
			std::clamp<u32>(U16[1] + static_cast<u32>(v.U16[1]), 0, UINT16_MAX),
			std::clamp<u32>(U16[2] + static_cast<u32>(v.U16[2]), 0, UINT16_MAX),
			std::clamp<u32>(U16[3] + static_cast<u32>(v.U16[3]), 0, UINT16_MAX),
			std::clamp<u32>(U16[4] + static_cast<u32>(v.U16[4]), 0, UINT16_MAX),
			std::clamp<u32>(U16[5] + static_cast<u32>(v.U16[5]), 0, UINT16_MAX),
			std::clamp<u32>(U16[6] + static_cast<u32>(v.U16[6]), 0, UINT16_MAX),
			std::clamp<u32>(U16[7] + static_cast<u32>(v.U16[7]), 0, UINT16_MAX));
	}

	__forceinline GSVector4i sub8(const GSVector4i& v) const
	{
		return GSVector4i(
			I8[0] - v.I8[0],
			I8[1] - v.I8[1],
			I8[2] - v.I8[2],
			I8[3] - v.I8[3],
			I8[4] - v.I8[4],
			I8[5] - v.I8[5],
			I8[6] - v.I8[6],
			I8[7] - v.I8[7],
			I8[8] - v.I8[8],
			I8[9] - v.I8[9],
			I8[10] - v.I8[10],
			I8[11] - v.I8[11],
			I8[12] - v.I8[12],
			I8[13] - v.I8[13],
			I8[14] - v.I8[14],
			I8[15] - v.I8[15]);
	}

	__forceinline GSVector4i sub16(const GSVector4i& v) const
	{
		return GSVector4i(
			I16[0] - v.I16[0],
			I16[1] - v.I16[1],
			I16[2] - v.I16[2],
			I16[3] - v.I16[3],
			I16[4] - v.I16[4],
			I16[5] - v.I16[5],
			I16[6] - v.I16[6],
			I16[7] - v.I16[7]);
	}

	__forceinline GSVector4i sub32(const GSVector4i& v) const
	{
		return GSVector4i(
			I32[0] - v.I32[0],
			I32[1] - v.I32[1],
			I32[2] - v.I32[2],
			I32[3] - v.I32[3]);
	}

	__forceinline GSVector4i subs8(const GSVector4i& v) const
	{
		return GSVector4i(
			std::clamp<s16>(I8[0] - static_cast<s16>(v.I8[0]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[1] - static_cast<s16>(v.I8[1]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[2] - static_cast<s16>(v.I8[2]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[3] - static_cast<s16>(v.I8[3]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[4] - static_cast<s16>(v.I8[4]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[5] - static_cast<s16>(v.I8[5]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[6] - static_cast<s16>(v.I8[6]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[7] - static_cast<s16>(v.I8[7]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[8] - static_cast<s16>(v.I8[8]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[9] - static_cast<s16>(v.I8[9]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[10] - static_cast<s16>(v.I8[10]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[11] - static_cast<s16>(v.I8[11]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[12] - static_cast<s16>(v.I8[12]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[13] - static_cast<s16>(v.I8[13]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[14] - static_cast<s16>(v.I8[14]), INT8_MIN, INT8_MAX),
			std::clamp<s16>(I8[15] - static_cast<s16>(v.I8[15]), INT8_MIN, INT8_MAX));
	}

	__forceinline GSVector4i subs16(const GSVector4i& v) const
	{
		return GSVector4i(
			std::clamp<s32>(I16[0] - static_cast<s32>(v.I16[0]), INT16_MIN, INT16_MAX),
			std::clamp<s32>(I16[1] - static_cast<s32>(v.I16[1]), INT16_MIN, INT16_MAX),
			std::clamp<s32>(I16[2] - static_cast<s32>(v.I16[2]), INT16_MIN, INT16_MAX),
			std::clamp<s32>(I16[3] - static_cast<s32>(v.I16[3]), INT16_MIN, INT16_MAX),
			std::clamp<s32>(I16[4] - static_cast<s32>(v.I16[4]), INT16_MIN, INT16_MAX),
			std::clamp<s32>(I16[5] - static_cast<s32>(v.I16[5]), INT16_MIN, INT16_MAX),
			std::clamp<s32>(I16[6] - static_cast<s32>(v.I16[6]), INT16_MIN, INT16_MAX),
			std::clamp<s32>(I16[7] - static_cast<s32>(v.I16[7]), INT16_MIN, INT16_MAX));
	}

	__forceinline GSVector4i subus8(const GSVector4i& v) const
	{
		return GSVector4i(
			std::clamp<s16>(U8[0] - static_cast<s16>(v.U8[0]), 0, UINT8_MAX),
			std::clamp<s16>(U8[1] - static_cast<s16>(v.U8[1]), 0, UINT8_MAX),
			std::clamp<s16>(U8[2] - static_cast<s16>(v.U8[2]), 0, UINT8_MAX),
			std::clamp<s16>(U8[3] - static_cast<s16>(v.U8[3]), 0, UINT8_MAX),
			std::clamp<s16>(U8[4] - static_cast<s16>(v.U8[4]), 0, UINT8_MAX),
			std::clamp<s16>(U8[5] - static_cast<s16>(v.U8[5]), 0, UINT8_MAX),
			std::clamp<s16>(U8[6] - static_cast<s16>(v.U8[6]), 0, UINT8_MAX),
			std::clamp<s16>(U8[7] - static_cast<s16>(v.U8[7]), 0, UINT8_MAX),
			std::clamp<s16>(U8[8] - static_cast<s16>(v.U8[8]), 0, UINT8_MAX),
			std::clamp<s16>(U8[9] - static_cast<s16>(v.U8[9]), 0, UINT8_MAX),
			std::clamp<s16>(U8[10] - static_cast<s16>(v.U8[10]), 0, UINT8_MAX),
			std::clamp<s16>(U8[11] - static_cast<s16>(v.U8[11]), 0, UINT8_MAX),
			std::clamp<s16>(U8[12] - static_cast<s16>(v.U8[12]), 0, UINT8_MAX),
			std::clamp<s16>(U8[13] - static_cast<s16>(v.U8[13]), 0, UINT8_MAX),
			std::clamp<s16>(U8[14] - static_cast<s16>(v.U8[14]), 0, UINT8_MAX),
			std::clamp<s16>(U8[15] - static_cast<s16>(v.U8[15]), 0, UINT8_MAX));
	}

	__forceinline GSVector4i subus16(const GSVector4i& v) const
	{
		return GSVector4i(
			std::clamp<s32>(U16[0] - static_cast<s32>(v.U16[0]), 0, UINT16_MAX),
			std::clamp<s32>(U16[1] - static_cast<s32>(v.U16[1]), 0, UINT16_MAX),
			std::clamp<s32>(U16[2] - static_cast<s32>(v.U16[2]), 0, UINT16_MAX),
			std::clamp<s32>(U16[3] - static_cast<s32>(v.U16[3]), 0, UINT16_MAX),
			std::clamp<s32>(U16[4] - static_cast<s32>(v.U16[4]), 0, UINT16_MAX),
			std::clamp<s32>(U16[5] - static_cast<s32>(v.U16[5]), 0, UINT16_MAX),
			std::clamp<s32>(U16[6] - static_cast<s32>(v.U16[6]), 0, UINT16_MAX),
			std::clamp<s32>(U16[7] - static_cast<s32>(v.U16[7]), 0, UINT16_MAX));
	}

	__forceinline GSVector4i avg8(const GSVector4i& v) const
	{
		return GSVector4i(
			(U8[0] + static_cast<u16>(v.U8[0])) >> 1,
			(U8[1] + static_cast<u16>(v.U8[1])) >> 1,
			(U8[2] + static_cast<u16>(v.U8[2])) >> 1,
			(U8[3] + static_cast<u16>(v.U8[3])) >> 1,
			(U8[4] + static_cast<u16>(v.U8[4])) >> 1,
			(U8[5] + static_cast<u16>(v.U8[5])) >> 1,
			(U8[6] + static_cast<u16>(v.U8[6])) >> 1,
			(U8[7] + static_cast<u16>(v.U8[7])) >> 1,
			(U8[8] + static_cast<u16>(v.U8[8])) >> 1,
			(U8[9] + static_cast<u16>(v.U8[9])) >> 1,
			(U8[10] + static_cast<u16>(v.U8[10])) >> 1,
			(U8[11] + static_cast<u16>(v.U8[11])) >> 1,
			(U8[12] + static_cast<u16>(v.U8[12])) >> 1,
			(U8[13] + static_cast<u16>(v.U8[13])) >> 1,
			(U8[14] + static_cast<u16>(v.U8[14])) >> 1,
			(U8[15] + static_cast<u16>(v.U8[15])) >> 1);
	}

	__forceinline GSVector4i avg16(const GSVector4i& v) const
	{
		return GSVector4i(
			(U16[0] + static_cast<u32>(v.U16[0])) >> 1,
			(U16[1] + static_cast<u32>(v.U16[1])) >> 1,
			(U16[2] + static_cast<u32>(v.U16[2])) >> 1,
			(U16[3] + static_cast<u32>(v.U16[3])) >> 1,
			(U16[4] + static_cast<u32>(v.U16[4])) >> 1,
			(U16[5] + static_cast<u32>(v.U16[5])) >> 1,
			(U16[6] + static_cast<u32>(v.U16[6])) >> 1,
			(U16[7] + static_cast<u32>(v.U16[7])) >> 1);
	}

	__forceinline GSVector4i mul16hs(const GSVector4i& v) const
	{
		return GSVector4i(
			(I16[0] * static_cast<s32>(v.I16[0])) >> 16,
			(I16[1] * static_cast<s32>(v.I16[1])) >> 16,
			(I16[2] * static_cast<s32>(v.I16[2])) >> 16,
			(I16[3] * static_cast<s32>(v.I16[3])) >> 16,
			(I16[4] * static_cast<s32>(v.I16[4])) >> 16,
			(I16[5] * static_cast<s32>(v.I16[5])) >> 16,
			(I16[6] * static_cast<s32>(v.I16[6])) >> 16,
			(I16[7] * static_cast<s32>(v.I16[7])) >> 16);
	}

	__forceinline GSVector4i mul16hu(const GSVector4i& v) const
	{
		return GSVector4i(
			(U16[0] * static_cast<u32>(v.U16[0])) >> 16,
			(U16[1] * static_cast<u32>(v.U16[1])) >> 16,
			(U16[2] * static_cast<u32>(v.U16[2])) >> 16,
			(U16[3] * static_cast<u32>(v.U16[3])) >> 16,
			(U16[4] * static_cast<u32>(v.U16[4])) >> 16,
			(U16[5] * static_cast<u32>(v.U16[5])) >> 16,
			(U16[6] * static_cast<u32>(v.U16[6])) >> 16,
			(U16[7] * static_cast<u32>(v.U16[7])) >> 16);
	}

	__forceinline GSVector4i mul16l(const GSVector4i& v) const
	{
		return GSVector4i(
			I16[0] * static_cast<s32>(v.I16[0]),
			I16[1] * static_cast<s32>(v.I16[1]),
			I16[2] * static_cast<s32>(v.I16[2]),
			I16[3] * static_cast<s32>(v.I16[3]),
			I16[4] * static_cast<s32>(v.I16[4]),
			I16[5] * static_cast<s32>(v.I16[5]),
			I16[6] * static_cast<s32>(v.I16[6]),
			I16[7] * static_cast<s32>(v.I16[7]));
	}

	__forceinline GSVector4i mul16hrs(const GSVector4i& v) const
	{
		return GSVector4i(
			(((I16[0] * static_cast<s32>(v.I16[0])) >> 14) + 1) >> 1,
			(((I16[1] * static_cast<s32>(v.I16[1])) >> 14) + 1) >> 1,
			(((I16[2] * static_cast<s32>(v.I16[2])) >> 14) + 1) >> 1,
			(((I16[3] * static_cast<s32>(v.I16[3])) >> 14) + 1) >> 1,
			(((I16[4] * static_cast<s32>(v.I16[4])) >> 14) + 1) >> 1,
			(((I16[5] * static_cast<s32>(v.I16[5])) >> 14) + 1) >> 1,
			(((I16[6] * static_cast<s32>(v.I16[6])) >> 14) + 1) >> 1,
			(((I16[7] * static_cast<s32>(v.I16[7])) >> 14) + 1) >> 1);
	}

	/*
	GSVector4i madd(const GSVector4i& v) const
	{
		return GSVector4i(_mm_madd_epi16(m, v.m));
	}
	*/

	template <int shift>
	__forceinline GSVector4i lerp16(const GSVector4i& a, const GSVector4i& f) const
	{
		// (a - this) * f << shift + this

		return add16(a.sub16(*this).modulate16<shift>(f));
	}

	template <int shift>
	__forceinline static GSVector4i lerp16(const GSVector4i& a, const GSVector4i& b, const GSVector4i& c)
	{
		// (a - b) * c << shift

		return a.sub16(b).modulate16<shift>(c);
	}

	template <int shift>
	__forceinline static GSVector4i lerp16(const GSVector4i& a, const GSVector4i& b, const GSVector4i& c, const GSVector4i& d)
	{
		// (a - b) * c << shift + d

		return d.add16(a.sub16(b).modulate16<shift>(c));
	}

	__forceinline GSVector4i lerp16_4(const GSVector4i& a, const GSVector4i& f) const
	{
		// (a - this) * f >> 4 + this (a, this: 8-bit, f: 4-bit)

		return add16(a.sub16(*this).mul16l(f).sra16<4>());
	}

	template <int shift>
	__forceinline GSVector4i modulate16(const GSVector4i& f) const
	{
		// a * f << shift
		if (shift == 0)
		{
			return mul16hrs(f);
		}

		return sll16<shift + 1>().mul16hs(f);
	}

	__forceinline bool eq(const GSVector4i& v) const
	{
		return (I32[0] == v.I32[0]) &&
			   (I32[1] == v.I32[1]) &&
			   (I32[2] == v.I32[2]) &&
			   (I32[3] == v.I32[3]);
	}

	__forceinline GSVector4i eq8(const GSVector4i& v) const
	{
		return GSVector4i(
			I8[0] == v.I8[0] ? 0xFF : 0,
			I8[1] == v.I8[1] ? 0xFF : 0,
			I8[2] == v.I8[2] ? 0xFF : 0,
			I8[3] == v.I8[3] ? 0xFF : 0,
			I8[4] == v.I8[4] ? 0xFF : 0,
			I8[5] == v.I8[5] ? 0xFF : 0,
			I8[6] == v.I8[6] ? 0xFF : 0,
			I8[7] == v.I8[7] ? 0xFF : 0,
			I8[8] == v.I8[8] ? 0xFF : 0,
			I8[9] == v.I8[9] ? 0xFF : 0,
			I8[10] == v.I8[10] ? 0xFF : 0,
			I8[11] == v.I8[11] ? 0xFF : 0,
			I8[12] == v.I8[12] ? 0xFF : 0,
			I8[13] == v.I8[13] ? 0xFF : 0,
			I8[14] == v.I8[14] ? 0xFF : 0,
			I8[15] == v.I8[15] ? 0xFF : 0);
	}

	__forceinline GSVector4i eq16(const GSVector4i& v) const
	{
		return GSVector4i(
			I16[0] == v.I16[0] ? 0xFFFF : 0,
			I16[1] == v.I16[1] ? 0xFFFF : 0,
			I16[2] == v.I16[2] ? 0xFFFF : 0,
			I16[3] == v.I16[3] ? 0xFFFF : 0,
			I16[4] == v.I16[4] ? 0xFFFF : 0,
			I16[5] == v.I16[5] ? 0xFFFF : 0,
			I16[6] == v.I16[6] ? 0xFFFF : 0,
			I16[7] == v.I16[7] ? 0xFFFF : 0);
	}

	__forceinline GSVector4i eq32(const GSVector4i& v) const
	{
		return GSVector4i(
			I32[0] == v.I32[0] ? 0xFFFFFFFF : 0,
			I32[1] == v.I32[1] ? 0xFFFFFFFF : 0,
			I32[2] == v.I32[2] ? 0xFFFFFFFF : 0,
			I32[3] == v.I32[3] ? 0xFFFFFFFF : 0);
	}

	__forceinline GSVector4i eq64(const GSVector4i& v) const
	{
		return GSVector4i(
			I64[0] == v.I64[0] ? 0xFFFFFFFF : 0,
			I64[0] == v.I64[0] ? 0xFFFFFFFF : 0,
			I64[1] == v.I64[1] ? 0xFFFFFFFF : 0,
			I64[1] == v.I64[1] ? 0xFFFFFFFF : 0);
	}

	__forceinline GSVector4i neq8(const GSVector4i& v) const
	{
		return ~eq8(v);
	}

	__forceinline GSVector4i neq16(const GSVector4i& v) const
	{
		return ~eq16(v);
	}

	__forceinline GSVector4i neq32(const GSVector4i& v) const
	{
		return ~eq32(v);
	}

	__forceinline GSVector4i gt8(const GSVector4i& v) const
	{
		return GSVector4i(
			I8[0] > v.I8[0] ? 0xFF : 0,
			I8[1] > v.I8[1] ? 0xFF : 0,
			I8[2] > v.I8[2] ? 0xFF : 0,
			I8[3] > v.I8[3] ? 0xFF : 0,
			I8[4] > v.I8[4] ? 0xFF : 0,
			I8[5] > v.I8[5] ? 0xFF : 0,
			I8[6] > v.I8[6] ? 0xFF : 0,
			I8[7] > v.I8[7] ? 0xFF : 0,
			I8[8] > v.I8[8] ? 0xFF : 0,
			I8[9] > v.I8[9] ? 0xFF : 0,
			I8[10] > v.I8[10] ? 0xFF : 0,
			I8[11] > v.I8[11] ? 0xFF : 0,
			I8[12] > v.I8[12] ? 0xFF : 0,
			I8[13] > v.I8[13] ? 0xFF : 0,
			I8[14] > v.I8[14] ? 0xFF : 0,
			I8[15] > v.I8[15] ? 0xFF : 0);
	}

	__forceinline GSVector4i gt16(const GSVector4i& v) const
	{
		return GSVector4i(
			I16[0] > v.I16[0] ? 0xFFFF : 0,
			I16[1] > v.I16[1] ? 0xFFFF : 0,
			I16[2] > v.I16[2] ? 0xFFFF : 0,
			I16[3] > v.I16[3] ? 0xFFFF : 0,
			I16[4] > v.I16[4] ? 0xFFFF : 0,
			I16[5] > v.I16[5] ? 0xFFFF : 0,
			I16[6] > v.I16[6] ? 0xFFFF : 0,
			I16[7] > v.I16[7] ? 0xFFFF : 0);
	}

	__forceinline GSVector4i gt32(const GSVector4i& v) const
	{
		return GSVector4i(
			I32[0] > v.I32[0] ? 0xFFFFFFFF : 0,
			I32[1] > v.I32[1] ? 0xFFFFFFFF : 0,
			I32[2] > v.I32[2] ? 0xFFFFFFFF : 0,
			I32[3] > v.I32[3] ? 0xFFFFFFFF : 0);
	}

	__forceinline GSVector4i ge8(const GSVector4i& v) const
	{
		return GSVector4i(
			I8[0] >= v.I8[0] ? 0xFF : 0,
			I8[1] >= v.I8[1] ? 0xFF : 0,
			I8[2] >= v.I8[2] ? 0xFF : 0,
			I8[3] >= v.I8[3] ? 0xFF : 0,
			I8[4] >= v.I8[4] ? 0xFF : 0,
			I8[5] >= v.I8[5] ? 0xFF : 0,
			I8[6] >= v.I8[6] ? 0xFF : 0,
			I8[7] >= v.I8[7] ? 0xFF : 0,
			I8[8] >= v.I8[8] ? 0xFF : 0,
			I8[9] >= v.I8[9] ? 0xFF : 0,
			I8[10] >= v.I8[10] ? 0xFF : 0,
			I8[11] >= v.I8[11] ? 0xFF : 0,
			I8[12] >= v.I8[12] ? 0xFF : 0,
			I8[13] >= v.I8[13] ? 0xFF : 0,
			I8[14] >= v.I8[14] ? 0xFF : 0,
			I8[15] >= v.I8[15] ? 0xFF : 0);
	}

	__forceinline GSVector4i ge16(const GSVector4i& v) const
	{
		return GSVector4i(
			I16[0] >= v.I16[0] ? 0xFFFF : 0,
			I16[1] >= v.I16[1] ? 0xFFFF : 0,
			I16[2] >= v.I16[2] ? 0xFFFF : 0,
			I16[3] >= v.I16[3] ? 0xFFFF : 0,
			I16[4] >= v.I16[4] ? 0xFFFF : 0,
			I16[5] >= v.I16[5] ? 0xFFFF : 0,
			I16[6] >= v.I16[6] ? 0xFFFF : 0,
			I16[7] >= v.I16[7] ? 0xFFFF : 0);
	}

	__forceinline GSVector4i ge32(const GSVector4i& v) const
	{
		return GSVector4i(
			I32[0] >= v.I32[0] ? 0xFFFFFFFF : 0,
			I32[1] >= v.I32[1] ? 0xFFFFFFFF : 0,
			I32[2] >= v.I32[2] ? 0xFFFFFFFF : 0,
			I32[3] >= v.I32[3] ? 0xFFFFFFFF : 0);
	}

	__forceinline GSVector4i lt8(const GSVector4i& v) const
	{
		return GSVector4i(
			I8[0] < v.I8[0] ? 0xFF : 0,
			I8[1] < v.I8[1] ? 0xFF : 0,
			I8[2] < v.I8[2] ? 0xFF : 0,
			I8[3] < v.I8[3] ? 0xFF : 0,
			I8[4] < v.I8[4] ? 0xFF : 0,
			I8[5] < v.I8[5] ? 0xFF : 0,
			I8[6] < v.I8[6] ? 0xFF : 0,
			I8[7] < v.I8[7] ? 0xFF : 0,
			I8[8] < v.I8[8] ? 0xFF : 0,
			I8[9] < v.I8[9] ? 0xFF : 0,
			I8[10] < v.I8[10] ? 0xFF : 0,
			I8[11] < v.I8[11] ? 0xFF : 0,
			I8[12] < v.I8[12] ? 0xFF : 0,
			I8[13] < v.I8[13] ? 0xFF : 0,
			I8[14] < v.I8[14] ? 0xFF : 0,
			I8[15] < v.I8[15] ? 0xFF : 0);
	}

	__forceinline GSVector4i lt16(const GSVector4i& v) const
	{
		return GSVector4i(
			I16[0] < v.I16[0] ? 0xFFFF : 0,
			I16[1] < v.I16[1] ? 0xFFFF : 0,
			I16[2] < v.I16[2] ? 0xFFFF : 0,
			I16[3] < v.I16[3] ? 0xFFFF : 0,
			I16[4] < v.I16[4] ? 0xFFFF : 0,
			I16[5] < v.I16[5] ? 0xFFFF : 0,
			I16[6] < v.I16[6] ? 0xFFFF : 0,
			I16[7] < v.I16[7] ? 0xFFFF : 0);
	}

	__forceinline GSVector4i lt32(const GSVector4i& v) const
	{
		return GSVector4i(
			I32[0] < v.I32[0] ? 0xFFFFFFFF : 0,
			I32[1] < v.I32[1] ? 0xFFFFFFFF : 0,
			I32[2] < v.I32[2] ? 0xFFFFFFFF : 0,
			I32[3] < v.I32[3] ? 0xFFFFFFFF : 0);
	}

	__forceinline GSVector4i le8(const GSVector4i& v) const
	{
		return GSVector4i(
			I8[0] <= v.I8[0] ? 0xFF : 0,
			I8[1] <= v.I8[1] ? 0xFF : 0,
			I8[2] <= v.I8[2] ? 0xFF : 0,
			I8[3] <= v.I8[3] ? 0xFF : 0,
			I8[4] <= v.I8[4] ? 0xFF : 0,
			I8[5] <= v.I8[5] ? 0xFF : 0,
			I8[6] <= v.I8[6] ? 0xFF : 0,
			I8[7] <= v.I8[7] ? 0xFF : 0,
			I8[8] <= v.I8[8] ? 0xFF : 0,
			I8[9] <= v.I8[9] ? 0xFF : 0,
			I8[10] <= v.I8[10] ? 0xFF : 0,
			I8[11] <= v.I8[11] ? 0xFF : 0,
			I8[12] <= v.I8[12] ? 0xFF : 0,
			I8[13] <= v.I8[13] ? 0xFF : 0,
			I8[14] <= v.I8[14] ? 0xFF : 0,
			I8[15] <= v.I8[15] ? 0xFF : 0);
	}
	__forceinline GSVector4i le16(const GSVector4i& v) const
	{
		return GSVector4i(
			I16[0] <= v.I16[0] ? 0xFFFF : 0,
			I16[1] <= v.I16[1] ? 0xFFFF : 0,
			I16[2] <= v.I16[2] ? 0xFFFF : 0,
			I16[3] <= v.I16[3] ? 0xFFFF : 0,
			I16[4] <= v.I16[4] ? 0xFFFF : 0,
			I16[5] <= v.I16[5] ? 0xFFFF : 0,
			I16[6] <= v.I16[6] ? 0xFFFF : 0,
			I16[7] <= v.I16[7] ? 0xFFFF : 0);
	}
	__forceinline GSVector4i le32(const GSVector4i& v) const
	{
		return GSVector4i(
			I32[0] <= v.I32[0] ? 0xFFFFFFFF : 0,
			I32[1] <= v.I32[1] ? 0xFFFFFFFF : 0,
			I32[2] <= v.I32[2] ? 0xFFFFFFFF : 0,
			I32[3] <= v.I32[3] ? 0xFFFFFFFF : 0);
	}

	__forceinline GSVector4i andnot(const GSVector4i& v) const
	{
		return GSVector4i(
			I32[0] & ~v.I32[0],
			I32[1] & ~v.I32[1],
			I32[2] & ~v.I32[2],
			I32[3] & ~v.I32[3]);
	}

	__forceinline int mask() const
	{
		return ((U8[0] >> 7) << 0) |
			   ((U8[1] >> 7) << 1) |
			   ((U8[2] >> 7) << 2) |
			   ((U8[3] >> 7) << 3) |
			   ((U8[4] >> 7) << 4) |
			   ((U8[5] >> 7) << 5) |
			   ((U8[6] >> 7) << 6) |
			   ((U8[7] >> 7) << 7) |
			   ((U8[8] >> 7) << 8) |
			   ((U8[9] >> 7) << 9) |
			   ((U8[10] >> 7) << 10) |
			   ((U8[11] >> 7) << 11) |
			   ((U8[12] >> 7) << 12) |
			   ((U8[13] >> 7) << 13) |
			   ((U8[14] >> 7) << 14) |
			   ((U8[15] >> 7) << 15);
	}

	__forceinline bool alltrue() const
	{
		return mask() == 0xffff;
	}

	__forceinline bool allfalse() const
	{
		return (I32[0] == 0) &&
			   (I32[1] == 0) &&
			   (I32[2] == 0) &&
			   (I32[3] == 0);
	}

	template <int i>
	__forceinline GSVector4i insert8(int a) const
	{
		return GSVector4i(
			i == 0 ? a : I8[0],
			i == 1 ? a : I8[1],
			i == 2 ? a : I8[2],
			i == 3 ? a : I8[3],
			i == 4 ? a : I8[4],
			i == 5 ? a : I8[5],
			i == 6 ? a : I8[6],
			i == 7 ? a : I8[7],
			i == 8 ? a : I8[8],
			i == 9 ? a : I8[9],
			i == 10 ? a : I8[10],
			i == 11 ? a : I8[11],
			i == 12 ? a : I8[12],
			i == 13 ? a : I8[13],
			i == 14 ? a : I8[14],
			i == 15 ? a : I8[15]);
	}

	template <int i>
	__forceinline int extract8() const
	{
		// No sign extend
		pxAssert(i < 16);
		return U8[i];
	}

	template <int i>
	__forceinline GSVector4i insert16(int a) const
	{
		return GSVector4i(
			i == 0 ? a : I16[0],
			i == 1 ? a : I16[1],
			i == 2 ? a : I16[2],
			i == 3 ? a : I16[3],
			i == 4 ? a : I16[4],
			i == 5 ? a : I16[5],
			i == 6 ? a : I16[6],
			i == 7 ? a : I16[7]);
	}

	template <int i>
	__forceinline int extract16() const
	{
		// No sign extend
		pxAssert(i < 8);
		return U16[i];
	}

	template <int i>
	__forceinline GSVector4i insert32(int a) const
	{
		return GSVector4i(
			i == 0 ? a : I32[0],
			i == 1 ? a : I32[1],
			i == 2 ? a : I32[2],
			i == 3 ? a : I32[3]);
	}

	template <int i>
	__forceinline int extract32() const
	{
		pxAssert(i < 4);
		return I32[i];
	}

	template <int i>
	__forceinline GSVector4i insert64(s64 a) const
	{
		return GSVector4i(
			i == 0 ? ((s32*)&a)[0] : I32[0],
			i == 0 ? ((s32*)&a)[1] : I32[1],
			i == 1 ? ((s32*)&a)[0] : I32[2],
			i == 1 ? ((s32*)&a)[1] : I32[3]);
	}

	template <int i>
	__forceinline s64 extract64() const
	{
		pxAssert(i < 2);
		return I64[i];
	}

	template <int src, class T>
	__forceinline GSVector4i gather8_4(const T* ptr) const
	{
		GSVector4i v;

		v = load((int)ptr[extract8<src + 0>() & 0xf]);
		v = v.insert8<1>((int)ptr[extract8<src + 0>() >> 4]);
		v = v.insert8<2>((int)ptr[extract8<src + 1>() & 0xf]);
		v = v.insert8<3>((int)ptr[extract8<src + 1>() >> 4]);
		v = v.insert8<4>((int)ptr[extract8<src + 2>() & 0xf]);
		v = v.insert8<5>((int)ptr[extract8<src + 2>() >> 4]);
		v = v.insert8<6>((int)ptr[extract8<src + 3>() & 0xf]);
		v = v.insert8<7>((int)ptr[extract8<src + 3>() >> 4]);
		v = v.insert8<8>((int)ptr[extract8<src + 4>() & 0xf]);
		v = v.insert8<9>((int)ptr[extract8<src + 4>() >> 4]);
		v = v.insert8<10>((int)ptr[extract8<src + 5>() & 0xf]);
		v = v.insert8<11>((int)ptr[extract8<src + 5>() >> 4]);
		v = v.insert8<12>((int)ptr[extract8<src + 6>() & 0xf]);
		v = v.insert8<13>((int)ptr[extract8<src + 6>() >> 4]);
		v = v.insert8<14>((int)ptr[extract8<src + 7>() & 0xf]);
		v = v.insert8<15>((int)ptr[extract8<src + 7>() >> 4]);

		return v;
	}

	template <class T>
	__forceinline GSVector4i gather8_8(const T* ptr) const
	{
		GSVector4i v;

		v = load((int)ptr[extract8<0>()]);
		v = v.insert8<1>((int)ptr[extract8<1>()]);
		v = v.insert8<2>((int)ptr[extract8<2>()]);
		v = v.insert8<3>((int)ptr[extract8<3>()]);
		v = v.insert8<4>((int)ptr[extract8<4>()]);
		v = v.insert8<5>((int)ptr[extract8<5>()]);
		v = v.insert8<6>((int)ptr[extract8<6>()]);
		v = v.insert8<7>((int)ptr[extract8<7>()]);
		v = v.insert8<8>((int)ptr[extract8<8>()]);
		v = v.insert8<9>((int)ptr[extract8<9>()]);
		v = v.insert8<10>((int)ptr[extract8<10>()]);
		v = v.insert8<11>((int)ptr[extract8<11>()]);
		v = v.insert8<12>((int)ptr[extract8<12>()]);
		v = v.insert8<13>((int)ptr[extract8<13>()]);
		v = v.insert8<14>((int)ptr[extract8<14>()]);
		v = v.insert8<15>((int)ptr[extract8<15>()]);

		return v;
	}

	template <int dst, class T>
	__forceinline GSVector4i gather8_16(const T* ptr, const GSVector4i& a) const
	{
		GSVector4i v = a;

		v = v.insert8<dst + 0>((int)ptr[extract16<0>()]);
		v = v.insert8<dst + 1>((int)ptr[extract16<1>()]);
		v = v.insert8<dst + 2>((int)ptr[extract16<2>()]);
		v = v.insert8<dst + 3>((int)ptr[extract16<3>()]);
		v = v.insert8<dst + 4>((int)ptr[extract16<4>()]);
		v = v.insert8<dst + 5>((int)ptr[extract16<5>()]);
		v = v.insert8<dst + 6>((int)ptr[extract16<6>()]);
		v = v.insert8<dst + 7>((int)ptr[extract16<7>()]);

		return v;
	}

	template <int dst, class T>
	__forceinline GSVector4i gather8_32(const T* ptr, const GSVector4i& a) const
	{
		GSVector4i v = a;

		v = v.insert8<dst + 0>((int)ptr[extract32<0>()]);
		v = v.insert8<dst + 1>((int)ptr[extract32<1>()]);
		v = v.insert8<dst + 2>((int)ptr[extract32<2>()]);
		v = v.insert8<dst + 3>((int)ptr[extract32<3>()]);

		return v;
	}

	template <int src, class T>
	__forceinline GSVector4i gather16_4(const T* ptr) const
	{
		GSVector4i v;

		v = load((int)ptr[extract8<src + 0>() & 0xf]);
		v = v.insert16<1>((int)ptr[extract8<src + 0>() >> 4]);
		v = v.insert16<2>((int)ptr[extract8<src + 1>() & 0xf]);
		v = v.insert16<3>((int)ptr[extract8<src + 1>() >> 4]);
		v = v.insert16<4>((int)ptr[extract8<src + 2>() & 0xf]);
		v = v.insert16<5>((int)ptr[extract8<src + 2>() >> 4]);
		v = v.insert16<6>((int)ptr[extract8<src + 3>() & 0xf]);
		v = v.insert16<7>((int)ptr[extract8<src + 3>() >> 4]);

		return v;
	}

	template <int src, class T>
	__forceinline GSVector4i gather16_8(const T* ptr) const
	{
		GSVector4i v;

		v = load((int)ptr[extract8<src + 0>()]);
		v = v.insert16<1>((int)ptr[extract8<src + 1>()]);
		v = v.insert16<2>((int)ptr[extract8<src + 2>()]);
		v = v.insert16<3>((int)ptr[extract8<src + 3>()]);
		v = v.insert16<4>((int)ptr[extract8<src + 4>()]);
		v = v.insert16<5>((int)ptr[extract8<src + 5>()]);
		v = v.insert16<6>((int)ptr[extract8<src + 6>()]);
		v = v.insert16<7>((int)ptr[extract8<src + 7>()]);

		return v;
	}

	template <class T>
	__forceinline GSVector4i gather16_16(const T* ptr) const
	{
		GSVector4i v;

		v = load((int)ptr[extract16<0>()]);
		v = v.insert16<1>((int)ptr[extract16<1>()]);
		v = v.insert16<2>((int)ptr[extract16<2>()]);
		v = v.insert16<3>((int)ptr[extract16<3>()]);
		v = v.insert16<4>((int)ptr[extract16<4>()]);
		v = v.insert16<5>((int)ptr[extract16<5>()]);
		v = v.insert16<6>((int)ptr[extract16<6>()]);
		v = v.insert16<7>((int)ptr[extract16<7>()]);

		return v;
	}

	template <class T1, class T2>
	__forceinline GSVector4i gather16_16(const T1* ptr1, const T2* ptr2) const
	{
		GSVector4i v;

		v = load((int)ptr2[ptr1[extract16<0>()]]);
		v = v.insert16<1>((int)ptr2[ptr1[extract16<1>()]]);
		v = v.insert16<2>((int)ptr2[ptr1[extract16<2>()]]);
		v = v.insert16<3>((int)ptr2[ptr1[extract16<3>()]]);
		v = v.insert16<4>((int)ptr2[ptr1[extract16<4>()]]);
		v = v.insert16<5>((int)ptr2[ptr1[extract16<5>()]]);
		v = v.insert16<6>((int)ptr2[ptr1[extract16<6>()]]);
		v = v.insert16<7>((int)ptr2[ptr1[extract16<7>()]]);

		return v;
	}

	template <int dst, class T>
	__forceinline GSVector4i gather16_32(const T* ptr, const GSVector4i& a) const
	{
		GSVector4i v = a;

		v = v.insert16<dst + 0>((int)ptr[extract32<0>()]);
		v = v.insert16<dst + 1>((int)ptr[extract32<1>()]);
		v = v.insert16<dst + 2>((int)ptr[extract32<2>()]);
		v = v.insert16<dst + 3>((int)ptr[extract32<3>()]);

		return v;
	}

	template <int src, class T>
	__forceinline GSVector4i gather32_4(const T* ptr) const
	{
		GSVector4i v;

		v = load((int)ptr[extract8<src + 0>() & 0xf]);
		v = v.insert32<1>((int)ptr[extract8<src + 0>() >> 4]);
		v = v.insert32<2>((int)ptr[extract8<src + 1>() & 0xf]);
		v = v.insert32<3>((int)ptr[extract8<src + 1>() >> 4]);
		return v;
	}

	template <int src, class T>
	__forceinline GSVector4i gather32_8(const T* ptr) const
	{
		GSVector4i v;

		v = load((int)ptr[extract8<src + 0>()]);
		v = v.insert32<1>((int)ptr[extract8<src + 1>()]);
		v = v.insert32<2>((int)ptr[extract8<src + 2>()]);
		v = v.insert32<3>((int)ptr[extract8<src + 3>()]);

		return v;
	}

	template <int src, class T>
	__forceinline GSVector4i gather32_16(const T* ptr) const
	{
		GSVector4i v;

		v = load((int)ptr[extract16<src + 0>()]);
		v = v.insert32<1>((int)ptr[extract16<src + 1>()]);
		v = v.insert32<2>((int)ptr[extract16<src + 2>()]);
		v = v.insert32<3>((int)ptr[extract16<src + 3>()]);

		return v;
	}

	template <class T>
	__forceinline GSVector4i gather32_32(const T* ptr) const
	{
		GSVector4i v;

		v = load((int)ptr[extract32<0>()]);
		v = v.insert32<1>((int)ptr[extract32<1>()]);
		v = v.insert32<2>((int)ptr[extract32<2>()]);
		v = v.insert32<3>((int)ptr[extract32<3>()]);

		return v;
	}

	template <class T1, class T2>
	__forceinline GSVector4i gather32_32(const T1* ptr1, const T2* ptr2) const
	{
		GSVector4i v;

		v = load((int)ptr2[ptr1[extract32<0>()]]);
		v = v.insert32<1>((int)ptr2[ptr1[extract32<1>()]]);
		v = v.insert32<2>((int)ptr2[ptr1[extract32<2>()]]);
		v = v.insert32<3>((int)ptr2[ptr1[extract32<3>()]]);

		return v;
	}

	template <int src, class T>
	__forceinline GSVector4i gather64_4(const T* ptr) const
	{
		GSVector4i v;

		v = loadq((s64)ptr[extract8<src + 0>() & 0xf]);
		v = v.insert64<1>((s64)ptr[extract8<src + 0>() >> 4]);

		return v;
	}

	template <int src, class T>
	__forceinline GSVector4i gather64_8(const T* ptr) const
	{
		GSVector4i v;

		v = loadq((s64)ptr[extract8<src + 0>()]);
		v = v.insert64<1>((s64)ptr[extract8<src + 1>()]);

		return v;
	}

	template <int src, class T>
	__forceinline GSVector4i gather64_16(const T* ptr) const
	{
		GSVector4i v;

		v = loadq((s64)ptr[extract16<src + 0>()]);
		v = v.insert64<1>((s64)ptr[extract16<src + 1>()]);

		return v;
	}

	template <int src, class T>
	__forceinline GSVector4i gather64_32(const T* ptr) const
	{
		GSVector4i v;

		v = loadq((s64)ptr[extract32<src + 0>()]);
		v = v.insert64<1>((s64)ptr[extract32<src + 1>()]);

		return v;
	}

	template <class T>
	__forceinline GSVector4i gather64_64(const T* ptr) const
	{
		GSVector4i v;

		v = loadq((s64)ptr[extract64<0>()]);
		v = v.insert64<1>((s64)ptr[extract64<1>()]);

		return v;
	}

	template <class T>
	__forceinline void gather8_4(const T* RESTRICT ptr, GSVector4i* RESTRICT dst) const
	{
		dst[0] = gather8_4<0>(ptr);
		dst[1] = gather8_4<8>(ptr);
	}

	__forceinline void gather8_8(const u8* RESTRICT ptr, GSVector4i* RESTRICT dst) const
	{
		dst[0] = gather8_8<>(ptr);
	}

	template <class T>
	__forceinline void gather16_4(const T* RESTRICT ptr, GSVector4i* RESTRICT dst) const
	{
		dst[0] = gather16_4<0>(ptr);
		dst[1] = gather16_4<4>(ptr);
		dst[2] = gather16_4<8>(ptr);
		dst[3] = gather16_4<12>(ptr);
	}

	template <class T>
	__forceinline void gather16_8(const T* RESTRICT ptr, GSVector4i* RESTRICT dst) const
	{
		dst[0] = gather16_8<0>(ptr);
		dst[1] = gather16_8<8>(ptr);
	}

	template <class T>
	__forceinline void gather16_16(const T* RESTRICT ptr, GSVector4i* RESTRICT dst) const
	{
		dst[0] = gather16_16<>(ptr);
	}

	template <class T>
	__forceinline void gather32_4(const T* RESTRICT ptr, GSVector4i* RESTRICT dst) const
	{
		dst[0] = gather32_4<0>(ptr);
		dst[1] = gather32_4<2>(ptr);
		dst[2] = gather32_4<4>(ptr);
		dst[3] = gather32_4<6>(ptr);
		dst[4] = gather32_4<8>(ptr);
		dst[5] = gather32_4<10>(ptr);
		dst[6] = gather32_4<12>(ptr);
		dst[7] = gather32_4<14>(ptr);
	}

	template <class T>
	__forceinline void gather32_8(const T* RESTRICT ptr, GSVector4i* RESTRICT dst) const
	{
		dst[0] = gather32_8<0>(ptr);
		dst[1] = gather32_8<4>(ptr);
		dst[2] = gather32_8<8>(ptr);
		dst[3] = gather32_8<12>(ptr);
	}

	template <class T>
	__forceinline void gather32_16(const T* RESTRICT ptr, GSVector4i* RESTRICT dst) const
	{
		dst[0] = gather32_16<0>(ptr);
		dst[1] = gather32_16<4>(ptr);
	}

	template <class T>
	__forceinline void gather32_32(const T* RESTRICT ptr, GSVector4i* RESTRICT dst) const
	{
		dst[0] = gather32_32<>(ptr);
	}

	template <class T>
	__forceinline void gather64_4(const T* RESTRICT ptr, GSVector4i* RESTRICT dst) const
	{
		dst[0] = gather64_4<0>(ptr);
		dst[1] = gather64_4<1>(ptr);
		dst[2] = gather64_4<2>(ptr);
		dst[3] = gather64_4<3>(ptr);
		dst[4] = gather64_4<4>(ptr);
		dst[5] = gather64_4<5>(ptr);
		dst[6] = gather64_4<6>(ptr);
		dst[7] = gather64_4<7>(ptr);
		dst[8] = gather64_4<8>(ptr);
		dst[9] = gather64_4<9>(ptr);
		dst[10] = gather64_4<10>(ptr);
		dst[11] = gather64_4<11>(ptr);
		dst[12] = gather64_4<12>(ptr);
		dst[13] = gather64_4<13>(ptr);
		dst[14] = gather64_4<14>(ptr);
		dst[15] = gather64_4<15>(ptr);
	}

	template <class T>
	__forceinline void gather64_8(const T* RESTRICT ptr, GSVector4i* RESTRICT dst) const
	{
		dst[0] = gather64_8<0>(ptr);
		dst[1] = gather64_8<2>(ptr);
		dst[2] = gather64_8<4>(ptr);
		dst[3] = gather64_8<6>(ptr);
		dst[4] = gather64_8<8>(ptr);
		dst[5] = gather64_8<10>(ptr);
		dst[6] = gather64_8<12>(ptr);
		dst[7] = gather64_8<14>(ptr);
	}

	template <class T>
	__forceinline void gather64_16(const T* RESTRICT ptr, GSVector4i* RESTRICT dst) const
	{
		dst[0] = gather64_16<0>(ptr);
		dst[1] = gather64_16<2>(ptr);
		dst[2] = gather64_16<4>(ptr);
		dst[3] = gather64_16<8>(ptr);
	}

	template <class T>
	__forceinline void gather64_32(const T* RESTRICT ptr, GSVector4i* RESTRICT dst) const
	{
		dst[0] = gather64_32<0>(ptr);
		dst[1] = gather64_32<2>(ptr);
	}

	template <class T>
	__forceinline void gather64_64(const T* RESTRICT ptr, GSVector4i* RESTRICT dst) const
	{
		dst[0] = gather64_64<>(ptr);
	}

	__forceinline static GSVector4i loadnt(const void* p)
	{
		const s32* ret = static_cast<const s32*>(p);
		return GSVector4i(ret[0], ret[1], ret[2], ret[3]);
	}

	__forceinline static GSVector4i loadl(const void* p)
	{
		const s32* ret = static_cast<const s32*>(p);
		return GSVector4i(ret[0], ret[1], 0, 0);
	}

	__forceinline static GSVector4i loadh(const void* p)
	{
		const s32* ret = static_cast<const s32*>(p);
		return GSVector4i(0, 0, ret[0], ret[1]);
	}

	__forceinline static GSVector4i loadh(const void* p, const GSVector4i& v)
	{
		const s32* ret = static_cast<const s32*>(p);
		return GSVector4i(v.I32[0], v.I32[1], ret[0], ret[1]);
	}

	__forceinline static GSVector4i loadh(const GSVector2i& v)
	{
		return loadh(&v);
	}

	__forceinline static GSVector4i load(const void* pl, const void* ph)
	{
		const s32* lo = static_cast<const s32*>(pl);
		const s32* hi = static_cast<const s32*>(ph);
		return GSVector4i(lo[0], lo[1], hi[0], hi[1]);
	}
	template <bool aligned>
	__forceinline static GSVector4i load(const void* p)
	{
		const s32* ret = static_cast<const s32*>(p);
		return GSVector4i(ret[0], ret[1], ret[2], ret[3]);
	}

	__forceinline static GSVector4i load(int i)
	{
		return GSVector4i(i, 0, 0, 0);
	}

	__forceinline static GSVector4i loadq(s64 i)
	{
		return GSVector4i(reinterpret_cast<s32*>(&i)[0], reinterpret_cast<s32*>(&i)[1], 0, 0);
	}

	__forceinline static void storent(void* p, const GSVector4i& v)
	{
		std::memcpy(p, v.U8, 16);
	}

	__forceinline static void storel(void* p, const GSVector4i& v)
	{
		std::memcpy(p, v.U8, 8);
	}

	__forceinline static void storeh(void* p, const GSVector4i& v)
	{
		std::memcpy(p, &v.U8[8], 8);
	}

	__forceinline static void store(void* pl, void* ph, const GSVector4i& v)
	{
		GSVector4i::storel(pl, v);
		GSVector4i::storeh(ph, v);
	}

	template <bool aligned>
	__forceinline static void store(void* p, const GSVector4i& v)
	{
		std::memcpy(p, v.U8, 16);
	}

	__forceinline static int store(const GSVector4i& v)
	{
		return v.I32[0];
	}

	__forceinline static s64 storeq(const GSVector4i& v)
	{
		return v.I64[0];
	}

	__forceinline static void storent(void* RESTRICT dst, const void* RESTRICT src, size_t size)
	{
		const GSVector4i* s = (const GSVector4i*)src;
		GSVector4i* d = (GSVector4i*)dst;

		if (size == 0)
			return;

		size_t i = 0;
		size_t j = size >> 6;

		for (; i < j; i++, s += 4, d += 4)
		{
			storent(&d[0], s[0]);
			storent(&d[1], s[1]);
			storent(&d[2], s[2]);
			storent(&d[3], s[3]);
		}

		size &= 63;

		if (size == 0)
			return;

		memcpy(d, s, size);
	}

	__forceinline static void mix4(GSVector4i& a, GSVector4i& b)
	{
		GSVector4i mask = GSVector4i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f);

		GSVector4i c = (b << 4).blend(a, mask);
		GSVector4i d = b.blend(a >> 4, mask);
		a = c;
		b = d;
	}

	__forceinline static void sw4(GSVector4i& a, GSVector4i& b, GSVector4i& c, GSVector4i& d)
	{
		mix4(a, b);
		mix4(c, d);
		sw8(a, b, c, d);
	}

	__forceinline static void sw8(GSVector4i& a, GSVector4i& b, GSVector4i& c, GSVector4i& d)
	{
		GSVector4i e = a;
		GSVector4i f = c;

		a = e.upl8(b);
		c = e.uph8(b);
		b = f.upl8(d);
		d = f.uph8(d);
	}

	__forceinline static void sw16(GSVector4i& a, GSVector4i& b, GSVector4i& c, GSVector4i& d)
	{
		GSVector4i e = a;
		GSVector4i f = c;

		a = e.upl16(b);
		c = e.uph16(b);
		b = f.upl16(d);
		d = f.uph16(d);
	}

	__forceinline static void sw16rl(GSVector4i& a, GSVector4i& b, GSVector4i& c, GSVector4i& d)
	{
		GSVector4i e = a;
		GSVector4i f = c;

		a = b.upl16(e);
		c = e.uph16(b);
		b = d.upl16(f);
		d = f.uph16(d);
	}

	__forceinline static void sw16rh(GSVector4i& a, GSVector4i& b, GSVector4i& c, GSVector4i& d)
	{
		GSVector4i e = a;
		GSVector4i f = c;

		a = e.upl16(b);
		c = b.uph16(e);
		b = f.upl16(d);
		d = d.uph16(f);
	}

	__forceinline static void sw32(GSVector4i& a, GSVector4i& b, GSVector4i& c, GSVector4i& d)
	{
		GSVector4i e = a;
		GSVector4i f = c;

		a = e.upl32(b);
		c = e.uph32(b);
		b = f.upl32(d);
		d = f.uph32(d);
	}

	__forceinline static void sw32_inv(GSVector4i& a, GSVector4i& b, GSVector4i& c, GSVector4i& d);

	__forceinline static void sw64(GSVector4i& a, GSVector4i& b, GSVector4i& c, GSVector4i& d)
	{
		GSVector4i e = a;
		GSVector4i f = c;

		a = e.upl64(b);
		c = e.uph64(b);
		b = f.upl64(d);
		d = f.uph64(d);
	}

	__forceinline static bool compare16(const void* dst, const void* src, size_t size)
	{
		pxAssert((size & 15) == 0);

		size >>= 4;

		GSVector4i* s = (GSVector4i*)src;
		GSVector4i* d = (GSVector4i*)dst;

		for (size_t i = 0; i < size; i++)
		{
			if (!d[i].eq(s[i]))
			{
				return false;
			}
		}

		return true;
	}

	__forceinline static bool compare64(const void* dst, const void* src, size_t size)
	{
		pxAssert((size & 63) == 0);

		size >>= 6;

		GSVector4i* s = (GSVector4i*)src;
		GSVector4i* d = (GSVector4i*)dst;

		for (size_t i = 0; i < size; ++i)
		{
			GSVector4i v0 = (d[i * 4 + 0] == s[i * 4 + 0]);
			GSVector4i v1 = (d[i * 4 + 1] == s[i * 4 + 1]);
			GSVector4i v2 = (d[i * 4 + 2] == s[i * 4 + 2]);
			GSVector4i v3 = (d[i * 4 + 3] == s[i * 4 + 3]);

			v0 = v0 & v1;
			v2 = v2 & v3;

			if (!(v0 & v2).alltrue())
			{
				return false;
			}
		}

		return true;
	}

	__forceinline static bool update(const void* dst, const void* src, size_t size)
	{
		pxAssert((size & 15) == 0);

		size >>= 4;

		GSVector4i* s = (GSVector4i*)src;
		GSVector4i* d = (GSVector4i*)dst;

		GSVector4i v = GSVector4i::xffffffff();

		for (size_t i = 0; i < size; i++)
		{
			v &= d[i] == s[i];

			d[i] = s[i];
		}

		return v.alltrue();
	}

	__forceinline void operator+=(const GSVector4i& v)
	{
		I32[0] += v.I32[0];
		I32[1] += v.I32[1];
		I32[2] += v.I32[2];
		I32[3] += v.I32[3];
	}

	__forceinline void operator-=(const GSVector4i& v)
	{
		I32[0] -= v.I32[0];
		I32[1] -= v.I32[1];
		I32[2] -= v.I32[2];
		I32[3] -= v.I32[3];
	}

	__forceinline void operator+=(int i)
	{
		*this += GSVector4i(i);
	}

	__forceinline void operator-=(int i)
	{
		*this -= GSVector4i(i);
	}

	__forceinline void operator<<=(const int i)
	{
		U32[0] <<= i;
		U32[1] <<= i;
		U32[2] <<= i;
		U32[3] <<= i;
	}

	__forceinline void operator>>=(const int i)
	{
		U32[0] >>= i;
		U32[1] >>= i;
		U32[2] >>= i;
		U32[3] >>= i;
	}

	__forceinline void operator&=(const GSVector4i& v)
	{
		I32[0] &= v.I32[0];
		I32[1] &= v.I32[1];
		I32[2] &= v.I32[2];
		I32[3] &= v.I32[3];
	}

	__forceinline void operator|=(const GSVector4i& v)
	{
		I32[0] |= v.I32[0];
		I32[1] |= v.I32[1];
		I32[2] |= v.I32[2];
		I32[3] |= v.I32[3];
	}

	__forceinline void operator^=(const GSVector4i& v)
	{
		I32[0] ^= v.I32[0];
		I32[1] ^= v.I32[1];
		I32[2] ^= v.I32[2];
		I32[3] ^= v.I32[3];
	}

	__forceinline friend GSVector4i operator+(const GSVector4i& v1, const GSVector4i& v2)
	{
		return GSVector4i(
			v1.I32[0] + v2.I32[0],
			v1.I32[1] + v2.I32[1],
			v1.I32[2] + v2.I32[2],
			v1.I32[3] + v2.I32[3]);
	}

	__forceinline friend GSVector4i operator-(const GSVector4i& v1, const GSVector4i& v2)
	{
		return GSVector4i(
			v1.I32[0] - v2.I32[0],
			v1.I32[1] - v2.I32[1],
			v1.I32[2] - v2.I32[2],
			v1.I32[3] - v2.I32[3]);
	}

	__forceinline friend GSVector4i operator+(const GSVector4i& v, int i)
	{
		return v + GSVector4i(i);
	}

	__forceinline friend GSVector4i operator-(const GSVector4i& v, int i)
	{
		return v - GSVector4i(i);
	}

	__forceinline friend GSVector4i operator<<(const GSVector4i& v, const int i)
	{
		return GSVector4i(
			v.U32[0] << i,
			v.U32[1] << i,
			v.U32[2] << i,
			v.U32[3] << i);
	}

	__forceinline friend GSVector4i operator>>(const GSVector4i& v, const int i)
	{
		return GSVector4i(
			v.U32[0] >> i,
			v.U32[1] >> i,
			v.U32[2] >> i,
			v.U32[3] >> i);
	}

	__forceinline friend GSVector4i operator&(const GSVector4i& v1, const GSVector4i& v2)
	{
		return GSVector4i(
			v1.I32[0] & v2.I32[0],
			v1.I32[1] & v2.I32[1],
			v1.I32[2] & v2.I32[2],
			v1.I32[3] & v2.I32[3]);
	}

	__forceinline friend GSVector4i operator|(const GSVector4i& v1, const GSVector4i& v2)
	{
		return GSVector4i(
			v1.I32[0] | v2.I32[0],
			v1.I32[1] | v2.I32[1],
			v1.I32[2] | v2.I32[2],
			v1.I32[3] | v2.I32[3]);
	}

	__forceinline friend GSVector4i operator^(const GSVector4i& v1, const GSVector4i& v2)
	{
		return GSVector4i(
			v1.I32[0] ^ v2.I32[0],
			v1.I32[1] ^ v2.I32[1],
			v1.I32[2] ^ v2.I32[2],
			v1.I32[3] ^ v2.I32[3]);
	}

	__forceinline friend GSVector4i operator&(const GSVector4i& v, int i)
	{
		return v & GSVector4i(i);
	}

	__forceinline friend GSVector4i operator|(const GSVector4i& v, int i)
	{
		return v | GSVector4i(i);
	}

	__forceinline friend GSVector4i operator^(const GSVector4i& v, int i)
	{
		return v ^ GSVector4i(i);
	}

	__forceinline friend GSVector4i operator~(const GSVector4i& v)
	{
		return v ^ (v == v);
	}

	__forceinline friend GSVector4i operator==(const GSVector4i& v1, const GSVector4i& v2)
	{
		return GSVector4i(
			v1.I32[0] == v2.I32[0] ? 0xFFFFFFFF : 0,
			v1.I32[1] == v2.I32[1] ? 0xFFFFFFFF : 0,
			v1.I32[2] == v2.I32[2] ? 0xFFFFFFFF : 0,
			v1.I32[3] == v2.I32[3] ? 0xFFFFFFFF : 0);
	}

	__forceinline friend GSVector4i operator!=(const GSVector4i& v1, const GSVector4i& v2)
	{
		return ~(v1 == v2);
	}

	__forceinline friend GSVector4i operator>(const GSVector4i& v1, const GSVector4i& v2)
	{
		return GSVector4i(
			v1.I32[0] > v2.I32[0] ? 0xFFFFFFFF : 0,
			v1.I32[1] > v2.I32[1] ? 0xFFFFFFFF : 0,
			v1.I32[2] > v2.I32[2] ? 0xFFFFFFFF : 0,
			v1.I32[3] > v2.I32[3] ? 0xFFFFFFFF : 0);
	}

	__forceinline friend GSVector4i operator<(const GSVector4i& v1, const GSVector4i& v2)
	{
		return GSVector4i(
			v1.I32[0] < v2.I32[0] ? 0xFFFFFFFF : 0,
			v1.I32[1] < v2.I32[1] ? 0xFFFFFFFF : 0,
			v1.I32[2] < v2.I32[2] ? 0xFFFFFFFF : 0,
			v1.I32[3] < v2.I32[3] ? 0xFFFFFFFF : 0);
	}

	__forceinline friend GSVector4i operator>=(const GSVector4i& v1, const GSVector4i& v2)
	{
		return (v1 > v2) | (v1 == v2);
	}

	__forceinline friend GSVector4i operator<=(const GSVector4i& v1, const GSVector4i& v2)
	{
		return (v1 < v2) | (v1 == v2);
	}

	// clang-format off

	#define VECTOR4i_SHUFFLE_4(xs, xn, ys, yn, zs, zn, ws, wn) \
		__forceinline GSVector4i xs##ys##zs##ws() const \
		{ \
			return GSVector4i( \
				I32[xn], \
				I32[yn], \
				I32[zn], \
				I32[wn]); \
		} \
		__forceinline GSVector4i xs##ys##zs##ws##l() const \
		{ \
			return GSVector4i( \
				I16[xn], \
				I16[yn], \
				I16[zn], \
				I16[wn], \
				I16[4], \
				I16[5], \
				I16[6], \
				I16[7]); \
		} \
		__forceinline GSVector4i xs##ys##zs##ws##h() const \
		{ \
			return GSVector4i( \
				I16[0], \
				I16[1], \
				I16[2], \
				I16[3], \
				I16[4 + xn], \
				I16[4 + yn], \
				I16[4 + zn], \
				I16[4 + wn]); \
		} \
		__forceinline GSVector4i xs##ys##zs##ws##lh() const \
		{ \
			return GSVector4i( \
				I16[xn], \
				I16[yn], \
				I16[zn], \
				I16[wn], \
				I16[4 + xn], \
				I16[4 + yn], \
				I16[4 + zn], \
				I16[4 + wn]); \
		} \

	#define VECTOR4i_SHUFFLE_3(xs, xn, ys, yn, zs, zn) \
		VECTOR4i_SHUFFLE_4(xs, xn, ys, yn, zs, zn, x, 0) \
		VECTOR4i_SHUFFLE_4(xs, xn, ys, yn, zs, zn, y, 1) \
		VECTOR4i_SHUFFLE_4(xs, xn, ys, yn, zs, zn, z, 2) \
		VECTOR4i_SHUFFLE_4(xs, xn, ys, yn, zs, zn, w, 3) \

	#define VECTOR4i_SHUFFLE_2(xs, xn, ys, yn) \
		VECTOR4i_SHUFFLE_3(xs, xn, ys, yn, x, 0) \
		VECTOR4i_SHUFFLE_3(xs, xn, ys, yn, y, 1) \
		VECTOR4i_SHUFFLE_3(xs, xn, ys, yn, z, 2) \
		VECTOR4i_SHUFFLE_3(xs, xn, ys, yn, w, 3) \

	#define VECTOR4i_SHUFFLE_1(xs, xn) \
		VECTOR4i_SHUFFLE_2(xs, xn, x, 0) \
		VECTOR4i_SHUFFLE_2(xs, xn, y, 1) \
		VECTOR4i_SHUFFLE_2(xs, xn, z, 2) \
		VECTOR4i_SHUFFLE_2(xs, xn, w, 3) \

	VECTOR4i_SHUFFLE_1(x, 0)
	VECTOR4i_SHUFFLE_1(y, 1)
	VECTOR4i_SHUFFLE_1(z, 2)
	VECTOR4i_SHUFFLE_1(w, 3)

	// clang-format on

	/// Noop, here so broadcast128 can be used generically over all vectors
	__forceinline static GSVector4i broadcast128(const GSVector4i& v)
	{
		return v;
	}

	__forceinline static GSVector4i broadcast16(u16 value)
	{
		return GSVector4i(value, value, value, value, value, value, value, value);
	}

	__forceinline static GSVector4i zero() { return GSVector4i(0); }

	__forceinline static GSVector4i xffffffff() { return GSVector4i(0xFFFFFFFF); }

	__forceinline static GSVector4i x00000001() { return xffffffff().srl32<31>(); }
	__forceinline static GSVector4i x00000003() { return xffffffff().srl32<30>(); }
	__forceinline static GSVector4i x00000007() { return xffffffff().srl32<29>(); }
	__forceinline static GSVector4i x0000000f() { return xffffffff().srl32<28>(); }
	__forceinline static GSVector4i x0000001f() { return xffffffff().srl32<27>(); }
	__forceinline static GSVector4i x0000003f() { return xffffffff().srl32<26>(); }
	__forceinline static GSVector4i x0000007f() { return xffffffff().srl32<25>(); }
	__forceinline static GSVector4i x000000ff() { return xffffffff().srl32<24>(); }
	__forceinline static GSVector4i x000001ff() { return xffffffff().srl32<23>(); }
	__forceinline static GSVector4i x000003ff() { return xffffffff().srl32<22>(); }
	__forceinline static GSVector4i x000007ff() { return xffffffff().srl32<21>(); }
	__forceinline static GSVector4i x00000fff() { return xffffffff().srl32<20>(); }
	__forceinline static GSVector4i x00001fff() { return xffffffff().srl32<19>(); }
	__forceinline static GSVector4i x00003fff() { return xffffffff().srl32<18>(); }
	__forceinline static GSVector4i x00007fff() { return xffffffff().srl32<17>(); }
	__forceinline static GSVector4i x0000ffff() { return xffffffff().srl32<16>(); }
	__forceinline static GSVector4i x0001ffff() { return xffffffff().srl32<15>(); }
	__forceinline static GSVector4i x0003ffff() { return xffffffff().srl32<14>(); }
	__forceinline static GSVector4i x0007ffff() { return xffffffff().srl32<13>(); }
	__forceinline static GSVector4i x000fffff() { return xffffffff().srl32<12>(); }
	__forceinline static GSVector4i x001fffff() { return xffffffff().srl32<11>(); }
	__forceinline static GSVector4i x003fffff() { return xffffffff().srl32<10>(); }
	__forceinline static GSVector4i x007fffff() { return xffffffff().srl32< 9>(); }
	__forceinline static GSVector4i x00ffffff() { return xffffffff().srl32< 8>(); }
	__forceinline static GSVector4i x01ffffff() { return xffffffff().srl32< 7>(); }
	__forceinline static GSVector4i x03ffffff() { return xffffffff().srl32< 6>(); }
	__forceinline static GSVector4i x07ffffff() { return xffffffff().srl32< 5>(); }
	__forceinline static GSVector4i x0fffffff() { return xffffffff().srl32< 4>(); }
	__forceinline static GSVector4i x1fffffff() { return xffffffff().srl32< 3>(); }
	__forceinline static GSVector4i x3fffffff() { return xffffffff().srl32< 2>(); }
	__forceinline static GSVector4i x7fffffff() { return xffffffff().srl32< 1>(); }

	__forceinline static GSVector4i x80000000() { return xffffffff().sll32<31>(); }
	__forceinline static GSVector4i xc0000000() { return xffffffff().sll32<30>(); }
	__forceinline static GSVector4i xe0000000() { return xffffffff().sll32<29>(); }
	__forceinline static GSVector4i xf0000000() { return xffffffff().sll32<28>(); }
	__forceinline static GSVector4i xf8000000() { return xffffffff().sll32<27>(); }
	__forceinline static GSVector4i xfc000000() { return xffffffff().sll32<26>(); }
	__forceinline static GSVector4i xfe000000() { return xffffffff().sll32<25>(); }
	__forceinline static GSVector4i xff000000() { return xffffffff().sll32<24>(); }
	__forceinline static GSVector4i xff800000() { return xffffffff().sll32<23>(); }
	__forceinline static GSVector4i xffc00000() { return xffffffff().sll32<22>(); }
	__forceinline static GSVector4i xffe00000() { return xffffffff().sll32<21>(); }
	__forceinline static GSVector4i xfff00000() { return xffffffff().sll32<20>(); }
	__forceinline static GSVector4i xfff80000() { return xffffffff().sll32<19>(); }
	__forceinline static GSVector4i xfffc0000() { return xffffffff().sll32<18>(); }
	__forceinline static GSVector4i xfffe0000() { return xffffffff().sll32<17>(); }
	__forceinline static GSVector4i xffff0000() { return xffffffff().sll32<16>(); }
	__forceinline static GSVector4i xffff8000() { return xffffffff().sll32<15>(); }
	__forceinline static GSVector4i xffffc000() { return xffffffff().sll32<14>(); }
	__forceinline static GSVector4i xffffe000() { return xffffffff().sll32<13>(); }
	__forceinline static GSVector4i xfffff000() { return xffffffff().sll32<12>(); }
	__forceinline static GSVector4i xfffff800() { return xffffffff().sll32<11>(); }
	__forceinline static GSVector4i xfffffc00() { return xffffffff().sll32<10>(); }
	__forceinline static GSVector4i xfffffe00() { return xffffffff().sll32< 9>(); }
	__forceinline static GSVector4i xffffff00() { return xffffffff().sll32< 8>(); }
	__forceinline static GSVector4i xffffff80() { return xffffffff().sll32< 7>(); }
	__forceinline static GSVector4i xffffffc0() { return xffffffff().sll32< 6>(); }
	__forceinline static GSVector4i xffffffe0() { return xffffffff().sll32< 5>(); }
	__forceinline static GSVector4i xfffffff0() { return xffffffff().sll32< 4>(); }
	__forceinline static GSVector4i xfffffff8() { return xffffffff().sll32< 3>(); }
	__forceinline static GSVector4i xfffffffc() { return xffffffff().sll32< 2>(); }
	__forceinline static GSVector4i xfffffffe() { return xffffffff().sll32< 1>(); }

	__forceinline static GSVector4i x0001() { return xffffffff().srl16<15>(); }
	__forceinline static GSVector4i x0003() { return xffffffff().srl16<14>(); }
	__forceinline static GSVector4i x0007() { return xffffffff().srl16<13>(); }
	__forceinline static GSVector4i x000f() { return xffffffff().srl16<12>(); }
	__forceinline static GSVector4i x001f() { return xffffffff().srl16<11>(); }
	__forceinline static GSVector4i x003f() { return xffffffff().srl16<10>(); }
	__forceinline static GSVector4i x007f() { return xffffffff().srl16< 9>(); }
	__forceinline static GSVector4i x00ff() { return xffffffff().srl16< 8>(); }
	__forceinline static GSVector4i x01ff() { return xffffffff().srl16< 7>(); }
	__forceinline static GSVector4i x03ff() { return xffffffff().srl16< 6>(); }
	__forceinline static GSVector4i x07ff() { return xffffffff().srl16< 5>(); }
	__forceinline static GSVector4i x0fff() { return xffffffff().srl16< 4>(); }
	__forceinline static GSVector4i x1fff() { return xffffffff().srl16< 3>(); }
	__forceinline static GSVector4i x3fff() { return xffffffff().srl16< 2>(); }
	__forceinline static GSVector4i x7fff() { return xffffffff().srl16< 1>(); }

	__forceinline static GSVector4i x8000() { return xffffffff().sll16<15>(); }
	__forceinline static GSVector4i xc000() { return xffffffff().sll16<14>(); }
	__forceinline static GSVector4i xe000() { return xffffffff().sll16<13>(); }
	__forceinline static GSVector4i xf000() { return xffffffff().sll16<12>(); }
	__forceinline static GSVector4i xf800() { return xffffffff().sll16<11>(); }
	__forceinline static GSVector4i xfc00() { return xffffffff().sll16<10>(); }
	__forceinline static GSVector4i xfe00() { return xffffffff().sll16< 9>(); }
	__forceinline static GSVector4i xff00() { return xffffffff().sll16< 8>(); }
	__forceinline static GSVector4i xff80() { return xffffffff().sll16< 7>(); }
	__forceinline static GSVector4i xffc0() { return xffffffff().sll16< 6>(); }
	__forceinline static GSVector4i xffe0() { return xffffffff().sll16< 5>(); }
	__forceinline static GSVector4i xfff0() { return xffffffff().sll16< 4>(); }
	__forceinline static GSVector4i xfff8() { return xffffffff().sll16< 3>(); }
	__forceinline static GSVector4i xfffc() { return xffffffff().sll16< 2>(); }
	__forceinline static GSVector4i xfffe() { return xffffffff().sll16< 1>(); }

	__forceinline static GSVector4i xffffffff(const GSVector4i& v) { return v == v; }

	__forceinline static GSVector4i x00000001(const GSVector4i& v) { return xffffffff(v).srl32<31>(); }
	__forceinline static GSVector4i x00000003(const GSVector4i& v) { return xffffffff(v).srl32<30>(); }
	__forceinline static GSVector4i x00000007(const GSVector4i& v) { return xffffffff(v).srl32<29>(); }
	__forceinline static GSVector4i x0000000f(const GSVector4i& v) { return xffffffff(v).srl32<28>(); }
	__forceinline static GSVector4i x0000001f(const GSVector4i& v) { return xffffffff(v).srl32<27>(); }
	__forceinline static GSVector4i x0000003f(const GSVector4i& v) { return xffffffff(v).srl32<26>(); }
	__forceinline static GSVector4i x0000007f(const GSVector4i& v) { return xffffffff(v).srl32<25>(); }
	__forceinline static GSVector4i x000000ff(const GSVector4i& v) { return xffffffff(v).srl32<24>(); }
	__forceinline static GSVector4i x000001ff(const GSVector4i& v) { return xffffffff(v).srl32<23>(); }
	__forceinline static GSVector4i x000003ff(const GSVector4i& v) { return xffffffff(v).srl32<22>(); }
	__forceinline static GSVector4i x000007ff(const GSVector4i& v) { return xffffffff(v).srl32<21>(); }
	__forceinline static GSVector4i x00000fff(const GSVector4i& v) { return xffffffff(v).srl32<20>(); }
	__forceinline static GSVector4i x00001fff(const GSVector4i& v) { return xffffffff(v).srl32<19>(); }
	__forceinline static GSVector4i x00003fff(const GSVector4i& v) { return xffffffff(v).srl32<18>(); }
	__forceinline static GSVector4i x00007fff(const GSVector4i& v) { return xffffffff(v).srl32<17>(); }
	__forceinline static GSVector4i x0000ffff(const GSVector4i& v) { return xffffffff(v).srl32<16>(); }
	__forceinline static GSVector4i x0001ffff(const GSVector4i& v) { return xffffffff(v).srl32<15>(); }
	__forceinline static GSVector4i x0003ffff(const GSVector4i& v) { return xffffffff(v).srl32<14>(); }
	__forceinline static GSVector4i x0007ffff(const GSVector4i& v) { return xffffffff(v).srl32<13>(); }
	__forceinline static GSVector4i x000fffff(const GSVector4i& v) { return xffffffff(v).srl32<12>(); }
	__forceinline static GSVector4i x001fffff(const GSVector4i& v) { return xffffffff(v).srl32<11>(); }
	__forceinline static GSVector4i x003fffff(const GSVector4i& v) { return xffffffff(v).srl32<10>(); }
	__forceinline static GSVector4i x007fffff(const GSVector4i& v) { return xffffffff(v).srl32< 9>(); }
	__forceinline static GSVector4i x00ffffff(const GSVector4i& v) { return xffffffff(v).srl32< 8>(); }
	__forceinline static GSVector4i x01ffffff(const GSVector4i& v) { return xffffffff(v).srl32< 7>(); }
	__forceinline static GSVector4i x03ffffff(const GSVector4i& v) { return xffffffff(v).srl32< 6>(); }
	__forceinline static GSVector4i x07ffffff(const GSVector4i& v) { return xffffffff(v).srl32< 5>(); }
	__forceinline static GSVector4i x0fffffff(const GSVector4i& v) { return xffffffff(v).srl32< 4>(); }
	__forceinline static GSVector4i x1fffffff(const GSVector4i& v) { return xffffffff(v).srl32< 3>(); }
	__forceinline static GSVector4i x3fffffff(const GSVector4i& v) { return xffffffff(v).srl32< 2>(); }
	__forceinline static GSVector4i x7fffffff(const GSVector4i& v) { return xffffffff(v).srl32< 1>(); }

	__forceinline static GSVector4i x80000000(const GSVector4i& v) { return xffffffff(v).sll32<31>(); }
	__forceinline static GSVector4i xc0000000(const GSVector4i& v) { return xffffffff(v).sll32<30>(); }
	__forceinline static GSVector4i xe0000000(const GSVector4i& v) { return xffffffff(v).sll32<29>(); }
	__forceinline static GSVector4i xf0000000(const GSVector4i& v) { return xffffffff(v).sll32<28>(); }
	__forceinline static GSVector4i xf8000000(const GSVector4i& v) { return xffffffff(v).sll32<27>(); }
	__forceinline static GSVector4i xfc000000(const GSVector4i& v) { return xffffffff(v).sll32<26>(); }
	__forceinline static GSVector4i xfe000000(const GSVector4i& v) { return xffffffff(v).sll32<25>(); }
	__forceinline static GSVector4i xff000000(const GSVector4i& v) { return xffffffff(v).sll32<24>(); }
	__forceinline static GSVector4i xff800000(const GSVector4i& v) { return xffffffff(v).sll32<23>(); }
	__forceinline static GSVector4i xffc00000(const GSVector4i& v) { return xffffffff(v).sll32<22>(); }
	__forceinline static GSVector4i xffe00000(const GSVector4i& v) { return xffffffff(v).sll32<21>(); }
	__forceinline static GSVector4i xfff00000(const GSVector4i& v) { return xffffffff(v).sll32<20>(); }
	__forceinline static GSVector4i xfff80000(const GSVector4i& v) { return xffffffff(v).sll32<19>(); }
	__forceinline static GSVector4i xfffc0000(const GSVector4i& v) { return xffffffff(v).sll32<18>(); }
	__forceinline static GSVector4i xfffe0000(const GSVector4i& v) { return xffffffff(v).sll32<17>(); }
	__forceinline static GSVector4i xffff0000(const GSVector4i& v) { return xffffffff(v).sll32<16>(); }
	__forceinline static GSVector4i xffff8000(const GSVector4i& v) { return xffffffff(v).sll32<15>(); }
	__forceinline static GSVector4i xffffc000(const GSVector4i& v) { return xffffffff(v).sll32<14>(); }
	__forceinline static GSVector4i xffffe000(const GSVector4i& v) { return xffffffff(v).sll32<13>(); }
	__forceinline static GSVector4i xfffff000(const GSVector4i& v) { return xffffffff(v).sll32<12>(); }
	__forceinline static GSVector4i xfffff800(const GSVector4i& v) { return xffffffff(v).sll32<11>(); }
	__forceinline static GSVector4i xfffffc00(const GSVector4i& v) { return xffffffff(v).sll32<10>(); }
	__forceinline static GSVector4i xfffffe00(const GSVector4i& v) { return xffffffff(v).sll32< 9>(); }
	__forceinline static GSVector4i xffffff00(const GSVector4i& v) { return xffffffff(v).sll32< 8>(); }
	__forceinline static GSVector4i xffffff80(const GSVector4i& v) { return xffffffff(v).sll32< 7>(); }
	__forceinline static GSVector4i xffffffc0(const GSVector4i& v) { return xffffffff(v).sll32< 6>(); }
	__forceinline static GSVector4i xffffffe0(const GSVector4i& v) { return xffffffff(v).sll32< 5>(); }
	__forceinline static GSVector4i xfffffff0(const GSVector4i& v) { return xffffffff(v).sll32< 4>(); }
	__forceinline static GSVector4i xfffffff8(const GSVector4i& v) { return xffffffff(v).sll32< 3>(); }
	__forceinline static GSVector4i xfffffffc(const GSVector4i& v) { return xffffffff(v).sll32< 2>(); }
	__forceinline static GSVector4i xfffffffe(const GSVector4i& v) { return xffffffff(v).sll32< 1>(); }

	__forceinline static GSVector4i x0001(const GSVector4i& v) { return xffffffff(v).srl16<15>(); }
	__forceinline static GSVector4i x0003(const GSVector4i& v) { return xffffffff(v).srl16<14>(); }
	__forceinline static GSVector4i x0007(const GSVector4i& v) { return xffffffff(v).srl16<13>(); }
	__forceinline static GSVector4i x000f(const GSVector4i& v) { return xffffffff(v).srl16<12>(); }
	__forceinline static GSVector4i x001f(const GSVector4i& v) { return xffffffff(v).srl16<11>(); }
	__forceinline static GSVector4i x003f(const GSVector4i& v) { return xffffffff(v).srl16<10>(); }
	__forceinline static GSVector4i x007f(const GSVector4i& v) { return xffffffff(v).srl16< 9>(); }
	__forceinline static GSVector4i x00ff(const GSVector4i& v) { return xffffffff(v).srl16< 8>(); }
	__forceinline static GSVector4i x01ff(const GSVector4i& v) { return xffffffff(v).srl16< 7>(); }
	__forceinline static GSVector4i x03ff(const GSVector4i& v) { return xffffffff(v).srl16< 6>(); }
	__forceinline static GSVector4i x07ff(const GSVector4i& v) { return xffffffff(v).srl16< 5>(); }
	__forceinline static GSVector4i x0fff(const GSVector4i& v) { return xffffffff(v).srl16< 4>(); }
	__forceinline static GSVector4i x1fff(const GSVector4i& v) { return xffffffff(v).srl16< 3>(); }
	__forceinline static GSVector4i x3fff(const GSVector4i& v) { return xffffffff(v).srl16< 2>(); }
	__forceinline static GSVector4i x7fff(const GSVector4i& v) { return xffffffff(v).srl16< 1>(); }

	__forceinline static GSVector4i x8000(const GSVector4i& v) { return xffffffff(v).sll16<15>(); }
	__forceinline static GSVector4i xc000(const GSVector4i& v) { return xffffffff(v).sll16<14>(); }
	__forceinline static GSVector4i xe000(const GSVector4i& v) { return xffffffff(v).sll16<13>(); }
	__forceinline static GSVector4i xf000(const GSVector4i& v) { return xffffffff(v).sll16<12>(); }
	__forceinline static GSVector4i xf800(const GSVector4i& v) { return xffffffff(v).sll16<11>(); }
	__forceinline static GSVector4i xfc00(const GSVector4i& v) { return xffffffff(v).sll16<10>(); }
	__forceinline static GSVector4i xfe00(const GSVector4i& v) { return xffffffff(v).sll16< 9>(); }
	__forceinline static GSVector4i xff00(const GSVector4i& v) { return xffffffff(v).sll16< 8>(); }
	__forceinline static GSVector4i xff80(const GSVector4i& v) { return xffffffff(v).sll16< 7>(); }
	__forceinline static GSVector4i xffc0(const GSVector4i& v) { return xffffffff(v).sll16< 6>(); }
	__forceinline static GSVector4i xffe0(const GSVector4i& v) { return xffffffff(v).sll16< 5>(); }
	__forceinline static GSVector4i xfff0(const GSVector4i& v) { return xffffffff(v).sll16< 4>(); }
	__forceinline static GSVector4i xfff8(const GSVector4i& v) { return xffffffff(v).sll16< 3>(); }
	__forceinline static GSVector4i xfffc(const GSVector4i& v) { return xffffffff(v).sll16< 2>(); }
	__forceinline static GSVector4i xfffe(const GSVector4i& v) { return xffffffff(v).sll16< 1>(); }

	__forceinline static GSVector4i xff(int n) { return m_xff[n]; }
	__forceinline static GSVector4i x0f(int n) { return m_x0f[n]; }
};
