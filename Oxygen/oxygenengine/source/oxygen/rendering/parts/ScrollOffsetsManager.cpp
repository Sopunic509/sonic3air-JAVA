/*
*	Part of the Oxygen Engine / Sonic 3 A.I.R. software distribution.
*	Copyright (C) 2017-2021 by Eukaryot
*
*	Published under the GNU GPLv3 open source software license, see license.txt
*	or https://www.gnu.org/licenses/gpl-3.0.en.html
*/

#include "oxygen/pch.h"
#include "oxygen/rendering/parts/ScrollOffsetsManager.h"
#include "oxygen/rendering/parts/PlaneManager.h"
#include "oxygen/simulation/EmulatorInterface.h"


// This bitmask is applied to reduce the necessary precision in "render_plane.shader"
//  -> That's needed because on some Android devices, we don't even get full 16-bit integers
static const constexpr uint16 SCROLL_OFFSET_VALUE_BITMASK = 0x0fff;


ScrollOffsetsManager::ScrollOffsetsManager(PlaneManager& planeManager) :
	mPlaneManager(planeManager)
{
	reset();
}

void ScrollOffsetsManager::reset()
{
	mVerticalScrolling = false;
	mHorizontalScrollMask = 0xff;
	mVerticalScrollOffsetBias = 0;

	for (int index = 0; index < 4; ++index)
	{
		mSets[index] = { 0 };
	}
}

void ScrollOffsetsManager::refresh(const RefreshParameters& refreshParameters)
{
	if (refreshParameters.mHasNewSimulationFrame)
	{
		if (refreshParameters.mUsingFrameInterpolation)
		{
			for (int index = 0; index < 2; ++index)
			{
				for (int k = 0; k < 0x100; ++k)
					mInterpolatedSets[index].mLastScrollOffsetsH[k] = mSets[index].mScrollOffsetsH[k];
				for (int k = 0; k < 0x20; ++k)
					mInterpolatedSets[index].mLastScrollOffsetsV[k] = mSets[index].mScrollOffsetsV[k];
				mInterpolatedSets[index].mValid = false;
			}
		}

		// Horizontal scrolling
		for (int index = 0; index < 4; ++index)
		{
			uint16* buffer = mSets[index].mScrollOffsetsH;
			bool* overwriteFlags = mSets[index].mExplicitOverwriteH;
			if (index == 1 && mAbstractionModeForPlaneA)
			{
				const uint16 cameraX = EmulatorInterface::instance().readMemory16(0xffffee78);
				for (int k = 0; k < 0x100; ++k)
				{
					if (!overwriteFlags[k])
					{
						buffer[k] = cameraX & SCROLL_OFFSET_VALUE_BITMASK;
					}
				}
			}
			else if (index < 2)
			{
				const uint16* src = (uint16*)&EmulatorInterface::instance().getVRam()[mHorizontalScrollTableBase + (1 - index) * 2];
				for (int k = 0; k < 0x100; ++k)
				{
					if (!overwriteFlags[k])
					{
						const int index = k & mHorizontalScrollMask;
						buffer[k] = (-src[index*2]) & SCROLL_OFFSET_VALUE_BITMASK;
					}
				}
			}
			else
			{
				for (int k = 0; k < 0x100; ++k)
				{
					if (!overwriteFlags[k])
					{
						// Fallback: Use standard scroll offset values
						buffer[k] = mSets[index - 2].mScrollOffsetsH[k];
					}
				}
			}

			// Reset overwrite flags
			for (int k = 0; k < 0x100; ++k)
			{
				overwriteFlags[k] = false;
			}
		}

		// Vertical scrolling
		for (int index = 0; index < 4; ++index)
		{
			uint16* buffer = mSets[index].mScrollOffsetsV;
			bool* overwriteFlags = mSets[index].mExplicitOverwriteV;

			if (index < 2)
			{
				// One entry in VSRAM for each pattern column
				const uint16* src = &EmulatorInterface::instance().getVSRam()[1 - index];
				for (int k = 0; k < 0x20; ++k)
				{
					if (!overwriteFlags[k])
					{
						buffer[k] = src[k*2] & SCROLL_OFFSET_VALUE_BITMASK;
					}
				}
			}
			else
			{
				for (int k = 0; k < 0x20; ++k)
				{
					if (!overwriteFlags[k])
					{
						// Fallback: Use standard scroll offset values
						buffer[k] = mSets[index - 2].mScrollOffsetsV[k];
					}
				}
			}

			// Reset overwrite flags
			for (int k = 0; k < 0x20; ++k)
			{
				overwriteFlags[k] = false;
			}
		}

		if (refreshParameters.mUsingFrameInterpolation)
		{
			const uint16 positionMaskH = mPlaneManager.getPlayfieldSizeInPixels().x - 1;
			const uint16 halfPositionH = mPlaneManager.getPlayfieldSizeInPixels().x / 2;
			const uint16 positionMaskV = mPlaneManager.getPlayfieldSizeInPixels().y - 1;
			const uint16 halfPositionV = mPlaneManager.getPlayfieldSizeInPixels().y / 2;
			
			for (int index = 0; index < 2; ++index)
			{
				const int verticalDifference = mSets[index].mScrollOffsetsV[0] - mInterpolatedSets[index].mLastScrollOffsetsV[0];
				for (int k = 0; k < 0x100; ++k)
				{
					const int oldK = clamp(k + verticalDifference, 0, 223);
					int16 diff = mSets[index].mScrollOffsetsH[k] - mInterpolatedSets[index].mLastScrollOffsetsH[oldK];
					diff = ((diff & positionMaskH) < halfPositionH) ? (diff & positionMaskH) : -((-diff) & positionMaskH);
					if (abs(diff) >= 0x30)
						diff = 0;
					mInterpolatedSets[index].mDifferenceScrollOffsetsH[k] = diff;
				}
				for (int k = 0; k < 0x20; ++k)
				{
					int16 diff = mSets[index].mScrollOffsetsV[k] - mInterpolatedSets[index].mLastScrollOffsetsV[k];
					diff = ((diff & positionMaskV) < halfPositionV) ? (diff & positionMaskV) : -((-diff) & positionMaskV);
					if (abs(diff) >= 0x30)
						diff = 0;
					mInterpolatedSets[index].mDifferenceScrollOffsetsV[k] = diff;
				}
			}
		}
	}

	if (refreshParameters.mUsingFrameInterpolation)
	{
		const int32 interpolationFactor = roundToInt((1.0f - refreshParameters.mInterFramePosition) * 256.0f);
		const uint16 positionMaskH = mPlaneManager.getPlayfieldSizeInPixels().x - 1;
		const uint16 positionMaskV = mPlaneManager.getPlayfieldSizeInPixels().y - 1;

		for (int index = 0; index < 2; ++index)
		{
			const int verticalDifference = -(((int32)mInterpolatedSets[index].mDifferenceScrollOffsetsV[0] * interpolationFactor + 0x80) >> 8);
			for (int k = 0; k < 0x100; ++k)
			{
				const int oldK = clamp(k + verticalDifference, 0, 223);
				mInterpolatedSets[index].mInterpolatedScrollOffsetsH[k] = (mSets[index].mScrollOffsetsH[oldK] - (((int32)mInterpolatedSets[index].mDifferenceScrollOffsetsH[oldK] * interpolationFactor + 0x80) >> 8)) & positionMaskH;
			}
			for (int k = 0; k < 0x20; ++k)
			{
				mInterpolatedSets[index].mInterpolatedScrollOffsetsV[k] = (mSets[index].mScrollOffsetsV[k] - (((int32)mInterpolatedSets[index].mDifferenceScrollOffsetsV[k] * interpolationFactor + 0x80) >> 8)) & positionMaskV;
			}
		}
	}
	for (int index = 0; index < 2; ++index)
	{
		mInterpolatedSets[index].mValid = refreshParameters.mUsingFrameInterpolation;
	}
}

void ScrollOffsetsManager::preFrameUpdate()
{
	// Reset this again on each frame
	for (int k = 0; k < 4; ++k)
		mSets[k].mHorizontalScrollNoRepeat = false;
}

void ScrollOffsetsManager::postFrameUpdate()
{
}

bool ScrollOffsetsManager::getHorizontalScrollNoRepeat(int setIndex) const
{
	RMX_ASSERT(setIndex >= 0 && setIndex < 4, "Invalid scroll offset set index " << setIndex);

	const ScrollOffsetSet& set = mSets[setIndex];
	return set.mHorizontalScrollNoRepeat;
}

void ScrollOffsetsManager::setHorizontalScrollNoRepeat(int setIndex, bool enable)
{
	RMX_ASSERT(setIndex >= 0 && setIndex < 4, "Invalid scroll offset set index " << setIndex);

	ScrollOffsetSet& set = mSets[setIndex];
	set.mHorizontalScrollNoRepeat = enable;
}

void ScrollOffsetsManager::overwriteScrollOffsetH(int setIndex, int index, uint16 value)
{
	RMX_ASSERT(setIndex >= 0 && setIndex < 4, "Invalid scroll offset set index " << setIndex);
	RMX_ASSERT(index >= 0 && index < 0x100, "Invalid index " << index);

	ScrollOffsetSet& set = mSets[setIndex];
	set.mScrollOffsetsH[index] = value & SCROLL_OFFSET_VALUE_BITMASK;
	set.mExplicitOverwriteH[index] = true;
}

void ScrollOffsetsManager::overwriteScrollOffsetV(int setIndex, int index, uint16 value)
{
	RMX_ASSERT(setIndex >= 0 && setIndex < 4, "Invalid scroll offset set index " << setIndex);
	RMX_ASSERT(index >= 0 && index < 0x20, "Invalid index " << index);

	ScrollOffsetSet& set = mSets[setIndex];
	set.mScrollOffsetsV[index] = value & SCROLL_OFFSET_VALUE_BITMASK;
	set.mExplicitOverwriteV[index] = true;
}

const uint16* ScrollOffsetsManager::getScrollOffsetsH(int setIndex) const
{
	if (setIndex == 0xff)
	{
		static uint16 emptyScrollOffsetsH[0x100] = { 0 };
		return emptyScrollOffsetsH;
	}

	RMX_ASSERT(setIndex >= 0 && setIndex < 4, "Invalid scroll offset set index " << setIndex);
	if (setIndex < 2 && mInterpolatedSets[setIndex].mValid)
		return mInterpolatedSets[setIndex].mInterpolatedScrollOffsetsH;
	return mSets[setIndex].mScrollOffsetsH;
}

const uint16* ScrollOffsetsManager::getScrollOffsetsV(int setIndex) const
{
	if (setIndex == 0xff)
	{
		static uint16 emptyScrollOffsetsV[0x20] = { 0 };
		return emptyScrollOffsetsV;
	}

	RMX_ASSERT(setIndex >= 0 && setIndex < 4, "Invalid scroll offset set index " << setIndex);
	if (setIndex < 2 && mInterpolatedSets[setIndex].mValid)
		return mInterpolatedSets[setIndex].mInterpolatedScrollOffsetsV;
	return mSets[setIndex].mScrollOffsetsV;
}

void ScrollOffsetsManager::setVerticalScrollOffsetBias(int16 bias)
{
	mVerticalScrollOffsetBias = bias;
}