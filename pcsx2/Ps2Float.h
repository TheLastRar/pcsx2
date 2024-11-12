// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <vector>

class Ps2Float
{
public:
    bool Sign;
    uint8_t Exponent;
    uint32_t Mantissa;

    static const uint8_t BIAS;
    static const uint32_t SIGNMASK;
    static const uint32_t MAX_FLOATING_POINT_VALUE;
    static const uint32_t MIN_FLOATING_POINT_VALUE;
    static const uint32_t POSITIVE_INFINITY_VALUE;
    static const uint32_t NEGATIVE_INFINITY_VALUE;
    static const uint32_t ONE;
    static const uint32_t MIN_ONE;
    static const int IMPLICIT_LEADING_BIT_POS;

    static const int8_t msb[256];
    static const int32_t debruijn32[64];
    static const int32_t normalizeAmounts[];

    Ps2Float(uint32_t value);

    Ps2Float(bool sign, uint8_t exponent, uint32_t mantissa);

    static Ps2Float Max();

    static Ps2Float Min();

    static Ps2Float One();

    static Ps2Float MinOne();

	static Ps2Float Neg(Ps2Float self);

    uint32_t AsUInt32() const;

    Ps2Float Add(Ps2Float addend, bool COP1);

    Ps2Float Sub(Ps2Float subtrahend, bool COP1);

    Ps2Float Mul(Ps2Float mulend);

    Ps2Float Div(Ps2Float divend);

    Ps2Float Sqrt();

    Ps2Float Rsqrt(Ps2Float other);

    bool IsDenormalized();

    bool IsAbnormal();

    bool IsZero();

    uint32_t Abs();

    Ps2Float RoundTowardsZero();

    int32_t CompareTo(Ps2Float other);

    double ToDouble();

    std::string ToString();

protected:

private:

    Ps2Float DoAdd(Ps2Float other);

    Ps2Float DoMul(Ps2Float other);

    Ps2Float DoDiv(Ps2Float other);

    static Ps2Float SolveAbnormalAdditionOrSubtractionOperation(Ps2Float a, Ps2Float b, bool add, bool COP1);

    static Ps2Float SolveAbnormalMultiplicationOrDivisionOperation(Ps2Float a, Ps2Float b, bool mul);

    static Ps2Float SolveAddSubDenormalizedOperation(Ps2Float a, Ps2Float b, bool add);

    static Ps2Float SolveMultiplicationDenormalizedOperation(Ps2Float a, Ps2Float b);

    static Ps2Float SolveDivisionDenormalizedOperation(Ps2Float a, Ps2Float b);

    static bool DetermineMultiplicationDivisionOperationSign(Ps2Float a, Ps2Float b);

    static bool DetermineAdditionOperationSign(Ps2Float a, Ps2Float b);

    static bool DetermineSubtractionOperationSign(Ps2Float a, Ps2Float b);

    static int32_t GetMostSignificantBitPosition(uint32_t value);

    static int32_t BitScanReverse8(int32_t b);

    static int32_t clz(int32_t x);
};
