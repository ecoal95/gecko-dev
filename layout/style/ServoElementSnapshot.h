/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoElementSnapshot_h
#define mozilla_ServoElementSnapshot_h

#include "mozilla/EventStates.h"
#include "nsAttrName.h"
#include "nsAttrValue.h"
#include "nsChangeHint.h"

namespace mozilla {

namespace dom {
class Element;
} // namespace dom

/**
 * A structure representing a single attribute name and value.
 *
 * This is pretty similar to the private nsAttrAndChildArray::InternalAttr.
 */
struct ServoAttrSnapshot {
  nsAttrName mName;
  nsAttrValue mValue;

  explicit ServoAttrSnapshot(const nsAttrName& aName,
                             const nsAttrValue& aValue)
    : mName(aName)
    , mValue(aValue)
  {}
};

/**
 * This class holds all non-tree-structural state of an element that might be
 * used for selector matching eventually.
 *
 * This means the attributes, and the element state, such as :hover, :active,
 * etc...
 */
class ServoElementSnapshot
{
  typedef mozilla::dom::Element Element;
  typedef EventStates::ServoType ServoStateType;
public:
  /**
   * A bitflags enum class used to determine what data does a ServoElementSnapshot
   * contain, if either only State, only Attributes, or everything.
   */
  class Flags {
  public:
    typedef uint8_t InternalType;

    enum InternalTypeEnum : InternalType {
      No = 0, // XXX None is a macro on Linux.
      State = 1 << 0,
      Attributes = 1 << 1,
      All = State | Attributes
    };

    /* implicit */
    Flags(enum InternalTypeEnum aFlags)
      : mInternal(aFlags)
    {}

    explicit Flags(InternalType aFlags)
      : mInternal(aFlags)
    {}

    Flags
    operator |(Flags aOther) {
      return Flags(mInternal | aOther.mInternal);
    }

    Flags&
    operator |=(Flags aOther) {
      mInternal |= aOther.mInternal;
      return *this;
    }

    Flags
    operator &(Flags aOther) {
      return Flags(mInternal & aOther.mInternal);
    }

    operator bool() {
      return mInternal != 0;
    }

    // FIXME: These are lame.
    Flags
    operator |(enum InternalTypeEnum aOther) {
      return Flags(mInternal | aOther);
    }

    Flags&
    operator |=(enum InternalTypeEnum aOther) {
      mInternal |= aOther;
      return *this;
    }

    Flags
    operator &(enum InternalTypeEnum aOther) {
      return Flags(mInternal & aOther);
    }

  private:
    InternalType mInternal;
  };

  explicit ServoElementSnapshot(Element* aElement,
                                Flags aWhatToCapture);

  /**
   * Empty snapshot, with no data at all.
   */
  explicit ServoElementSnapshot()
    : mContains(0)
    , mState(0)
    , mExplicitRestyleHint(nsRestyleHint(0))
    , mExplicitChangeHint(nsChangeHint(0))
  {}

  bool HasAttrs() {
    return HasAny(Flags::Attributes);
  }

  bool HasState() {
    return HasAny(Flags::State);
  }

  void Add(Element* aElement,
           Flags aWhatToCapture,
           nsRestyleHint aRestyleHint,
           nsChangeHint aMinChangeHint);

  /**
   * Captures the given element state (if not previously captured).
   *
   * Equivalent to call Add(aElement, Flags::State).
   */
  void AddState(Element* aElement);

  /**
   * Captures the given element attributes (if not previously captured).
   *
   * Equivalent to call Add(aElement, Flags::Attributes).
   */
  void AddAttrs(Element* aElement);

  nsRestyleHint RestyleHint() { return mExplicitRestyleHint; }

  nsChangeHint ChangeHint() { return mExplicitChangeHint; }

private:
  bool HasAny(Flags aFlags) {
    return mContains & aFlags;
  }

  // TODO: Profile, a 1 or 2 element AutoTArray could be worth it, given we know
  // we're dealing with attribute changes when we take snapshots of attributes,
  // though it can be wasted space if we deal with a lot of state-only
  // snapshots.
  Flags mContains;
  nsTArray<ServoAttrSnapshot> mAttrs;
  ServoStateType mState;
  nsRestyleHint mExplicitRestyleHint;
  nsChangeHint mExplicitChangeHint;
};

} // namespace mozilla
#endif
