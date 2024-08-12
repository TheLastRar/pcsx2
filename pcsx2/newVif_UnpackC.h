// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0

#pragma once

#include "Common.h"
#include "Vif_Dma.h"
#include "Vif_Dynarec.h"

// --------------------------------------------------------------------------------------
//  VifUnpack_Base
// --------------------------------------------------------------------------------------
class VifUnpackC
{
public:
	//bool usn; // unsigned flag
	//bool doMask; // masking write enable flag
	//const static int UnpkLoopIteration = 0;
	const static int IsAligned;

protected:
	union regVector
	{
		u8 uB[16];
		s8 sB[16];
		u16 uH[8];
		s16 sH[8];
		u32 uS[4];
		s32 sS[4];
		u64 uD[2];
		s64 sD[2];
	};

public:
	VifUnpackC();
	virtual ~VifUnpackC() = default;

	template <int upktype, bool usn>
	static regVector xUnpack(const void* src);
	template <bool doMask, int curCycle>
	static void xMovDest(regVector destReg, void* dstIndirect);

protected:
	template <int curCycle>
	static void doMaskWrite(regVector& regX, void* dstIndirect);

	//static void xShiftR(const vixl::aarch64::VRegister& regX, int n) const;
	template <bool usn>
	static void xPMOVXX8(regVector& regX, const void* src);
	template <bool usn>
	static void xPMOVXX16(regVector& regX, const void* src);

	template <bool usn>
	static regVector xUPK_S_32(const void* src);
	template <bool usn>
	static regVector xUPK_S_16(const void* src);
	template <bool usn>
	static regVector xUPK_S_8(const void* src);

	template <bool usn>
	static regVector xUPK_V2_32(const void* src);
	template <bool usn>
	static regVector xUPK_V2_16(const void* src);
	template <bool usn>
	static regVector xUPK_V2_8(const void* src);

	template <bool usn>
	static regVector xUPK_V3_32(const void* src);
	template <bool usn>
	static regVector xUPK_V3_16(const void* src);
	template <bool usn>
	static regVector xUPK_V3_8(const void* src);

	template <bool usn>
	static regVector xUPK_V4_32(const void* src);
	template <bool usn>
	static regVector xUPK_V4_16(const void* src);
	template <bool usn>
	static regVector xUPK_V4_8(const void* src);
	template <bool usn>
	static regVector xUPK_V4_5(const void* src);

	template <bool usn>
	static regVector xUPK_INV(const void* src);
};

// --------------------------------------------------------------------------------------
//  VifUnpackSSE_Simple
// --------------------------------------------------------------------------------------
class VifUnpackC_Simple : public VifUnpackC
{
	typedef VifUnpackC _parent;

public:
	int curCycle;

public:
	VifUnpackC_Simple(bool usn_, bool domask_, int curCycle_);
	virtual ~VifUnpackC_Simple() = default;
};
