// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

class BoothMultiplier
{
public:

	static uint64_t MulMantissa(uint32_t a, uint32_t b);

protected:

private:
	struct BoothRecode
	{
		uint32_t data;
		uint32_t negate;
	};

	struct AddResult
	{
		uint32_t lo;
		uint32_t hi;
	};

	static BoothRecode Booth(uint32_t a, uint32_t b, uint32_t bit);

	static AddResult Add3(uint32_t a, uint32_t b, uint32_t c);
};