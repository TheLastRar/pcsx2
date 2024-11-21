// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <vector>

class PS2Float
{
	struct BoothRecode
	{
		u32 data;
		u32 negate;
	};

	struct AddResult
	{
		u32 lo;
		u32 hi;
	};

    static u64 MulMantissa(u32 a, u32 b);

    static BoothRecode Booth(u32 a, u32 b, u32 bit);

	static AddResult Add3(u32 a, u32 b, u32 c);

public:
    bool Sign;
    u8 Exponent;
    u32 Mantissa;

    static const u8 BIAS;
    static const u32 SIGNMASK;
    static const u32 MAX_FLOATING_POINT_VALUE;
    static const u32 MIN_FLOATING_POINT_VALUE;
    static const u32 POSITIVE_INFINITY_VALUE;
    static const u32 NEGATIVE_INFINITY_VALUE;
    static const u32 ONE;
    static const u32 MIN_ONE;
    static const int IMPLICIT_LEADING_BIT_POS;

    static const s8 msb[256];
    static const s32 debruijn32[64];
    static const s32 normalizeAmounts[];

    PS2Float(u32 value);

    PS2Float(bool sign, u8 exponent, u32 mantissa);

    static PS2Float Max();

    static PS2Float Min();

    static PS2Float One();

    static PS2Float MinOne();

	static PS2Float Neg(PS2Float self);

    u32 AsUInt32() const;

    PS2Float Add(PS2Float addend);

    PS2Float Sub(PS2Float subtrahend);

    PS2Float Mul(PS2Float mulend);

    PS2Float Div(PS2Float divend);

    PS2Float Sqrt();

    PS2Float Rsqrt(PS2Float other);

    bool IsDenormalized();

    bool IsAbnormal();

    bool IsZero();

    u32 Abs();

    PS2Float RoundTowardsZero();

    s32 CompareTo(PS2Float other);

    s32 CompareOperand(PS2Float other);

    double ToDouble();

    std::string ToString();

protected:

private:

    PS2Float DoAdd(PS2Float other);

    PS2Float DoMul(PS2Float other);

    PS2Float DoDiv(PS2Float other);

    static PS2Float SolveAbnormalAdditionOrSubtractionOperation(PS2Float a, PS2Float b, bool add);

    static PS2Float SolveAbnormalMultiplicationOrDivisionOperation(PS2Float a, PS2Float b, bool mul);

    static PS2Float SolveAddSubDenormalizedOperation(PS2Float a, PS2Float b, bool add);

    static PS2Float SolveMultiplicationDenormalizedOperation(PS2Float a, PS2Float b);

    static PS2Float SolveDivisionDenormalizedOperation(PS2Float a, PS2Float b);

    static bool DetermineMultiplicationDivisionOperationSign(PS2Float a, PS2Float b);

    static bool DetermineAdditionOperationSign(PS2Float a, PS2Float b);

    static bool DetermineSubtractionOperationSign(PS2Float a, PS2Float b);

    static s32 GetMostSignificantBitPosition(u32 value);

    static s32 BitScanReverse8(s32 b);

    static s32 clz(s32 x);
};
