// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0

#include "newVif_UnpackC.h"
#include "MTVU.h"
#include "common/Perf.h"

// =====================================================================================================
//  VifUnpackC
// =====================================================================================================

VifUnpackC::VifUnpackC()
{
}

template <bool doMask, int curCycle>
void VifUnpackC::xMovDest(regVector destReg, void* dstIndirect)
{
	if (!doMask)
		std::memcpy(dstIndirect, &destReg, sizeof(regVector));
	else
		doMaskWrite<curCycle>(destReg, dstIndirect);
}

template <bool usn>
void VifUnpackC::xPMOVXX8(regVector& regX, const void* srcIndirect)
{
	regVector workReg;
	std::memcpy(&workReg, srcIndirect, sizeof(regVector));
	regVector destReg;

	if (usn)
	{
		destReg.uS[0] = workReg.uB[0];
		destReg.uS[1] = workReg.uB[1];
		destReg.uS[2] = workReg.uB[2];
		destReg.uS[3] = workReg.uB[3];
	}
	else
	{
		destReg.sS[0] = workReg.sB[0];
		destReg.sS[1] = workReg.sB[1];
		destReg.sS[2] = workReg.sB[2];
		destReg.sS[3] = workReg.sB[3];
	}
	regX = destReg;
}

template <bool usn>
void VifUnpackC::xPMOVXX16(regVector& regX, const void* srcIndirect)
{
	regVector workReg;
	std::memcpy(&workReg, srcIndirect, sizeof(regVector));
	regVector destReg;

	if (usn)
	{
		destReg.uS[0] = workReg.uH[0];
		destReg.uS[1] = workReg.uH[1];
		destReg.uS[2] = workReg.uH[2];
		destReg.uS[3] = workReg.uH[3];
	}
	else
	{
		destReg.sS[0] = workReg.sH[0];
		destReg.sS[1] = workReg.sH[1];
		destReg.sS[2] = workReg.sH[2];
		destReg.sS[3] = workReg.sH[3];
	}
	regX = destReg;
}

template <bool usn>
VifUnpackC::regVector VifUnpackC::xUPK_S_32(const void* srcIndirect)
{
	regVector workReg;
	std::memcpy(&workReg, srcIndirect, sizeof(regVector));
	regVector destReg;
	destReg.uS[0] = workReg.uS[0];
	destReg.uS[1] = workReg.uS[0];
	destReg.uS[2] = workReg.uS[0];
	destReg.uS[3] = workReg.uS[0];
	return destReg;
}

template <bool usn>
VifUnpackC::regVector VifUnpackC::xUPK_S_16(const void* srcIndirect)
{
	regVector workReg;
	xPMOVXX16<usn>(workReg, srcIndirect);

	regVector destReg;
	destReg.uS[0] = workReg.uS[0];
	destReg.uS[1] = workReg.uS[0];
	destReg.uS[2] = workReg.uS[0];
	destReg.uS[3] = workReg.uS[0];
	return destReg;
}

template <bool usn>
VifUnpackC::regVector VifUnpackC::xUPK_S_8(const void* srcIndirect)
{
	regVector workReg;
	xPMOVXX8<usn>(workReg, srcIndirect);

	regVector destReg;
	destReg.uS[0] = workReg.uS[0];
	destReg.uS[1] = workReg.uS[0];
	destReg.uS[2] = workReg.uS[0];
	destReg.uS[3] = workReg.uS[0];
	return destReg;
}

// The V2 + V3 unpacks have freaky behaviour, the manual claims "indeterminate".
// After testing on the PS2, it's very much determinate in 99% of cases
// and games like Lemmings, And1 Streetball rely on this data to be like this!
// I have commented after each shuffle to show what data is going where - Ref

template <bool usn>
VifUnpackC::regVector VifUnpackC::xUPK_V2_32(const void* srcIndirect)
{
	regVector workReg;
	std::memcpy(&workReg, srcIndirect, sizeof(regVector));

	regVector destReg; //v1v0v1v0
	destReg.uD[0] = workReg.uD[0];
	destReg.uD[1] = workReg.uD[0];
	if (true)
		destReg.uS[3] = 0; //zero last word - tested on ps2
	return destReg;
}

template <bool usn>
VifUnpackC::regVector VifUnpackC::xUPK_V2_16(const void* srcIndirect)
{
	regVector workReg;
	xPMOVXX16<usn>(workReg, srcIndirect);

	regVector destReg; //v1v0v1v0
	destReg.uD[0] = workReg.uD[0];
	destReg.uD[1] = workReg.uD[0];
	return destReg;
}

template <bool usn>
VifUnpackC::regVector VifUnpackC::xUPK_V2_8(const void* srcIndirect)
{
	regVector workReg;
	xPMOVXX8<usn>(workReg, srcIndirect);

	regVector destReg; //v1v0v1v0
	destReg.uD[0] = workReg.uD[0];
	destReg.uD[1] = workReg.uD[0];
	return destReg;
}

template <bool usn>
VifUnpackC::regVector VifUnpackC::xUPK_V3_32(const void* srcIndirect)
{
	regVector destReg;
	std::memcpy(&destReg, srcIndirect, sizeof(regVector));

	if (0 != true)
		destReg.uS[3] = 0;
	//	armAsm->Ins(destReg.V4S(), 3, a64::wzr);
	return destReg;
}

template <bool usn>
VifUnpackC::regVector VifUnpackC::xUPK_V3_16(const void* srcIndirect)
{
	regVector destReg;
	xPMOVXX16<usn>(destReg, srcIndirect);

	//With V3-16, it takes the first vector from the next position as the W vector
	//However - IF the end of this iteration of the unpack falls on a quadword boundary, W becomes 0
	//IsAligned is the position through the current QW in the vif packet
	//Iteration counts where we are in the packet.
	int result = (((0 / 4) + 1 + (4 - 1)) & 0x3);

	if ((0 & 0x1) == 0 && result == 0)
		destReg.uS[3] = 0; //zero last word on QW boundary if whole 32bit word is used - tested on ps2

	return destReg;
}

template <bool usn>
VifUnpackC::regVector VifUnpackC::xUPK_V3_8(const void* srcIndirect)
{
	regVector destReg;
	xPMOVXX8<usn>(destReg, srcIndirect);

	if (0 != false)
		destReg.uS[3] = 0;

	return destReg;
}

template <bool usn>
VifUnpackC::regVector VifUnpackC::xUPK_V4_32(const void* srcIndirect)
{
	regVector workReg;
	std::memcpy(&workReg, srcIndirect, sizeof(regVector));
	return workReg;
}

template <bool usn>
VifUnpackC::regVector VifUnpackC::xUPK_V4_16(const void* srcIndirect)
{
	regVector destReg;
	xPMOVXX16<usn>(destReg, srcIndirect);
	return destReg;
}

template <bool usn>
VifUnpackC::regVector VifUnpackC::xUPK_V4_8(const void* srcIndirect)
{
	regVector destReg;
	xPMOVXX8<usn>(destReg, srcIndirect);
	return destReg;
}

template <bool usn>
VifUnpackC::regVector VifUnpackC::xUPK_V4_5(const void* srcIndirect)
{
	regVector destReg;
	std::memcpy(&destReg, srcIndirect, sizeof(regVector));

	u32 workW = destReg.uH[0];
	workW = workW << 3; // ABG|R5.000
	destReg.uS[0] = workW; // x|x|x|R
	workW = workW >> 8; // ABG
	workW = workW << 3; // AB|G5.000
	destReg.uS[1] = workW; // x|x|G|R
	workW = workW >> 8; // AB
	workW = workW << 3; // A|B5.000
	destReg.uS[2] = workW; // x|B|G|R
	workW = workW >> 8; // A
	workW = workW << 7; // A.0000000
	destReg.uS[3] = workW; // A|B|G|R

	destReg.uS[0] = destReg.uS[0] << 24; // can optimize to
	destReg.uS[0] = destReg.uS[0] >> 24; // single AND...
	destReg.uS[1] = destReg.uS[1] << 24;
	destReg.uS[1] = destReg.uS[1] << 24;
	destReg.uS[2] = destReg.uS[2] << 24;
	destReg.uS[2] = destReg.uS[2] >> 24;
	destReg.uS[3] = destReg.uS[3] >> 24;
	destReg.uS[3] = destReg.uS[3] >> 24;

	return destReg;
}

template <bool usn>
VifUnpackC::regVector VifUnpackC::xUPK_INV(const void* srcIndirect)
{
	regVector workReg;
	std::memcpy(&workReg, srcIndirect, sizeof(regVector));
	return workReg;
}

template <int upknum, bool usn>
VifUnpackC::regVector VifUnpackC::xUnpack(const void* srcIndirect)
{
	switch (upknum)
	{
		case 0:
			return xUPK_S_32<usn>(srcIndirect);
		case 1:
			return xUPK_S_16<usn>(srcIndirect);
		case 2:
			return xUPK_S_8<usn>(srcIndirect);

		case 4:
			return xUPK_V2_32<usn>(srcIndirect);
		case 5:
			return xUPK_V2_16<usn>(srcIndirect);
		case 6:
			return xUPK_V2_8<usn>(srcIndirect);

		case 8:
			return xUPK_V3_32<usn>(srcIndirect);
		case 9:
			return xUPK_V3_16<usn>(srcIndirect);
		case 10:
			return xUPK_V3_8<usn>(srcIndirect);

		case 12:
			return xUPK_V4_32<usn>(srcIndirect);
		case 13:
			return xUPK_V4_16<usn>(srcIndirect);
		case 14:
			return xUPK_V4_8<usn>(srcIndirect);
		case 15:
			return xUPK_V4_5<usn>(srcIndirect);

		case 3:
		case 7:
		case 11:
		default:
			Console.Warning("Vpu/Vif: Invalid Unpack %d", upknum);
			return xUPK_INV<usn>(srcIndirect);
	}
}

template <int curCycle>
void VifUnpackC::doMaskWrite(regVector& regX, void* dstIndirect)
{
	regVector reg7;

	std::memcpy(&reg7, dstIndirect, sizeof(regVector));

	int offX = std::min(curCycle, 3);

	regVector mask1, mask2, mask3;
	std::memcpy(&mask1, nVifMask[0][offX], sizeof(regVector));
	std::memcpy(&mask2, nVifMask[1][offX], sizeof(regVector));
	std::memcpy(&mask3, nVifMask[2][offX], sizeof(regVector));

	regX.uD[0] = regX.uD[0] & mask1.uD[0];
	regX.uD[1] = regX.uD[1] & mask1.uD[1];

	reg7.uD[0] = reg7.uD[0] & mask2.uD[0];
	reg7.uD[1] = reg7.uD[1] & mask2.uD[1];

	regX.uD[0] = regX.uD[0] | mask3.uD[0];
	regX.uD[1] = regX.uD[1] | mask3.uD[1];

	regX.uD[0] = regX.uD[0] | reg7.uD[0];
	regX.uD[1] = regX.uD[1] | reg7.uD[1];

	std::memcpy(dstIndirect, &regX, sizeof(regVector));
}

#define GenNVifCall(i) \
	{ \
		nVifCall& ucall(nVifUpk[((usnpart + maskpart + i) * 4) + curCycle]); \
		ucall = NULL; \
		if (nVifT[i] != 0) \
		{ \
			ucall = [](void* dest, const void* src) { \
				VifUnpackC_Simple::xMovDest<!!mask, curCycle>( \
					VifUnpackC_Simple::xUnpack<i, !!usn>(src), \
					dest); \
				return (u32)0; \
			}; \
		} \
	}

// ecx = dest, edx = src
template <int usn, int mask, int curCycle>
static void nVifGen()
{
	int usnpart = usn * 2 * 16;
	int maskpart = mask * 16;

	GenNVifCall(0);
	GenNVifCall(1);
	GenNVifCall(2);
	GenNVifCall(3);
	GenNVifCall(4);
	GenNVifCall(5);
	GenNVifCall(6);
	GenNVifCall(7);
	GenNVifCall(8);
	GenNVifCall(9);
	GenNVifCall(10);
	GenNVifCall(11);
	GenNVifCall(12);
	GenNVifCall(13);
	GenNVifCall(14);
	GenNVifCall(15);
}

#define NVIF_ENTRY_3(a, b, c) nVifGen<a, b, c>();

#define NVIF_ENTRY_2(a, b) \
	NVIF_ENTRY_3(a, b, 0) \
	NVIF_ENTRY_3(a, b, 1) \
	NVIF_ENTRY_3(a, b, 2) \
	NVIF_ENTRY_3(a, b, 3)

#define NVIF_ENTRY_1(a) \
	NVIF_ENTRY_2(a, 0) \
	NVIF_ENTRY_2(a, 1) \
	NVIF_ENTRY_2(a, 2)

void VifUnpackSSE_Init()
{
	DevCon.WriteLn("Getting template unpacking functions for VIF interpreters...");

	NVIF_ENTRY_1(0)
	NVIF_ENTRY_1(1)
	NVIF_ENTRY_1(2)
}

void dVifReset(int idx){};
void dVifRelease(int idx){};

_vifT extern void dVifUnpack(const u8* data, bool isFill)
{
	VIFregisters& vifRegs = MTVU_VifXRegs;
	_nVifUnpack(idx, data, vifRegs.mode, isFill);
}

template void dVifUnpack<0>(const u8* data, bool isFill);
template void dVifUnpack<1>(const u8* data, bool isFill);