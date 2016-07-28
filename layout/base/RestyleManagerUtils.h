/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RestyleManagerUtils_h
#define mozilla_RestyleManagerUtils_h

#include "mozilla/OverflowChangedTracker.h"
#include "nsIFrame.h"

class nsStyleChangeList;

namespace mozilla {

/**
 * Common utilities that both restyle managers need.
 */
class RestyleManagerUtils final
{
  RestyleManagerUtils() = delete;
public:

  static nsIFrame*
  GetNearestAncestorFrame(nsIContent* aContent);

  static nsIFrame*
  GetNextBlockInInlineSibling(FramePropertyTable* aPropTable, nsIFrame* aFrame);

  static nsresult
  ProcessRestyledFrames(nsStyleChangeList& aChangeList,
                        nsPresContext& aPresContext,
                        OverflowChangedTracker& aOverflowChangedTracker);

  /**
   * Get the next continuation or similar ib-split sibling (assuming
   * block/inline alternation), conditionally on it having the same style.
   *
   * Since this is used when deciding to copy the new style context, it
   * takes as an argument the old style context to check if the style is
   * the same.  When it is used in other contexts (i.e., where the next
   * continuation would already have the new style context), the current
   * style context should be passed.
   */
  static nsIFrame*
  GetNextContinuationWithSameStyle(nsIFrame* aFrame,
                                   nsStyleContext* aOldStyleContext,
                                   bool* aHaveMoreContinuations = nullptr);
};

} // namespace mozilla

#endif
