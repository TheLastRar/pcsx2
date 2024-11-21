// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <bit>
#include "BoothMultiplier.h"

BoothMultiplier::BoothRecode BoothMultiplier::Booth(uint32_t a, uint32_t b, uint32_t bit)
{
	uint32_t test = (bit ? b >> (bit * 2 - 1) : b << 1) & 7;
	a <<= (bit * 2);
	a += (test == 3 || test == 4) ? a : 0;
	uint32_t neg = (test >= 4 && test <= 6) ? ~0u : 0;
	uint32_t pos = 1 << (bit * 2);
	a ^= (neg & -pos);
	a &= (test >= 1 && test <= 6) ? ~0u : 0;
	return {a, neg & pos};
}

BoothMultiplier::AddResult BoothMultiplier::Add3(uint32_t a, uint32_t b, uint32_t c)
{
	uint32_t u = a ^ b;
	return {u ^ c, ((u & c) | (a & b)) << 1};
}

uint64_t BoothMultiplier::MulMantissa(uint32_t a, uint32_t b)
{
	uint64_t full = static_cast<uint64_t>(a) * static_cast<uint64_t>(b);
	BoothRecode b0 = Booth(a, b, 0);
	BoothRecode b1 = Booth(a, b, 1);
	BoothRecode b2 = Booth(a, b, 2);
	BoothRecode b3 = Booth(a, b, 3);
	BoothRecode b4 = Booth(a, b, 4);
	BoothRecode b5 = Booth(a, b, 5);
	BoothRecode b6 = Booth(a, b, 6);
	BoothRecode b7 = Booth(a, b, 7);

	// First cycle
	AddResult t0 = Add3(b1.data, b2.data, b3.data);
	AddResult t1 = Add3(b4.data & ~0x7ffu, b5.data & ~0xfffu, b6.data);
	// A few adds get skipped, squeeze them back in
	t1.hi |= b6.negate | (b5.data & 0x800);
	b7.data |= (b5.data & 0x400) + b5.negate;

	// Second cycle
	AddResult t2 = Add3(b0.data, t0.lo, t0.hi);
	AddResult t3 = Add3(b7.data, t1.lo, t1.hi);

	// Third cycle
	AddResult t4 = Add3(t2.hi, t3.lo, t3.hi);

	// Fourth cycle
	AddResult t5 = Add3(t2.lo, t4.lo, t4.hi);

	// Discard bits and sum
	t5.hi += b7.negate;
	t5.lo &= ~0x7fffu;
	t5.hi &= ~0x7fffu;
	uint32_t ps2lo = t5.lo + t5.hi;
	return full - ((ps2lo ^ full) & 0x8000);
}