// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Common.h"
#include "Ps2Float.h"
#include <cmath>
#include <float.h>

#include "VUmicro.h"

/*****************************************/
/*          NEW FLAGS                    */ //By asadr. Thnkx F|RES :p
/*****************************************/

static __ri u32 VU_MAC_UPDATE(int shift, VURegs* VU, uint32_t f)
{
	Ps2Float ps2f = Ps2Float(f);

	uint exp = ps2f.Exponent;
	u32 s = ps2f.AsUInt32() & Ps2Float::SIGNMASK;

	if (s)
		VU->macflag |= 0x0010<<shift;
	else
		VU->macflag &= ~(0x0010<<shift);

	if (ps2f.IsZero())
	{
		VU->macflag = (VU->macflag & ~(0x1100<<shift)) | (0x0001<<shift);
		return f;
	}

	switch(exp)
	{
		case 0:
			VU->macflag = (VU->macflag&~(0x1000<<shift)) | (0x0101<<shift);
			return s;
		case 255:
			if (CHECK_VU_SOFT_ADDSUB((VU == &VU1) ? 1 : 0) || CHECK_VU_SOFT_MULDIV((VU == &VU1) ? 1 : 0) || CHECK_VU_SOFT_SQRT((VU == &VU1) ? 1 : 0))
			{
				if (f == Ps2Float::MAX_FLOATING_POINT_VALUE || f == Ps2Float::MIN_FLOATING_POINT_VALUE)
				{
					VU->macflag = (VU->macflag & ~(0x0101 << shift)) | (0x1000 << shift);
					return f;
				}
				else
					return f;
			}
			else if (CHECK_VU_OVERFLOW((VU == &VU1) ? 1 : 0))
			{
				VU->macflag = (VU->macflag & ~(0x0101 << shift)) | (0x1000 << shift);
				return s | 0x7f7fffff; /* max IEEE754 allowed */
			}
			else
			{
				VU->macflag = (VU->macflag & ~(0x0101 << shift)) | (0x1000 << shift);
				return f;
			}
		default:
			VU->macflag = (VU->macflag & ~(0x1101<<shift));
			return f;
	}
}

__fi u32 VU_MACx_UPDATE(VURegs * VU, uint32_t x)
{
	return VU_MAC_UPDATE(3, VU, x);
}

__fi u32 VU_MACy_UPDATE(VURegs* VU, uint32_t y)
{
	return VU_MAC_UPDATE(2, VU, y);
}

__fi u32 VU_MACz_UPDATE(VURegs* VU, uint32_t z)
{
	return VU_MAC_UPDATE(1, VU, z);
}

__fi u32 VU_MACw_UPDATE(VURegs* VU, uint32_t w)
{
	return VU_MAC_UPDATE(0, VU, w);
}

__fi void VU_MACx_CLEAR(VURegs * VU)
{
	VU->macflag&= ~(0x1111<<3);
}

__fi void VU_MACy_CLEAR(VURegs * VU)
{
	VU->macflag&= ~(0x1111<<2);
}

__fi void VU_MACz_CLEAR(VURegs * VU)
{
	VU->macflag&= ~(0x1111<<1);
}

__fi void VU_MACw_CLEAR(VURegs * VU)
{
	VU->macflag&= ~(0x1111<<0);
}

__ri void VU_STAT_UPDATE(VURegs * VU) {
	int newflag = 0 ;
	if (VU->macflag & 0x000F) newflag = 0x1;
	if (VU->macflag & 0x00F0) newflag |= 0x2;
	if (VU->macflag & 0x0F00) newflag |= 0x4;
	if (VU->macflag & 0xF000) newflag |= 0x8;
	// Save old sticky flags and D/I settings, everthing else is the new flags only
	VU->statusflag = newflag;
}
