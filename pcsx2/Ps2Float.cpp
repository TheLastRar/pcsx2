// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include <stdexcept>
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <bit>
#include "Ps2Float.h"
#include "BoothMultiplier.h"
#include "Common.h"

const uint8_t Ps2Float::BIAS = 127;
const uint32_t Ps2Float::SIGNMASK = 0x80000000;
const uint32_t Ps2Float::MAX_FLOATING_POINT_VALUE = 0x7FFFFFFF;
const uint32_t Ps2Float::MIN_FLOATING_POINT_VALUE = 0xFFFFFFFF;
const uint32_t Ps2Float::POSITIVE_INFINITY_VALUE = 0x7F800000;
const uint32_t Ps2Float::NEGATIVE_INFINITY_VALUE = 0xFF800000;
const uint32_t Ps2Float::ONE = 0x3F800000;
const uint32_t Ps2Float::MIN_ONE = 0xBF800000;
const int32_t Ps2Float::IMPLICIT_LEADING_BIT_POS = 23;

Ps2Float::Ps2Float(uint32_t value)
	: Sign((value >> 31) & 1)
	, Exponent((uint8_t)(((value >> 23) & 0xFF)))
	, Mantissa(value & 0x7FFFFF)
{
}

Ps2Float::Ps2Float(bool sign, uint8_t exponent, uint32_t mantissa)
	: Sign(sign)
	, Exponent(exponent)
	, Mantissa(mantissa)
{
}

Ps2Float Ps2Float::Max()
{
	return Ps2Float(MAX_FLOATING_POINT_VALUE);
}

Ps2Float Ps2Float::Min()
{
	return Ps2Float(MIN_FLOATING_POINT_VALUE);
}

Ps2Float Ps2Float::One()
{
	return Ps2Float(ONE);
}

Ps2Float Ps2Float::MinOne()
{
	return Ps2Float(MIN_ONE);
}

uint32_t Ps2Float::AsUInt32() const
{
	uint32_t result = 0;
	result |= (Sign ? 1u : 0u) << 31;
	result |= (uint32_t)(Exponent << 23);
	result |= Mantissa;
	return result;
}

Ps2Float Ps2Float::Add(Ps2Float addend)
{
	if (IsDenormalized() || addend.IsDenormalized())
		return SolveAddSubDenormalizedOperation(*this, addend, true);

	if (IsAbnormal() && addend.IsAbnormal())
		return SolveAbnormalAdditionOrSubtractionOperation(*this, addend, true);

	uint32_t a = AsUInt32();
	uint32_t b = addend.AsUInt32();
	int32_t temp = 0;

	//exponent difference
	int exp_diff = ((a >> 23) & 0xFF) - ((b >> 23) & 0xFF);

	//diff = 25 .. 255 , expt < expd
	if (exp_diff >= 25)
	{
		b = b & SIGNMASK;
	}

	//diff = 1 .. 24, expt < expd
	else if (exp_diff > 0)
	{
		exp_diff = exp_diff - 1;
		temp = MIN_FLOATING_POINT_VALUE << exp_diff;
		b = temp & b;
	}

	//diff = -255 .. -25, expd < expt
	else if (exp_diff <= -25)
	{
		a = a & SIGNMASK;
	}

	//diff = -24 .. -1 , expd < expt
	else if (exp_diff < 0)
	{
		exp_diff = -exp_diff;
		exp_diff = exp_diff - 1;
		temp = MIN_FLOATING_POINT_VALUE << exp_diff;
		a = a & temp;
	}

	return Ps2Float(a).DoAdd(Ps2Float(b));
}

Ps2Float Ps2Float::Sub(Ps2Float subtrahend)
{
	if (IsDenormalized() || subtrahend.IsDenormalized())
		return SolveAddSubDenormalizedOperation(*this, subtrahend, false);

	if (IsAbnormal() && subtrahend.IsAbnormal())
		return SolveAbnormalAdditionOrSubtractionOperation(*this, subtrahend, false);

	uint32_t a = AsUInt32();
	uint32_t b = subtrahend.AsUInt32();
	int32_t temp = 0;

	//exponent difference
	int exp_diff = ((a >> 23) & 0xFF) - ((b >> 23) & 0xFF);

	//diff = 25 .. 255 , expt < expd
	if (exp_diff >= 25)
	{
		b = b & SIGNMASK;
	}

	//diff = 1 .. 24, expt < expd
	else if (exp_diff > 0)
	{
		exp_diff = exp_diff - 1;
		temp = MIN_FLOATING_POINT_VALUE << exp_diff;
		b = temp & b;
	}

	//diff = -255 .. -25, expd < expt
	else if (exp_diff <= -25)
	{
		a = a & SIGNMASK;
	}

	//diff = -24 .. -1 , expd < expt
	else if (exp_diff < 0)
	{
		exp_diff = -exp_diff;
		exp_diff = exp_diff - 1;
		temp = MIN_FLOATING_POINT_VALUE << exp_diff;
		a = a & temp;
	}


	return Ps2Float(a).DoAdd(Neg(Ps2Float(b)));
}

Ps2Float Ps2Float::Mul(Ps2Float mulend)
{
	if (IsDenormalized() || mulend.IsDenormalized())
		return SolveMultiplicationDenormalizedOperation(*this, mulend);

	if (IsAbnormal() && mulend.IsAbnormal())
		return SolveAbnormalMultiplicationOrDivisionOperation(*this, mulend, true);

	if (IsZero() || mulend.IsZero())
	{
		Ps2Float result = Ps2Float(0);

		result.Sign = DetermineMultiplicationDivisionOperationSign(*this, mulend);
		return result;
	}

	return DoMul(mulend);
}

Ps2Float Ps2Float::Div(Ps2Float divend)
{
	if (IsDenormalized() || divend.IsDenormalized())
		return SolveDivisionDenormalizedOperation(*this, divend);

	if (IsAbnormal() && divend.IsAbnormal())
		return SolveAbnormalMultiplicationOrDivisionOperation(*this, divend, false);

	if (IsZero())
	{
		Ps2Float result = Ps2Float(0);

		result.Sign = DetermineMultiplicationDivisionOperationSign(*this, divend);
		return result;
	}
	else if (divend.IsZero())
		return DetermineMultiplicationDivisionOperationSign(*this, divend) ? Min() : Max();

	return DoDiv(divend);
}

Ps2Float Ps2Float::Sqrt()
{
	int32_t t;
	int32_t s = 0;
	int32_t q = 0;
	uint32_t r = 0x01000000; /* r = moving bit from right to left */

	if (IsDenormalized())
		return Ps2Float(0);

	// PS2 only takes positive numbers for SQRT, and convert if necessary.
	int32_t ix = (int32_t)(Ps2Float(false, Exponent, Mantissa).AsUInt32());

	/* extract mantissa and unbias exponent */
	int32_t m = (ix >> 23) - BIAS;

	ix = (ix & 0x007FFFFF) | 0x00800000;
	if ((m & 1) == 1)
	{
		/* odd m, double x to make it even */
		ix += ix;
	}

	m >>= 1; /* m = [m/2] */

	/* generate sqrt(x) bit by bit */
	ix += ix;

	while (r != 0)
	{
		t = s + (int32_t)(r);
		if (t <= ix)
		{
			s = t + (int32_t)(r);
			ix -= t;
			q += (int32_t)(r);
		}

		ix += ix;
		r >>= 1;
	}

	/* use floating add to find out rounding direction */
	if (ix != 0)
	{
		q += q & 1;
	}

	ix = (q >> 1) + 0x3F000000;
	ix += m << 23;

	return Ps2Float((uint32_t)(ix));
}

Ps2Float Ps2Float::Rsqrt(Ps2Float other)
{
	return Div(other.Sqrt());
}

bool Ps2Float::IsDenormalized()
{
	return Exponent == 0;
}

bool Ps2Float::IsAbnormal()
{
	uint32_t val = AsUInt32();
	return val == MAX_FLOATING_POINT_VALUE || val == MIN_FLOATING_POINT_VALUE ||
		   val == POSITIVE_INFINITY_VALUE || val == NEGATIVE_INFINITY_VALUE;
}

bool Ps2Float::IsZero()
{
	return (Abs()) == 0;
}

uint32_t Ps2Float::Abs()
{
	return (AsUInt32() & MAX_FLOATING_POINT_VALUE);
}

Ps2Float Ps2Float::RoundTowardsZero()
{
	return Ps2Float((uint32_t)(std::trunc((double)(AsUInt32()))));
}

int32_t Ps2Float::CompareTo(Ps2Float other)
{
	int32_t selfTwoComplementVal = (int32_t)(Abs());
	if (Sign)
		selfTwoComplementVal = -selfTwoComplementVal;

	int32_t otherTwoComplementVal = (int32_t)(other.Abs());
	if (other.Sign)
		otherTwoComplementVal = -otherTwoComplementVal;

	if (selfTwoComplementVal < otherTwoComplementVal)
		return -1;
	else if (selfTwoComplementVal == otherTwoComplementVal)
		return 0;
	else
		return 1;
}

int32_t Ps2Float::CompareOperand(Ps2Float other)
{
	int32_t selfTwoComplementVal = (int32_t)(Abs());
	int32_t otherTwoComplementVal = (int32_t)(other.Abs());

	if (selfTwoComplementVal < otherTwoComplementVal)
		return -1;
	else if (selfTwoComplementVal == otherTwoComplementVal)
		return 0;
	else
		return 1;
}

double Ps2Float::ToDouble()
{
	return std::bit_cast<double>(((u64)Sign << 63) | ((((u64)Exponent - BIAS) + 1023ULL) << 52) | ((u64)Mantissa << 29));
}

std::string Ps2Float::ToString()
{
	double res = ToDouble();

	uint32_t value = AsUInt32();
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(6);

	if (IsDenormalized())
	{
		oss << "Denormalized(" << res << ")";
	}
	else if (value == MAX_FLOATING_POINT_VALUE)
	{
		oss << "Fmax(" << res << ")";
	}
	else if (value == MIN_FLOATING_POINT_VALUE)
	{
		oss << "-Fmax(" << res << ")";
	}
	else if (value == POSITIVE_INFINITY_VALUE)
	{
		oss << "Inf(" << res << ")";
	}
	else if (value == NEGATIVE_INFINITY_VALUE)
	{
		oss << "-Inf(" << res << ")";
	}
	else
	{
		oss << "Ps2Float(" << res << ")";
	}

	return oss.str();
}

Ps2Float Ps2Float::DoAdd(Ps2Float other)
{
	const uint8_t roundingMultiplier = 6;

	uint8_t selfExponent = Exponent;
	int32_t resExponent = selfExponent - other.Exponent;

	if (resExponent < 0)
		return other.DoAdd(*this);
	else if (resExponent >= 25)
		return *this;

	// http://graphics.stanford.edu/~seander/bithacks.html#ConditionalNegate
	uint32_t sign1 = (uint32_t)((int32_t)AsUInt32() >> 31);
	int32_t selfMantissa = (int32_t)(((Mantissa | 0x800000) ^ sign1) - sign1);
	uint32_t sign2 = (uint32_t)((int32_t)other.AsUInt32() >> 31);
	int32_t otherMantissa = (int32_t)(((other.Mantissa | 0x800000) ^ sign2) - sign2);

	// PS2 multiply by 2 before doing the Math here.
	int32_t man = (selfMantissa << roundingMultiplier) + ((otherMantissa << roundingMultiplier) >> resExponent);
	int32_t absMan = abs(man);
	if (absMan == 0)
		return Ps2Float(0);

	// Remove from exponent the PS2 Multiplier value.
	int32_t rawExp = selfExponent - roundingMultiplier;

	int32_t amount = normalizeAmounts[clz(absMan)];
	rawExp -= amount;
	absMan <<= amount;

	int32_t msbIndex = BitScanReverse8(absMan >> 23);
	rawExp += msbIndex;
	absMan >>= msbIndex;

	if (rawExp > 255)
		return man < 0 ? Min() : Max();
	else if (rawExp <= 0)
		return Ps2Float(man < 0, 0, 0);

	return Ps2Float((uint32_t)man & SIGNMASK | (uint32_t)rawExp << 23 | ((uint32_t)absMan & 0x7FFFFF));
}

Ps2Float Ps2Float::DoMul(Ps2Float other)
{
	uint8_t selfExponent = Exponent;
	uint8_t otherExponent = other.Exponent;
	uint32_t selfMantissa = Mantissa | 0x800000;
	uint32_t otherMantissa = other.Mantissa | 0x800000;
	uint32_t sign = (AsUInt32() ^ other.AsUInt32()) & SIGNMASK;

	int32_t resExponent = selfExponent + otherExponent - 127;
	uint32_t resMantissa = (uint32_t)(BoothMultiplier::MulMantissa(selfMantissa, otherMantissa) >> 23);

	if (resMantissa > 0xFFFFFF)
	{
		resMantissa >>= 1;
		resExponent++;
	}

	if (resExponent > 255)
		return Ps2Float(sign | MAX_FLOATING_POINT_VALUE);
	else if (resExponent <= 0)
		return Ps2Float(sign);

	return Ps2Float(sign | (uint32_t)(resExponent << 23) | (resMantissa & 0x7FFFFF));
}

Ps2Float Ps2Float::DoDiv(Ps2Float other)
{
	uint64_t selfMantissa64;
	uint32_t selfMantissa = Mantissa | 0x800000;
	uint32_t otherMantissa = other.Mantissa | 0x800000;
	int resExponent = Exponent - other.Exponent + BIAS;

	Ps2Float result = Ps2Float(0);

	result.Sign = DetermineMultiplicationDivisionOperationSign(*this, other);

	if (resExponent > 255)
		return result.Sign ? Min() : Max();
	else if (resExponent <= 0)
		return Ps2Float(result.Sign, 0, 0);

	if (selfMantissa < otherMantissa)
	{
		--resExponent;
		if (resExponent == 0)
			return Ps2Float(result.Sign, 0, 0);
		selfMantissa64 = (uint64_t)(selfMantissa) << 31;
	}
	else
	{
		selfMantissa64 = (uint64_t)(selfMantissa) << 30;
	}

	uint32_t resMantissa = (uint32_t)(selfMantissa64 / otherMantissa);
	if ((resMantissa & 0x3F) == 0)
		resMantissa |= ((uint64_t)(otherMantissa)*resMantissa != selfMantissa64) ? 1U : 0;

	result.Exponent = (uint8_t)(resExponent);
	result.Mantissa = (resMantissa + 0x39U /* Non-standard value, 40U in IEEE754 (PS2: rsqrt(0x40400000, 0x40400000) = 0x3FDDB3D7 -> IEEE754: rsqrt(0x40400000, 0x40400000) = 0x3FDDB3D8 */) >> 7;

	if (result.Mantissa > 0)
	{
		int32_t leadingBitPosition = Ps2Float::GetMostSignificantBitPosition(result.Mantissa);

		while (leadingBitPosition != IMPLICIT_LEADING_BIT_POS)
		{
			if (leadingBitPosition > IMPLICIT_LEADING_BIT_POS)
			{
				result.Mantissa >>= 1;

				int32_t exp = ((int32_t)result.Exponent + 1);

				if (exp > 255)
					return result.Sign ? Min() : Max();

				result.Exponent = (uint8_t)exp;

				leadingBitPosition--;
			}
			else if (leadingBitPosition < IMPLICIT_LEADING_BIT_POS)
			{
				result.Mantissa <<= 1;

				int32_t exp = ((int32_t)result.Exponent - 1);

				if (exp <= 0)
					return Ps2Float(result.Sign, 0, 0);

				result.Exponent = (uint8_t)exp;

				leadingBitPosition++;
			}
		}
	}

	result.Mantissa &= 0x7FFFFF;
	return result.RoundTowardsZero();
}

Ps2Float Ps2Float::SolveAbnormalAdditionOrSubtractionOperation(Ps2Float a, Ps2Float b, bool add)
{
	uint32_t aval = a.AsUInt32();
	uint32_t bval = b.AsUInt32();

	if (aval == MAX_FLOATING_POINT_VALUE && bval == MAX_FLOATING_POINT_VALUE)
		return add ? Max() : Ps2Float(0);

	if (aval == MIN_FLOATING_POINT_VALUE && bval == MIN_FLOATING_POINT_VALUE)
		return add ? Min() : Ps2Float(0);

	if (aval == MIN_FLOATING_POINT_VALUE && bval == MAX_FLOATING_POINT_VALUE)
		return add ? Ps2Float(0) : Min();

	if (aval == MAX_FLOATING_POINT_VALUE && bval == MIN_FLOATING_POINT_VALUE)
		return add ? Ps2Float(0) : Max();

	if (aval == POSITIVE_INFINITY_VALUE && bval == POSITIVE_INFINITY_VALUE)
		return add ? Max() : Ps2Float(0);

	if (aval == NEGATIVE_INFINITY_VALUE && bval == POSITIVE_INFINITY_VALUE)
		return add ? Ps2Float(0) : Min();

	if (aval == POSITIVE_INFINITY_VALUE && bval == NEGATIVE_INFINITY_VALUE)
		return add ? Ps2Float(0) : Max();

	if (aval == NEGATIVE_INFINITY_VALUE && bval == NEGATIVE_INFINITY_VALUE)
		return add ? Min() : Ps2Float(0);

	if (aval == MAX_FLOATING_POINT_VALUE && bval == POSITIVE_INFINITY_VALUE)
		return add ? Max() : Ps2Float(0x7F7FFFFE);

	if (aval == MAX_FLOATING_POINT_VALUE && bval == NEGATIVE_INFINITY_VALUE)
		return add ? Ps2Float(0x7F7FFFFE) : Max();

	if (aval == MIN_FLOATING_POINT_VALUE && bval == POSITIVE_INFINITY_VALUE)
		return add ? Ps2Float(0xFF7FFFFE) : Min();

	if (aval == MIN_FLOATING_POINT_VALUE && bval == NEGATIVE_INFINITY_VALUE)
		return add ? Min() : Ps2Float(0xFF7FFFFE);

	if (aval == POSITIVE_INFINITY_VALUE && bval == MAX_FLOATING_POINT_VALUE)
		return add ? Max() : Ps2Float(0xFF7FFFFE);

	if (aval == POSITIVE_INFINITY_VALUE && bval == MIN_FLOATING_POINT_VALUE)
		return add ? Ps2Float(0xFF7FFFFE) : Max();

	if (aval == NEGATIVE_INFINITY_VALUE && bval == MAX_FLOATING_POINT_VALUE)
		return add ? Ps2Float(0x7F7FFFFE) : Min();

	if (aval == NEGATIVE_INFINITY_VALUE && bval == MIN_FLOATING_POINT_VALUE)
		return add ? Min() : Ps2Float(0x7F7FFFFE);

	Console.Error("Unhandled abnormal add/sub floating point operation");
}

Ps2Float Ps2Float::SolveAbnormalMultiplicationOrDivisionOperation(Ps2Float a, Ps2Float b, bool mul)
{
	uint32_t aval = a.AsUInt32();
	uint32_t bval = b.AsUInt32();

	if (mul)
	{
		if ((aval == MAX_FLOATING_POINT_VALUE && bval == MAX_FLOATING_POINT_VALUE) ||
			(aval == MIN_FLOATING_POINT_VALUE && bval == MIN_FLOATING_POINT_VALUE))
			return Max();

		if ((aval == MAX_FLOATING_POINT_VALUE && bval == MIN_FLOATING_POINT_VALUE) ||
			(aval == MIN_FLOATING_POINT_VALUE && bval == MAX_FLOATING_POINT_VALUE))
			return Min();

		if (aval == POSITIVE_INFINITY_VALUE && bval == POSITIVE_INFINITY_VALUE)
			return Max();

		if (aval == NEGATIVE_INFINITY_VALUE && bval == POSITIVE_INFINITY_VALUE)
			return Min();

		if (aval == POSITIVE_INFINITY_VALUE && bval == NEGATIVE_INFINITY_VALUE)
			return Min();

		if (aval == NEGATIVE_INFINITY_VALUE && bval == NEGATIVE_INFINITY_VALUE)
			return Max();

		if (aval == MAX_FLOATING_POINT_VALUE && bval == POSITIVE_INFINITY_VALUE)
			return Max();

		if (aval == MAX_FLOATING_POINT_VALUE && bval == NEGATIVE_INFINITY_VALUE)
			return Min();

		if (aval == MIN_FLOATING_POINT_VALUE && bval == POSITIVE_INFINITY_VALUE)
			return Min();

		if (aval == MIN_FLOATING_POINT_VALUE && bval == NEGATIVE_INFINITY_VALUE)
			return Max();

		if (aval == POSITIVE_INFINITY_VALUE && bval == MAX_FLOATING_POINT_VALUE)
			return Max();

		if (aval == POSITIVE_INFINITY_VALUE && bval == MIN_FLOATING_POINT_VALUE)
			return Min();

		if (aval == NEGATIVE_INFINITY_VALUE && bval == MAX_FLOATING_POINT_VALUE)
			return Min();

		if (aval == NEGATIVE_INFINITY_VALUE && bval == MIN_FLOATING_POINT_VALUE)
			return Max();
	}
	else
	{
		if ((aval == MAX_FLOATING_POINT_VALUE && bval == MAX_FLOATING_POINT_VALUE) ||
			(aval == MIN_FLOATING_POINT_VALUE && bval == MIN_FLOATING_POINT_VALUE))
			return One();

		if ((aval == MAX_FLOATING_POINT_VALUE && bval == MIN_FLOATING_POINT_VALUE) ||
			(aval == MIN_FLOATING_POINT_VALUE && bval == MAX_FLOATING_POINT_VALUE))
			return MinOne();

		if (aval == POSITIVE_INFINITY_VALUE && bval == POSITIVE_INFINITY_VALUE)
			return One();

		if (aval == NEGATIVE_INFINITY_VALUE && bval == POSITIVE_INFINITY_VALUE)
			return MinOne();

		if (aval == POSITIVE_INFINITY_VALUE && bval == NEGATIVE_INFINITY_VALUE)
			return MinOne();

		if (aval == NEGATIVE_INFINITY_VALUE && bval == NEGATIVE_INFINITY_VALUE)
			return One();

		if (aval == MAX_FLOATING_POINT_VALUE && bval == POSITIVE_INFINITY_VALUE)
			return Ps2Float(0x3FFFFFFF);

		if (aval == MAX_FLOATING_POINT_VALUE && bval == NEGATIVE_INFINITY_VALUE)
			return Ps2Float(0xBFFFFFFF);

		if (aval == MIN_FLOATING_POINT_VALUE && bval == POSITIVE_INFINITY_VALUE)
			return Ps2Float(0xBFFFFFFF);

		if (aval == MIN_FLOATING_POINT_VALUE && bval == NEGATIVE_INFINITY_VALUE)
			return Ps2Float(0x3FFFFFFF);

		if (aval == POSITIVE_INFINITY_VALUE && bval == MAX_FLOATING_POINT_VALUE)
			return Ps2Float(0x3F000001);

		if (aval == POSITIVE_INFINITY_VALUE && bval == MIN_FLOATING_POINT_VALUE)
			return Ps2Float(0xBF000001);

		if (aval == NEGATIVE_INFINITY_VALUE && bval == MAX_FLOATING_POINT_VALUE)
			return Ps2Float(0xBF000001);

		if (aval == NEGATIVE_INFINITY_VALUE && bval == MIN_FLOATING_POINT_VALUE)
			return Ps2Float(0x3F000001);
	}

	Console.Error("Unhandled abnormal mul/div floating point operation");
}

Ps2Float Ps2Float::SolveAddSubDenormalizedOperation(Ps2Float a, Ps2Float b, bool add)
{
	Ps2Float result = Ps2Float(0);

	if (a.IsDenormalized() && !b.IsDenormalized())
		result = b;
	else if (!a.IsDenormalized() && b.IsDenormalized())
		result = a;
	else if (a.IsDenormalized() && b.IsDenormalized())
	{
	}
	else
		Console.Error("Both numbers are not denormalized");

	result.Sign = add ? DetermineAdditionOperationSign(a, b) : DetermineSubtractionOperationSign(a, b);
	return result;
}

Ps2Float Ps2Float::SolveMultiplicationDenormalizedOperation(Ps2Float a, Ps2Float b)
{
	Ps2Float result = Ps2Float(0);

	result.Sign = DetermineMultiplicationDivisionOperationSign(a, b);
	return result;
}

Ps2Float Ps2Float::SolveDivisionDenormalizedOperation(Ps2Float a, Ps2Float b)
{
	bool sign = DetermineMultiplicationDivisionOperationSign(a, b);
	Ps2Float result = Ps2Float(0);

	if (a.IsDenormalized() && !b.IsDenormalized())
	{
	}
	else if (!a.IsDenormalized() && b.IsDenormalized())
		return sign ? Min() : Max();
	else if (a.IsDenormalized() && b.IsDenormalized())
		return sign ? Min() : Max();
	else
		Console.Error("Both numbers are not denormalized");

	result.Sign = sign;
	return result;
}

Ps2Float Ps2Float::Neg(Ps2Float self)
{
	return Ps2Float(self.AsUInt32() ^ SIGNMASK);
}

bool Ps2Float::DetermineMultiplicationDivisionOperationSign(Ps2Float a, Ps2Float b)
{
	return a.Sign ^ b.Sign;
}

bool Ps2Float::DetermineAdditionOperationSign(Ps2Float a, Ps2Float b)
{
	if (a.IsZero() && b.IsZero())
	{
		if (!a.Sign || !b.Sign)
			return false;
		else if (a.Sign && b.Sign)
			return true;
		else
			Console.Error("Unhandled addition operation flags");
	}
	
	return a.CompareOperand(b) >= 0 ? a.Sign : b.Sign;
}

bool Ps2Float::DetermineSubtractionOperationSign(Ps2Float a, Ps2Float b)
{
	if (a.IsZero() && b.IsZero())
	{
		if (!a.Sign || b.Sign)
			return false;
		else if (a.Sign && !b.Sign)
			return true;
		else
			Console.Error("Unhandled subtraction operation flags");
	}

	return a.CompareOperand(b) >= 0 ? a.Sign : !b.Sign;
}

int32_t Ps2Float::clz(int x)
{
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;

	return debruijn32[(uint)x * 0x8c0b2891u >> 26];
}

int32_t Ps2Float::BitScanReverse8(int b)
{
	return msb[b];
}

int32_t Ps2Float::GetMostSignificantBitPosition(uint32_t value)
{
	for (int32_t i = 31; i >= 0; i--)
	{
		if (((value >> i) & 1) != 0)
			return i;
	}
	return -1;
}

const int8_t Ps2Float::msb[256] =
	{
		-1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
		5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};

const int32_t Ps2Float::debruijn32[64] =
	{
		32, 8, 17, -1, -1, 14, -1, -1, -1, 20, -1, -1, -1, 28, -1, 18,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 26, 25, 24,
		4, 11, 23, 31, 3, 7, 10, 16, 22, 30, -1, -1, 2, 6, 13, 9,
		-1, 15, -1, 21, -1, 29, 19, -1, -1, -1, -1, -1, 1, 27, 5, 12};

const int32_t Ps2Float::normalizeAmounts[] =
	{
		0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 8, 8, 8, 8, 8, 8, 16, 16, 16, 16, 16, 16, 16, 16, 24, 24, 24, 24, 24, 24, 24};
