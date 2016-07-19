/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/ServoElementSnapshot.h"
#include "mozilla/dom/Element.h"

namespace mozilla {

ServoElementSnapshot::ServoElementSnapshot(Element* aElement,
                                           Flags aWhatToCapture)
  : mContains(Flags(0))
{
  MOZ_ASSERT(aWhatToCapture & Flags::All,
             "Huh, nothing to snapshot?");

  Add(aElement, aWhatToCapture, nsRestyleHint(0), nsChangeHint(0));

  MOZ_ASSERT(mContains == aWhatToCapture, "What happened here?");
}

void ServoElementSnapshot::Add(Element* aElement,
                               Flags aWhatToCapture,
                               nsRestyleHint aRestyleHint,
                               nsChangeHint aMinChangeHint)
{
  if (aWhatToCapture & Flags::State) {
    AddState(aElement);
  }

  if (aWhatToCapture & Flags::Attributes) {
    AddAttrs(aElement);
  }

  mExplicitChangeHint |= aMinChangeHint;
  mExplicitRestyleHint |= aRestyleHint;
}

void
ServoElementSnapshot::AddState(Element* aElement)
{
  MOZ_ASSERT(aElement);
  if (!HasAny(Flags::State)) {
    mState = aElement->StyleState().ServoValue();
    mContains |= Flags::State;
  }
}

void
ServoElementSnapshot::AddAttrs(Element* aElement)
{
  MOZ_ASSERT(aElement);
  uint32_t attrCount = aElement->GetAttrCount();
  const nsAttrName* attrName;
  for (uint32_t i = 0; i < attrCount; ++i) {
    attrName = aElement->GetAttrNameAt(i);
    const nsAttrValue* attrValue =
      aElement->GetParsedAttr(attrName->LocalName(), attrName->NamespaceID());
    mAttrs.AppendElement(ServoAttrSnapshot(*attrName, *attrValue));
  }
  mContains |= Flags::Attributes;
}

} // namespace mozilla
