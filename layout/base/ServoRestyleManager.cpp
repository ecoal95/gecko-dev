/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoRestyleManager.h"
#include "mozilla/ServoStyleSet.h"

using namespace mozilla::dom;

namespace mozilla {

ServoRestyleManager::ServoRestyleManager(nsPresContext* aPresContext)
  : RestyleManagerBase(aPresContext)
{
}

/* static */ void
ServoRestyleManager::DirtyTree(nsIContent* aContent, bool aIncludingRoot)
{
  if (aIncludingRoot) {
    // XXX: This can in theory leave nodes not dirty, but in practice this is not
    // a problem, at least for now, since right now element dirty implies
    // descendants dirty. Remove this early return if this ever changes.
    if (aContent->IsDirtyForServo()) {
      return;
    }

    aContent->SetIsDirtyForServo();
  }

  FlattenedChildIterator it(aContent);

  nsIContent* n = it.GetNextChild();
  bool hadChildren = bool(n);
  for ( ; n; n = it.GetNextChild()) {
    DirtyTree(n, true);
  }

  if (hadChildren) {
    aContent->SetHasDirtyDescendantsForServo();
  }
}

void
ServoRestyleManager::PostRestyleEvent(Element* aElement,
                                      nsRestyleHint aRestyleHint,
                                      nsChangeHint aMinChangeHint)
{
  if (MOZ_UNLIKELY(IsDisconnected()) ||
      MOZ_UNLIKELY(PresContext()->PresShell()->IsDestroying())) {
    return;
  }

  // NOTE: We defer the processing of restyle/change hints until
  // ProcessPendingRestyles.
  //
  // This snapshot effectively stores nothing (at least yet), except the
  // restyle/change hint.
  //
  // TODO: we could do some space optimisations here it seems, though I don't
  // expect this path to be really hot.
  bool needsRestyle = false;

  if (aRestyleHint || aMinChangeHint) {
    AddElementSnapshot(aElement, ServoElementSnapshot::Flags::No,
                       aRestyleHint, aMinChangeHint);
    needsRestyle = true;
  } else {
    // Note that we could have been called just after adding an element to the
    // table, for example.
    //
    // Other way to structure this would be to duplicate the logic in
    // AddElementSnapshot, at the (very minor) cost of possibly doing things
    // twice.
    needsRestyle = HasPendingRestyles();
  }

  if (needsRestyle) {
    nsIPresShell* presShell = PresContext()->PresShell();
    if (!ObservingRefreshDriver()) {
      SetObservingRefreshDriver(PresContext()->RefreshDriver()->
          AddStyleFlushObserver(presShell));
    }

    presShell->GetDocument()->SetNeedStyleFlush();
  }
}

void
ServoRestyleManager::PostRestyleEventForLazyConstruction()
{
  NS_ERROR("stylo: ServoRestyleManager::PostRestyleEventForLazyConstruction not implemented");
}

void
ServoRestyleManager::RebuildAllStyleData(nsChangeHint aExtraHint,
                                         nsRestyleHint aRestyleHint)
{
  NS_ERROR("stylo: ServoRestyleManager::RebuildAllStyleData not implemented");
}

void
ServoRestyleManager::PostRebuildAllStyleDataEvent(nsChangeHint aExtraHint,
                                                  nsRestyleHint aRestyleHint)
{
  MOZ_CRASH("stylo: ServoRestyleManager::PostRebuildAllStyleDataEvent not implemented");
}

/* static */ void
ServoRestyleManager::RecreateStyleContexts(nsIContent* aContent,
                                           nsStyleContext* aParentContext,
                                           ServoStyleSet* aStyleSet)
{
  nsIFrame* primaryFrame = aContent->GetPrimaryFrame();

  // TODO: AFAIK this can happen when we have, let's say, display: none. Here we
  // should trigger frame construction if the element is actually dirty (I
  // guess), but we'd better do that once we have all the restyle hints thing
  // figured out.
  if (!primaryFrame) {
    aContent->UnsetFlags(NODE_IS_DIRTY_FOR_SERVO | NODE_HAS_DIRTY_DESCENDANTS_FOR_SERVO);
    return;
  }

  if (aContent->IsDirtyForServo()) {
    RefPtr<ServoComputedValues> computedValues =
      dont_AddRef(Servo_GetComputedValues(aContent));

    // TODO: Figure out what pseudos does this content have, and do the proper
    // thing with them.
    RefPtr<nsStyleContext> context =
      aStyleSet->GetContext(computedValues.forget(),
                            aParentContext,
                            nullptr,
                            CSSPseudoElementType::NotPseudo);

    // TODO: Compare old and new styles to generate restyle change hints, and
    // process them.
    primaryFrame->SetStyleContext(context.get());

    aContent->UnsetFlags(NODE_IS_DIRTY_FOR_SERVO);
  }

  if (aContent->HasDirtyDescendantsForServo()) {
    FlattenedChildIterator it(aContent);
    for (nsIContent* n = it.GetNextChild(); n; n = it.GetNextChild()) {
      RecreateStyleContexts(n, primaryFrame->StyleContext(), aStyleSet);
    }
    aContent->UnsetFlags(NODE_HAS_DIRTY_DESCENDANTS_FOR_SERVO);
  }
}

static void
MarkParentsAsHavingDirtyDescendants(Element* aElement)
{
  nsINode* cur = aElement;
  while ((cur = cur->GetParentNode())) {
    if (cur->HasDirtyDescendantsForServo()) {
      break;
    }

    cur->SetHasDirtyDescendantsForServo();
  }
}

void
ServoRestyleManager::NoteRestyleHint(Element* aElement, nsRestyleHint aHint)
{
  if (aHint & eRestyle_Self) {
    aElement->SetIsDirtyForServo();
    MarkParentsAsHavingDirtyDescendants(aElement);

    // XXX Self must imply Subtree, at least for Servo, because of style
    // structs inheritance. Would that be taken care by the SetStyleContext
    // call?
    aHint |= eRestyle_Subtree;
    // MarkParentsAsHavingDirtyDescendants(aElement);
  }

  if (aHint & eRestyle_Subtree) {
    DirtyTree(aElement, /* aIncludingRoot = */ false);
    MarkParentsAsHavingDirtyDescendants(aElement);
  }

  if (aHint & eRestyle_LaterSiblings) {
    for (nsINode* cur = aElement->GetNextSibling(); cur; cur = cur->GetNextSibling()) {
      if (cur->IsContent()) {
        DirtyTree(cur->AsContent(), /* aIncludingRoot = */ true);
      }
    }
  }

  // TODO: detect restyle for animations/transitions/etc, and act properly.
  //
  // The cascade levels there are going to be fun, if we keep the actual
  // mechanism.
}

void
ServoRestyleManager::ProcessPendingRestyles()
{
  if (!HasPendingRestyles()) {
    return;
  }

  ServoStyleSet* styleSet = StyleSet();

  if (!styleSet->InitialRestyleDone()) {
    // If something caused us to restyle, and we haven't done the initial
    // full document restyle, do nothing.
    return;
  }

  nsIDocument* doc = PresContext()->Document();
  Element* root = doc->GetRootElement();
  if (root) {
#ifdef DEBUG // XXX remove this
    printf("\n\nAbout to process a restyle with %zu modified elements\n\n",
           mModifiedElements.Count());
#endif

    for (auto iter = mModifiedElements.Iter(); !iter.Done(); iter.Next()) {
      ServoElementSnapshot* snapshot = iter.UserData();
      Element* element = iter.Key();

      // TODO: avoid this if we already have the highest restyle hint in the
      // subtree.
      nsRestyleHint hint = styleSet->ComputeRestyleHint(element, snapshot);
      hint = hint | snapshot->RestyleHint();
      // nsRestyleHint hint = eRestyle_Self;
      NoteRestyleHint(element, hint);
    }

    styleSet->RestyleSubtree(root, /* aForce = */ false);
    RecreateStyleContexts(root, nullptr, styleSet);
  }

  mModifiedElements.Clear();

  // NB: we restyle from the root element, but the document also gets the
  // HAS_DIRTY_DESCENDANTS flag as part of the loop on PostRestyleEvent, and we
  // use that to check we have pending restyles.
  //
  // Thus, they need to get cleared here.
  MOZ_ASSERT(!doc->IsDirtyForServo());
  doc->UnsetFlags(NODE_HAS_DIRTY_DESCENDANTS_FOR_SERVO);

  IncrementRestyleGeneration();
}

void
ServoRestyleManager::RestyleForInsertOrChange(Element* aContainer,
                                              nsIContent* aChild)
{
  NS_ASSERTION(!aChild->IsRootOfAnonymousSubtree(),
               "anonymous nodes should not be in child lists");

  uint32_t selectorFlags =
    aContainer ? (aContainer->GetFlags() & NODE_ALL_SELECTOR_FLAGS) : 0;
  if (selectorFlags == 0)
    return;

}

void
ServoRestyleManager::RestyleForAppend(Element* aContainer,
                                      nsIContent* aFirstNewContent)
{
  NS_ERROR("stylo: ServoRestyleManager::RestyleForAppend not implemented");
}

void
ServoRestyleManager::RestyleForRemove(Element* aContainer,
                                      nsIContent* aOldChild,
                                      nsIContent* aFollowingSibling)
{
  NS_ERROR("stylo: ServoRestyleManager::RestyleForRemove not implemented");
}

nsresult
ServoRestyleManager::ContentStateChanged(nsIContent* aContent,
                                         EventStates aStateMask)
{
  if (!aContent->IsElement()) {
    return NS_OK;
  }

  Element* aElement = aContent->AsElement();
  nsChangeHint changeHint;
  nsRestyleHint restyleHint;

  // NOTE: restyleHint here is effectively always 0, since that's what
  // ServoStyleSet::HasStateDependentStyle returns. Servo computes on
  // ProcessPendingRestyles using the ElementSnapshot, but in theory could
  // compute it sequentially easily.
  //
  // Determine what's the best way to do it, and how much work do we save
  // processing the restyle hint early (i.e., computing the style hint in
  // ServoStyleSet), vs lazily (snapshot approach), and if we definitely take
  // the second approach, take rid of HasStateDependentStyle, etc.
  //
  // Also, profile whether we save something storing the restyle hint in the
  // table and deferring the dirtiness setting until ProcessPendingRestyles
  // (that's a requirement if we store snapshots though), vs processing the
  // restyle hint in-place, dirtying the nodes on PostRestyleEvent.
  ContentStateChangedInternal(aElement, aStateMask, &changeHint, &restyleHint);

  AddElementSnapshot(aElement, ServoElementSnapshot::Flags::State,
                     restyleHint, changeHint);

  PostRestyleEvent(aElement, restyleHint, changeHint);
  return NS_OK;
}

void
ServoRestyleManager::AttributeWillChange(Element* aElement,
                                         int32_t aNameSpaceID,
                                         nsIAtom* aAttribute,
                                         int32_t aModType,
                                         const nsAttrValue* aNewValue)
{
  NS_ERROR("stylo: ServoRestyleManager::AttributeWillChange not implemented");
}

void
ServoRestyleManager::AttributeChanged(Element* aElement,
                                      int32_t aNameSpaceID,
                                      nsIAtom* aAttribute,
                                      int32_t aModType,
                                      const nsAttrValue* aOldValue)
{
  NS_ERROR("stylo: ServoRestyleManager::AttributeChanged not implemented");
}

nsresult
ServoRestyleManager::ReparentStyleContext(nsIFrame* aFrame)
{
  NS_ERROR("stylo: ServoRestyleManager::ReparentStyleContext not implemented");
  return NS_OK;
}

void
ServoRestyleManager::AddElementSnapshot(Element* aElement,
                                        ServoElementSnapshot::Flags aWhatToCapture)
{
  AddElementSnapshot(aElement,
                     aWhatToCapture,
                     nsRestyleHint(0),
                     nsChangeHint(0));
}

void
ServoRestyleManager::AddElementSnapshot(Element* aElement,
                                        ServoElementSnapshot::Flags aWhatToCapture,
                                        nsRestyleHint aRestyleHint,
                                        nsChangeHint aMinChangeHint)
{
  ServoElementSnapshot* existingSnapshot = mModifiedElements.LookupOrAdd(aElement);
  MOZ_ASSERT(existingSnapshot);

  existingSnapshot->Add(aElement, aWhatToCapture, aRestyleHint, aMinChangeHint);
}

} // namespace mozilla
