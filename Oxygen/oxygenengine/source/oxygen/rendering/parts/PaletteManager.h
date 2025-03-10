/*
*	Part of the Oxygen Engine / Sonic 3 A.I.R. software distribution.
*	Copyright (C) 2017-2023 by Eukaryot
*
*	Published under the GNU GPLv3 open source software license, see license.txt
*	or https://www.gnu.org/licenses/gpl-3.0.en.html
*/

#pragma once

#include <rmxmedia.h>


class PaletteManager
{
public:
	static const constexpr uint16 NUM_COLORS = 0x200;

public:
	static Color unpackColor(uint16 packedColor);

public:
	void preFrameUpdate();

	const uint32* getPalette(int paletteIndex) const;
	void getPalette(Color* palette, int paletteIndex) const;
	Color getPaletteEntry(int paletteIndex, uint16 colorIndex) const;
	uint16 getPaletteEntryPacked(int paletteIndex, uint16 colorIndex, bool allowExtendedPacked = false) const;
	void writePaletteEntry(int paletteIndex, uint16 colorIndex, uint32 color);
	void writePaletteEntryPacked(int paletteIndex, uint16 colorIndex, uint16 packedColor);

	const uint64* getPaletteChangeFlags(int paletteIndex) const;
	void resetAllPaletteChangeFlags();
	void setAllPaletteChangeFlags();

	inline Color getBackdropColor() const  { return Color::fromABGR32(mPalette[0].mColor[mBackdropColorIndex]); }
	inline void setBackdropColorIndex(uint16 paletteIndex)  { mBackdropColorIndex = paletteIndex; }

	void setPaletteSplitPositionY(uint8 py);

	inline bool usesGlobalComponentTint() const  { return mUsesGlobalComponentTint; }
	inline const Vec4f& getGlobalComponentTintColor() const  { return mGlobalComponentTintColor; }
	inline const Vec4f& getGlobalComponentAddedColor() const { return mGlobalComponentAddedColor; }
	void setGlobalComponentTint(const Vec4f& tintColor, const Vec4f& addedColor);

public:
	int mSplitPositionY = 0xffff;	// Use some large value as default that is definitely larger than any responable screen height

private:
	struct PackedPaletteColor
	{
		uint16 mPackedColor = 0;
		bool mIsValid = false;
	};
	struct Palette
	{
		uint32 mColor[NUM_COLORS] = { 0 };							// Colors in the palette
		uint64 mChangeFlags[NUM_COLORS/64] = { true };				// One flag per color; only actually used and reset by hardware rendering
		PackedPaletteColor mPackedColorCache[NUM_COLORS] = { 0 };	// Only used as an optimization
	};

private:
	void setPaletteEntry(int paletteIndex, uint16 colorIndex, uint32 color);
	void setPaletteEntryPacked(int paletteIndex, uint16 colorIndex, uint32 color, uint16 packedColor);

private:
	Palette mPalette[2];			// [0] = Standard palette, [1] = Underwater palette (in S3AIR)
	uint16 mBackdropColorIndex = 0;

	bool mUsesGlobalComponentTint = false;
	Vec4f mGlobalComponentTintColor = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);	// Not using the Color class to be able to have negative channel values as well (especially for added color)
	Vec4f mGlobalComponentAddedColor = Vec4f(0.0f, 0.0f, 0.0f, 0.0f);
};
