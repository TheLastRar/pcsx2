// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include <algorithm>

class alignas(16) GSVector4
{
	struct cxpr_init_tag {};
	static constexpr cxpr_init_tag cxpr_init{};

	constexpr GSVector4(cxpr_init_tag, float x, float y, float z, float w)
		: F32{x, y, z, w}
	{
	}

	constexpr GSVector4(cxpr_init_tag, int x, int y, int z, int w)
		: I32{x, y, z, w}
	{
	}

	constexpr GSVector4(cxpr_init_tag, u64 x, u64 y)
		: U64{x, y}
	{
	}

public:
	union
	{
		struct { float x, y, z, w; };
		struct { float r, g, b, a; };
		struct { float left, top, right, bottom; };
		float v[4];
		float F32[4];
		double F64[2];
		s8  I8[16];
		s16 I16[8];
		s32 I32[4];
		s64 I64[2];
		u8  U8[16];
		u16 U16[8];
		u32 U32[4];
		u64 U64[2];
	};

	static const GSVector4 m_ps0123;
	static const GSVector4 m_ps4567;
	static const GSVector4 m_half;
	static const GSVector4 m_one;
	static const GSVector4 m_two;
	static const GSVector4 m_four;
	static const GSVector4 m_x4b000000;
	static const GSVector4 m_x4f800000;
	static const GSVector4 m_xc1e00000000fffff;
	static const GSVector4 m_max;
	static const GSVector4 m_min;

	GSVector4() = default;

	constexpr static GSVector4 cxpr(float x, float y, float z, float w)
	{
		return GSVector4(cxpr_init, x, y, z, w);
	}

	constexpr static GSVector4 cxpr(float x)
	{
		return GSVector4(cxpr_init, x, x, x, x);
	}

	constexpr static GSVector4 cxpr(int x, int y, int z, int w)
	{
		return GSVector4(cxpr_init, x, y, z, w);
	}

	constexpr static GSVector4 cxpr(int x)
	{
		return GSVector4(cxpr_init, x, x, x, x);
	}

	constexpr static GSVector4 cxpr64(u64 x, u64 y)
	{
		return GSVector4(cxpr_init, x, y);
	}

	constexpr static GSVector4 cxpr64(u64 x)
	{
		return GSVector4(cxpr_init, x, x);
	}

	__forceinline GSVector4(float x, float y, float z, float w)
		: F32{x, y, z, w}
	{
	}

	__forceinline GSVector4(float x, float y)
		: F32{x, y, 0, 0}
	{
	}

	__forceinline GSVector4(int x, int y, int z, int w)
		: F32{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), static_cast<float>(w)}
	{
	}

	__forceinline GSVector4(int x, int y)
		: F32{static_cast<float>(x), static_cast<float>(y), 0, 0}
	{
	}

	__forceinline explicit GSVector4(const GSVector2& v)
		: F32{v.x, v.y, 0, 0}
	{
	}

	__forceinline explicit GSVector4(const GSVector2i& v)
		: F32{static_cast<float>(v.x), static_cast<float>(v.y), 0, 0}
	{
	}

#ifndef _M_ARM64
	__forceinline explicit GSVector4(__m128 m)
		: I32{_mm_extract_ps(m, 0), _mm_extract_ps(m, 1), _mm_extract_ps(m, 2), _mm_extract_ps(m, 3)}
	{
	}
#else
	__forceinline explicit GSVector4(int32x4_t m)
		: F32{vgetq_lane_f32(m, 0), vgetq_lane_f32(m, 1), vgetq_lane_f32(m, 2), vgetq_lane_f32(m, 3)}
	{
	}
#endif
	/*
	__forceinline explicit GSVector4(__m128d m)
		: m(_mm_castpd_ps(m))
	{
	}
	*/

	__forceinline explicit GSVector4(float f)
		: F32{f, f, f, f}
	{
	}

	__forceinline explicit GSVector4(int i)
		: F32{static_cast<float>(i), static_cast<float>(i), static_cast<float>(i), static_cast<float>(i)}
	{
	}

	__forceinline explicit GSVector4(u32 u)
	{
		GSVector4i v((int)u);

		*this = GSVector4(v) + (m_x4f800000 & GSVector4::cast(v.sra32<31>()));
	}

	__forceinline explicit GSVector4(const GSVector4i& v);

	__forceinline static GSVector4 cast(const GSVector4i& v);

	__forceinline static GSVector4 f64(double x, double y)
	{	
		return GSVector4(reinterpret_cast<float*>(&x)[0], reinterpret_cast<float*>(&x)[1], reinterpret_cast<float*>(&y)[0], reinterpret_cast<float*>(&y)[1]);
	}

	__forceinline void operator=(float f)
	{
		F32[0] = f;
		F32[1] = f;
		F32[2] = f;
		F32[3] = f;
	}
	/*
	__forceinline void operator=(__m128 m)
	{
		this->m = m;
	}
	*/
#ifndef _M_ARM64
	__forceinline operator __m128() const
	{
		return _mm_load_ps(F32);
	}
#else
	__forceinline operator float32x4_t() const
	{
		return vld1q_f32(F32);
	}
#endif

	__forceinline GSVector4 noopt()
	{
		return *this;
	}

	__forceinline u32 rgba32() const
	{
		return GSVector4i(*this).rgba32();
	}

	__forceinline static GSVector4 rgba32(u32 rgba)
	{
		return GSVector4(GSVector4i::load((int)rgba).u8to32());
	}

	__forceinline static GSVector4 rgba32(u32 rgba, int shift)
	{
		return GSVector4(GSVector4i::load((int)rgba).u8to32() << shift);
	}

	__forceinline static GSVector4 unorm8(u32 rgba)
	{
		return rgba32(rgba) * GSVector4::cxpr(1.0f / 255.0f);
	}

	__forceinline GSVector4 abs() const
	{
		return GSVector4(
			std::abs(F32[0]),
			std::abs(F32[1]),
			std::abs(F32[2]),
			std::abs(F32[3]));
	}

	__forceinline GSVector4 neg() const
	{
		return GSVector4(
			-F32[0],
			-F32[1],
			-F32[2],
			-F32[3]);
	}

	__forceinline GSVector4 rcp() const
	{
		return GSVector4(
			1.0f / F32[0],
			1.0f / F32[1],
			1.0f / F32[2],
			1.0f / F32[3]);
	}

	__forceinline GSVector4 rcpnr() const
	{
		GSVector4 v = rcp();
		/*
		return GSVector4(
			v.F32[0] * (2.0f - v.F32[0] * F32[0]),
			v.F32[1] * (2.0f - v.F32[1] * F32[1]),
			v.F32[2] * (2.0f - v.F32[2] * F32[2]),
			v.F32[3] * (2.0f - v.F32[3] * F32[3]));
		*/

		return (v + v) - (v * v) * *this;
	}

	template <int mode>
	__forceinline GSVector4 round() const
	{
		if constexpr (mode == Round_NegInf)
			return GSVector4(
				std::floor(F32[0]),
				std::floor(F32[1]),
				std::floor(F32[2]),
				std::floor(F32[3]));
		else if constexpr (mode == Round_PosInf)
			return GSVector4(
				std::ceil(F32[0]),
				std::ceil(F32[1]),
				std::ceil(F32[2]),
				std::ceil(F32[3]));
		else if constexpr (mode == Round_NearestInt)
			return GSVector4(
				std::round(F32[0]),
				std::round(F32[1]),
				std::round(F32[2]),
				std::round(F32[3]));
		else
			return GSVector4(
				std::trunc(F32[0]),
				std::trunc(F32[1]),
				std::trunc(F32[2]),
				std::trunc(F32[3]));
	}

	__forceinline GSVector4 floor() const
	{
		return GSVector4(
			std::floor(F32[0]),
			std::floor(F32[1]),
			std::floor(F32[2]),
			std::floor(F32[3]));
	}

	__forceinline GSVector4 ceil() const
	{
		return GSVector4(
			std::ceil(F32[0]),
			std::ceil(F32[1]),
			std::ceil(F32[2]),
			std::ceil(F32[3]));
	}

	// http://jrfonseca.blogspot.com/2008/09/fast-sse2-pow-tables-or-polynomials.html

#define LOG_POLY0(x, c0) GSVector4(c0)
#define LOG_POLY1(x, c0, c1) (LOG_POLY0(x, c1).madd(x, GSVector4(c0)))
#define LOG_POLY2(x, c0, c1, c2) (LOG_POLY1(x, c1, c2).madd(x, GSVector4(c0)))
#define LOG_POLY3(x, c0, c1, c2, c3) (LOG_POLY2(x, c1, c2, c3).madd(x, GSVector4(c0)))
#define LOG_POLY4(x, c0, c1, c2, c3, c4) (LOG_POLY3(x, c1, c2, c3, c4).madd(x, GSVector4(c0)))
#define LOG_POLY5(x, c0, c1, c2, c3, c4, c5) (LOG_POLY4(x, c1, c2, c3, c4, c5).madd(x, GSVector4(c0)))

	__forceinline GSVector4 log2(int precision = 5) const
	{
		// NOTE: sign bit ignored, safe to pass negative numbers

		// The idea behind this algorithm is to split the float into two parts, log2(m * 2^e) => log2(m) + log2(2^e) => log2(m) + e,
		// and then approximate the logarithm of the mantissa (it's 1.x when normalized, a nice short range).

		GSVector4 one = m_one;

		GSVector4i i = GSVector4i::cast(*this);

		GSVector4 e = GSVector4(((i << 1) >> 24) - GSVector4i::x0000007f());
		GSVector4 m = GSVector4::cast((i << 9) >> 9) | one;

		GSVector4 p;

		// Minimax polynomial fit of log2(x)/(x - 1), for x in range [1, 2[

		switch (precision)
		{
			case 3:
				p = LOG_POLY2(m, 2.28330284476918490682f, -1.04913055217340124191f, 0.204446009836232697516f);
				break;
			case 4:
				p = LOG_POLY3(m, 2.61761038894603480148f, -1.75647175389045657003f, 0.688243882994381274313f, -0.107254423828329604454f);
				break;
			default:
			case 5:
				p = LOG_POLY4(m, 2.8882704548164776201f, -2.52074962577807006663f, 1.48116647521213171641f, -0.465725644288844778798f, 0.0596515482674574969533f);
				break;
			case 6:
				p = LOG_POLY5(m, 3.1157899f, -3.3241990f, 2.5988452f, -1.2315303f, 3.1821337e-1f, -3.4436006e-2f);
				break;
		}

		// This effectively increases the polynomial degree by one, but ensures that log2(1) == 0

		p = p * (m - one);

		return p + e;
	}

	__forceinline GSVector4 madd(const GSVector4& a, const GSVector4& b) const
	{
		return *this * a + b;
	}

	__forceinline GSVector4 msub(const GSVector4& a, const GSVector4& b) const
	{
		return *this * a - b;
	}

	__forceinline GSVector4 nmadd(const GSVector4& a, const GSVector4& b) const
	{
		return b - *this * a;
	}

	__forceinline GSVector4 nmsub(const GSVector4& a, const GSVector4& b) const
	{
		return -b - *this * a;
	}

	__forceinline GSVector4 addm(const GSVector4& a, const GSVector4& b) const
	{
		return a.madd(b, *this); // *this + a * b
	}

	__forceinline GSVector4 subm(const GSVector4& a, const GSVector4& b) const
	{
		return a.nmadd(b, *this); // *this - a * b
	}

	__forceinline GSVector4 hadd() const
	{
		return GSVector4(
			F32[0] + F32[1],
			F32[2] + F32[3],
			F32[0] + F32[1],
			F32[2] + F32[3]);
	}

	__forceinline GSVector4 hadd(const GSVector4& v) const
	{
		return GSVector4(
			F32[0] + F32[1],
			F32[2] + F32[3],
			v.F32[0] + v.F32[1],
			v.F32[2] + v.F32[3]);
	}

	__forceinline GSVector4 hsub() const
	{
		return GSVector4(
			F32[0] - F32[1],
			F32[2] - F32[3],
			F32[0] - F32[1],
			F32[2] - F32[3]);
	}

	__forceinline GSVector4 hsub(const GSVector4& v) const
	{
		return GSVector4(
			F32[0] - F32[1],
			F32[2] - F32[3],
			v.F32[0] - v.F32[1],
			v.F32[2] - v.F32[3]);
	}

	/*
	template <int i>
	__forceinline GSVector4 dp(const GSVector4& v) const
	{
		return GSVector4(_mm_dp_ps(m, v.m, i));
	}
	*/

	__forceinline GSVector4 sat(const GSVector4& a, const GSVector4& b) const
	{
		return max(a).min(b);
	}

	__forceinline GSVector4 sat(const GSVector4& a) const
	{
		const GSVector4 minv(a.x);
		const GSVector4 maxv(a.y);
		return sat(minv, maxv);
	}

	__forceinline GSVector4 sat(const float scale = 255) const
	{
		return sat(zero(), GSVector4(scale));
	}

	__forceinline GSVector4 clamp(const float scale = 255) const
	{
		return min(GSVector4(scale));
	}

	__forceinline GSVector4 min(const GSVector4& a) const
	{
		return GSVector4(
			std::min(F32[0], a.F32[0]),
			std::min(F32[1], a.F32[1]),
			std::min(F32[2], a.F32[2]),
			std::min(F32[3], a.F32[3]));
	}

	__forceinline GSVector4 max(const GSVector4& a) const
	{
		return GSVector4(
			std::max(F32[0], a.F32[0]),
			std::max(F32[1], a.F32[1]),
			std::max(F32[2], a.F32[2]),
			std::max(F32[3], a.F32[3]));
	}

	template <int mask>
	__forceinline GSVector4 blend32(const GSVector4& a) const
	{
		return GSVector4(
			(mask & 1) ? a.F32[0] : F32[0],
			(mask & 2) ? a.F32[1] : F32[1],
			(mask & 4) ? a.F32[2] : F32[2],
			(mask & 8) ? a.F32[3] : F32[3]);
	}

	__forceinline GSVector4 blend32(const GSVector4& a, const GSVector4& mask) const
	{
		float ret[4];

		for (int i = 0; i < 4; i++)
		{
			u32 maskB = mask.I32[i] >> 31;
			ret[i] = std::bit_cast<float>(U32[i] ^ ((U32[i] ^ a.U32[i]) & maskB));
		}
		return GSVector4(ret[0], ret[1], ret[2], ret[3]);
	}

	__forceinline GSVector4 upl(const GSVector4& a) const
	{
		return GSVector4(F32[0], a.F32[0], F32[1], a.F32[1]);
	}

	__forceinline GSVector4 uph(const GSVector4& a) const
	{
		return GSVector4(F32[2], a.F32[2], F32[3], a.F32[3]);
	}

	__forceinline GSVector4 upld(const GSVector4& a) const
	{
		return GSVector4(F32[0], F32[1], a.F32[0], a.F32[1]);
	}

	__forceinline GSVector4 uphd(const GSVector4& a) const
	{
		return GSVector4(F32[2], F32[3], a.F32[2], a.F32[3]);
	}

	__forceinline GSVector4 l2h(const GSVector4& a) const
	{
		return GSVector4(
			F32[0],
			F32[1],
			F32[0],
			F32[1]);
	}

	__forceinline GSVector4 h2l(const GSVector4& a) const
	{
		return GSVector4(
			F32[2],
			F32[3],
			F32[2],
			F32[3]);
	}

	__forceinline GSVector4 andnot(const GSVector4& v) const
	{
		return GSVector4(
			std::bit_cast<float>(I32[0] & ~v.I32[0]),
			std::bit_cast<float>(I32[1] & ~v.I32[1]),
			std::bit_cast<float>(I32[2] & ~v.I32[2]),
			std::bit_cast<float>(I32[3] & ~v.I32[3]));
	}

	__forceinline int mask() const
	{
		return ((U32[0] >> 31) << 0) |
			   ((U32[1] >> 31) << 1) |
			   ((U32[2] >> 31) << 2) |
			   ((U32[3] >> 31) << 3);
	}

	__forceinline bool alltrue() const
	{
		return mask() == 0xf;
	}

	__forceinline bool allfalse() const
	{
		return (I32[0] == 0) &&
			   (I32[1] == 0) &&
			   (I32[2] == 0) &&
			   (I32[3] == 0);
	}

	__forceinline GSVector4 replace_nan(const GSVector4& v) const
	{
		return v.blend32(*this, *this == *this);
	}

	template <int src, int dst>
	__forceinline GSVector4 insert32(const GSVector4& v) const
	{
		return GSVector4(
			dst == 0 ? v.F32[src] : F32[0],
			dst == 1 ? v.F32[src] : F32[1],
			dst == 2 ? v.F32[src] : F32[2],
			dst == 3 ? v.F32[src] : F32[3]);
	}

	template <int i>
	__forceinline int extract32() const
	{
		pxAssert(i < 4);
		return I32[i];
	}

	__forceinline static GSVector4 zero()
	{
		return GSVector4(0);
	}

	__forceinline static GSVector4 xffffffff()
	{
		return GSVector4(0xFFFFFFFF);
	}

	__forceinline static GSVector4 ps0123()
	{
		return GSVector4(m_ps0123);
	}

	__forceinline static GSVector4 ps4567()
	{
		return GSVector4(m_ps4567);
	}

	__forceinline static GSVector4 loadl(const void* p)
	{
		const float* ret = static_cast<const float*>(p);
		return GSVector4(ret[0], ret[1], 0.0f, 0.0f);
	}

	__forceinline static GSVector4 load(float f)
	{
		return GSVector4(f, 0.0f, 0.0f, 0.0f);
	}

	__forceinline static GSVector4 load(u32 u)
	{
		GSVector4i v = GSVector4i::load((int)u);

		return GSVector4(v) + (m_x4f800000 & GSVector4::cast(v.sra32<31>()));
	}

	template <bool aligned>
	__forceinline static GSVector4 load(const void* p)
	{
		const float* ret = static_cast<const float*>(p);
		return GSVector4(ret[0], ret[1], ret[2], ret[3]);
	}

	__forceinline static void storent(void* p, const GSVector4& v)
	{
		std::memcpy(p, v.U8, 16);
	}

	__forceinline static void storel(void* p, const GSVector4& v)
	{
		std::memcpy(p, v.U8, 8);
	}

	__forceinline static void storeh(void* p, const GSVector4& v)
	{
		std::memcpy(p, &v.U8[8], 8);
	}

	template <bool aligned>
	__forceinline static void store(void* p, const GSVector4& v)
	{
		std::memcpy(p, v.U8, 16);
	}

	__forceinline static void store(float* p, const GSVector4& v)
	{
		std::memcpy(p, v.U8, 16);
	}

	__forceinline static void expand(const GSVector4i& v, GSVector4& a, GSVector4& b, GSVector4& c, GSVector4& d)
	{
		GSVector4i mask = GSVector4i::x000000ff();

		a = GSVector4(v & mask);
		b = GSVector4((v >> 8) & mask);
		c = GSVector4((v >> 16) & mask);
		d = GSVector4((v >> 24));
	}

	__forceinline static void transpose(GSVector4& a, GSVector4& b, GSVector4& c, GSVector4& d)
	{
		GSVector4 v0 = a.xyxy(b);
		GSVector4 v1 = c.xyxy(d);

		GSVector4 e = v0.xzxz(v1);
		GSVector4 f = v0.ywyw(v1);

		GSVector4 v2 = a.zwzw(b);
		GSVector4 v3 = c.zwzw(d);

		GSVector4 g = v2.xzxz(v3);
		GSVector4 h = v2.ywyw(v3);

		a = e;
		b = f;
		c = g;
		d = h;
	}

	__forceinline GSVector4 operator-() const
	{
		return neg();
	}

	__forceinline void operator+=(const GSVector4& v)
	{
		F32[0] += v.F32[0];
		F32[1] += v.F32[1];
		F32[2] += v.F32[2];
		F32[3] += v.F32[3];
	}

	__forceinline void operator-=(const GSVector4& v)
	{
		F32[0] -= v.F32[0];
		F32[1] -= v.F32[1];
		F32[2] -= v.F32[2];
		F32[3] -= v.F32[3];
	}

	__forceinline void operator*=(const GSVector4& v)
	{
		F32[0] *= v.F32[0];
		F32[1] *= v.F32[1];
		F32[2] *= v.F32[2];
		F32[3] *= v.F32[3];
	}

	__forceinline void operator/=(const GSVector4& v)
	{
		F32[0] /= v.F32[0];
		F32[1] /= v.F32[1];
		F32[2] /= v.F32[2];
		F32[3] /= v.F32[3];
	}

	__forceinline void operator+=(float f)
	{
		*this += GSVector4(f);
	}

	__forceinline void operator-=(float f)
	{
		*this -= GSVector4(f);
	}

	__forceinline void operator*=(float f)
	{
		*this *= GSVector4(f);
	}

	__forceinline void operator/=(float f)
	{
		*this /= GSVector4(f);
	}

	__forceinline void operator&=(const GSVector4& v)
	{
		U32[0] &= v.U32[0];
		U32[1] &= v.U32[1];
		U32[2] &= v.U32[2];
		U32[3] &= v.U32[3];
	}

	__forceinline void operator|=(const GSVector4& v)
	{
		U32[0] |= v.U32[0];
		U32[1] |= v.U32[1];
		U32[2] |= v.U32[2];
		U32[3] |= v.U32[3];
	}

	__forceinline void operator^=(const GSVector4& v)
	{
		U32[0] ^= v.U32[0];
		U32[1] ^= v.U32[1];
		U32[2] ^= v.U32[2];
		U32[3] ^= v.U32[3];
	}

	__forceinline friend GSVector4 operator+(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(
			v1.F32[0] + v2.F32[0],
			v1.F32[1] + v2.F32[1],
			v1.F32[2] + v2.F32[2],
			v1.F32[3] + v2.F32[3]);
	}

	__forceinline friend GSVector4 operator-(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(
			v1.F32[0] - v2.F32[0],
			v1.F32[1] - v2.F32[1],
			v1.F32[2] - v2.F32[2],
			v1.F32[3] - v2.F32[3]);
	}

	__forceinline friend GSVector4 operator*(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(
			v1.F32[0] * v2.F32[0],
			v1.F32[1] * v2.F32[1],
			v1.F32[2] * v2.F32[2],
			v1.F32[3] * v2.F32[3]);
	}

	__forceinline friend GSVector4 operator/(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(
			v1.F32[0] / v2.F32[0],
			v1.F32[1] / v2.F32[1],
			v1.F32[2] / v2.F32[2],
			v1.F32[3] / v2.F32[3]);
	}

	__forceinline friend GSVector4 operator+(const GSVector4& v, float f)
	{
		return v + GSVector4(f);
	}

	__forceinline friend GSVector4 operator-(const GSVector4& v, float f)
	{
		return v - GSVector4(f);
	}

	__forceinline friend GSVector4 operator*(const GSVector4& v, float f)
	{
		return v * GSVector4(f);
	}

	__forceinline friend GSVector4 operator/(const GSVector4& v, float f)
	{
		return v / GSVector4(f);
	}

	__forceinline friend GSVector4 operator&(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(
			std::bit_cast<float>(v1.U32[0] & v2.U32[0]),
			std::bit_cast<float>(v1.U32[1] & v2.U32[1]),
			std::bit_cast<float>(v1.U32[2] & v2.U32[2]),
			std::bit_cast<float>(v1.U32[3] & v2.U32[3]));
	}

	__forceinline friend GSVector4 operator|(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(
			std::bit_cast<float>(v1.U32[0] | v2.U32[0]),
			std::bit_cast<float>(v1.U32[1] | v2.U32[1]),
			std::bit_cast<float>(v1.U32[2] | v2.U32[2]),
			std::bit_cast<float>(v1.U32[3] | v2.U32[3]));
	}

	__forceinline friend GSVector4 operator^(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(
			std::bit_cast<float>(v1.U32[0] ^ v2.U32[0]),
			std::bit_cast<float>(v1.U32[1] ^ v2.U32[1]),
			std::bit_cast<float>(v1.U32[2] ^ v2.U32[2]),
			std::bit_cast<float>(v1.U32[3] ^ v2.U32[3]));
	}

	__forceinline friend GSVector4 operator==(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(
			std::bit_cast<float>(v1.F32[0] == v2.F32[0] ? 0xFFFFFFFF : 0),
			std::bit_cast<float>(v1.F32[1] == v2.F32[1] ? 0xFFFFFFFF : 0),
			std::bit_cast<float>(v1.F32[2] == v2.F32[2] ? 0xFFFFFFFF : 0),
			std::bit_cast<float>(v1.F32[3] == v2.F32[3] ? 0xFFFFFFFF : 0));
	}

	__forceinline friend GSVector4 operator!=(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(
			std::bit_cast<float>(v1.F32[0] != v2.F32[0] ? 0xFFFFFFFF : 0),
			std::bit_cast<float>(v1.F32[1] != v2.F32[1] ? 0xFFFFFFFF : 0),
			std::bit_cast<float>(v1.F32[2] != v2.F32[2] ? 0xFFFFFFFF : 0),
			std::bit_cast<float>(v1.F32[3] != v2.F32[3] ? 0xFFFFFFFF : 0));
	}

	__forceinline friend GSVector4 operator>(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(
			std::bit_cast<float>(v1.F32[0] > v2.F32[0] ? 0xFFFFFFFF : 0),
			std::bit_cast<float>(v1.F32[1] > v2.F32[1] ? 0xFFFFFFFF : 0),
			std::bit_cast<float>(v1.F32[2] > v2.F32[2] ? 0xFFFFFFFF : 0),
			std::bit_cast<float>(v1.F32[3] > v2.F32[3] ? 0xFFFFFFFF : 0));
	}

	__forceinline friend GSVector4 operator<(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(
			std::bit_cast<float>(v1.F32[0] < v2.F32[0] ? 0xFFFFFFFF : 0),
			std::bit_cast<float>(v1.F32[1] < v2.F32[1] ? 0xFFFFFFFF : 0),
			std::bit_cast<float>(v1.F32[2] < v2.F32[2] ? 0xFFFFFFFF : 0),
			std::bit_cast<float>(v1.F32[3] < v2.F32[3] ? 0xFFFFFFFF : 0));
	}

	__forceinline friend GSVector4 operator>=(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(
			std::bit_cast<float>(v1.F32[0] >= v2.F32[0] ? 0xFFFFFFFF : 0),
			std::bit_cast<float>(v1.F32[1] >= v2.F32[1] ? 0xFFFFFFFF : 0),
			std::bit_cast<float>(v1.F32[2] >= v2.F32[2] ? 0xFFFFFFFF : 0),
			std::bit_cast<float>(v1.F32[3] >= v2.F32[3] ? 0xFFFFFFFF : 0));
	}

	__forceinline friend GSVector4 operator<=(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(
			std::bit_cast<float>(v1.F32[0] <= v2.F32[0] ? 0xFFFFFFFF : 0),
			std::bit_cast<float>(v1.F32[1] <= v2.F32[1] ? 0xFFFFFFFF : 0),
			std::bit_cast<float>(v1.F32[2] <= v2.F32[2] ? 0xFFFFFFFF : 0),
			std::bit_cast<float>(v1.F32[3] <= v2.F32[3] ? 0xFFFFFFFF : 0));
	}

	__forceinline GSVector4 mul64(const GSVector4& v) const
	{
		double lo = F64[0] * v.F64[0];
		double hi = F64[1] * v.F64[1];
		return GSVector4(reinterpret_cast<float*>(&lo)[0], reinterpret_cast<float*>(&lo)[1], reinterpret_cast<float*>(&hi)[0], reinterpret_cast<float*>(&hi)[1]);
	}

	__forceinline GSVector4 add64(const GSVector4& v) const
	{
		double lo = F64[0] + v.F64[0];
		double hi = F64[1] + v.F64[1];
		return GSVector4(reinterpret_cast<float*>(&lo)[0], reinterpret_cast<float*>(&lo)[1], reinterpret_cast<float*>(&hi)[0], reinterpret_cast<float*>(&hi)[1]);
	}

	__forceinline GSVector4 sub64(const GSVector4& v) const
	{
		double lo = F64[0] - v.F64[0];
		double hi = F64[1] - v.F64[1];
		return GSVector4(reinterpret_cast<float*>(&lo)[0], reinterpret_cast<float*>(&lo)[1], reinterpret_cast<float*>(&hi)[0], reinterpret_cast<float*>(&hi)[1]);
	}

	__forceinline static GSVector4 f32to64(const GSVector4& v)
	{
		double lo = v.F64[0];
		double hi = v.F64[1];
		return GSVector4(reinterpret_cast<float*>(&lo)[0], reinterpret_cast<float*>(&lo)[1], reinterpret_cast<float*>(&hi)[0], reinterpret_cast<float*>(&hi)[1]);
	}

	__forceinline static GSVector4 f32to64(const void* p)
	{
		const float* f32 = static_cast<const float*>(p);
		double lo = f32[0];
		double hi = f32[1];
		return GSVector4(reinterpret_cast<float*>(&lo)[0], reinterpret_cast<float*>(&lo)[1], reinterpret_cast<float*>(&hi)[0], reinterpret_cast<float*>(&hi)[1]);
	}

	__forceinline GSVector4i f64toi32(bool truncate = true) const
	{
		// Emulate x86 overflow behaviour
		s64 I64_0 = static_cast<s64>(std::clamp<double>(truncate ? std::trunc(F64[0]) : std::round(F64[0]), INT32_MIN, INT32_MAX + 1i64));
		s64 I64_1 = static_cast<s64>(std::clamp<double>(truncate ? std::trunc(F64[1]) : std::round(F64[1]), INT32_MIN, INT32_MAX + 1i64));

		return GSVector4i(
			static_cast<s32>(static_cast<s32>(I64_0)),
			static_cast<s32>(static_cast<s32>(I64_1)));
	}

	// clang-format off

	#define VECTOR4_SHUFFLE_4(xs, xn, ys, yn, zs, zn, ws, wn) \
		__forceinline GSVector4 xs##ys##zs##ws() const { \
			return GSVector4( \
				F32[xn], \
				F32[yn], \
				F32[zn], \
				F32[wn]); \
		} \
		__forceinline GSVector4 xs##ys##zs##ws(const GSVector4& v) const { \
			return GSVector4( \
				F32[xn], \
				F32[yn], \
				v.F32[zn], \
				v.F32[wn]); \
		} \

	#define VECTOR4_SHUFFLE_3(xs, xn, ys, yn, zs, zn) \
		VECTOR4_SHUFFLE_4(xs, xn, ys, yn, zs, zn, x, 0) \
		VECTOR4_SHUFFLE_4(xs, xn, ys, yn, zs, zn, y, 1) \
		VECTOR4_SHUFFLE_4(xs, xn, ys, yn, zs, zn, z, 2) \
		VECTOR4_SHUFFLE_4(xs, xn, ys, yn, zs, zn, w, 3) \

	#define VECTOR4_SHUFFLE_2(xs, xn, ys, yn) \
		VECTOR4_SHUFFLE_3(xs, xn, ys, yn, x, 0) \
		VECTOR4_SHUFFLE_3(xs, xn, ys, yn, y, 1) \
		VECTOR4_SHUFFLE_3(xs, xn, ys, yn, z, 2) \
		VECTOR4_SHUFFLE_3(xs, xn, ys, yn, w, 3) \

	#define VECTOR4_SHUFFLE_1(xs, xn) \
		VECTOR4_SHUFFLE_2(xs, xn, x, 0) \
		VECTOR4_SHUFFLE_2(xs, xn, y, 1) \
		VECTOR4_SHUFFLE_2(xs, xn, z, 2) \
		VECTOR4_SHUFFLE_2(xs, xn, w, 3) \

	VECTOR4_SHUFFLE_1(x, 0)
	VECTOR4_SHUFFLE_1(y, 1)
	VECTOR4_SHUFFLE_1(z, 2)
	VECTOR4_SHUFFLE_1(w, 3)

	// clang-format on

	__forceinline GSVector4 broadcast32() const
	{
		return GSVector4(F32[0], F32[0], F32[0], F32[0]);
	}

	__forceinline static GSVector4 broadcast32(const GSVector4& v)
	{
		return GSVector4(v.x, v.x, v.x, v.x);
	}

	__forceinline static GSVector4 broadcast32(const void* f)
	{
		return GSVector4(*static_cast<const float*>(f), *static_cast<const float*>(f), *static_cast<const float*>(f), *static_cast<const float*>(f));
	}

	__forceinline static GSVector4 broadcast64(const void* d)
	{
		return GSVector4(static_cast<const float*>(d)[0], static_cast<const float*>(d)[1], static_cast<const float*>(d)[0], static_cast<const float*>(d)[1]);
	}
};
