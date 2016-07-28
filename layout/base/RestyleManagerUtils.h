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
};

} // namespace mozilla

#endif
