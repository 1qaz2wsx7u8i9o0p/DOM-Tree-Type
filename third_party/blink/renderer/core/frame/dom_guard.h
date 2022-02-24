// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DOM_GUARD_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DOM_GUARD_H_

#include "base/feature_list.h"
#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class ComputedStyle;
class CSSParserContext;
enum class CSSPropertyID;
class CSSValue;
class CSSProperty;
class Document;
class Element;
enum class FrameDetachType;
class LocalFrame;
class Node;
class PropertyHandle;
class QualifiedName;
class HTMLDocumentParser;
using NodeVector = HeapVector<Member<Node>, 11>;  // Dirty hack, need to be synced with kInitialNodeVectorSize in container_node.h

namespace probe {
  class ParseHTML;
}  // namespace probe

class CORE_EXPORT DOMGuard : public GarbageCollected<DOMGuard> {
 public:
  void WillInsertDOMNodeExtended(Node*, Node*, Node*, bool&);
  void WillModifyDOMAttrExtended(Element*, const QualifiedName&, const AtomicString&, const AtomicString&, bool&);
  void WillRemoveDOMNodeExtended(Node*, bool&);
  void WillSetStyle(Element*, const ComputedStyle*, bool&);
  void FrameAttachedToParent(LocalFrame*);
  void DidParseHTML(Document*, HTMLDocumentParser*);

  void Will(const probe::ParseHTML& probe);
  void Did(const probe::ParseHTML& probe);

  virtual void Trace(Visitor*) const;

  void Shutdown();
  explicit DOMGuard(LocalFrame*);
  virtual ~DOMGuard();

  DISALLOW_COPY_AND_ASSIGN(DOMGuard);

private:
  enum ShadowTreeMatchResult {
    Found = 0,
    NotFound = 1,
    RootIsNotDocument = 2,
    WhitelistMatch = 3,
  };

  bool stringEquals(const String&, wtf_size_t, const String&, wtf_size_t);
  bool stringEquals(const AtomicString&, wtf_size_t, const AtomicString&, wtf_size_t);
  bool scriptEquals(const String& shadow_string, const String& actual_string);
  bool idEquals(const AtomicString&, const AtomicString&, const String&);
  bool attributeEquals(Element*, const AtomicString&, const AtomicString&, const AtomicString&);
  void cssValueEquals(const CSSProperty&, const CSSValue*, const CSSValue*, const CSSParserContext*, int&);
  void cssValueEquals(const CSSProperty&, const String&, const CSSValue*, const CSSParserContext*, int&);
  bool propertyEquals(Element*, const CSSProperty&, const AtomicString&, const CSSValue*, const CSSParserContext*);
  bool urlEquals(const KURL&, const KURL&);
  bool urlEquals(const Vector<KURL>&, const KURL&);

  Node* locateNodeInShadowTree(Node*, ShadowTreeMatchResult&);
  Node* locateNodeAndCreateAncestorsInShadowTree(Node*, ShadowTreeMatchResult&);
  void createShadowNode(Document*, Element*, Node*);
  bool shouldMonitorAttribute(const Element*, const QualifiedName&);
  bool isScriptAttribute(const Element*, const AtomicString&);
  bool isURLAttribute(const Element*, const AtomicString&);
  bool isEqualInShadowTree(Element*, Element*);
  AtomicString escapeAndAddToAttributeValue(const AtomicString&, const AtomicString&);
  AtomicString mergeShadowAttribute(Element*, const AtomicString&, const AtomicString&, const AtomicString&);
  AtomicString mergeShadowProperty(Element*, const CSSProperty&, const AtomicString&, const CSSValue*, const CSSParserContext*);
  bool hasMatchingSubtreeInShadowTree(Node*, Node*);
  bool hasMatchingNodeInShadowTree(Node*, Node*);
  bool matchesNodeWhitelistInShadowTree(Node*, Node*);
  bool matchesAttributeWhitelistInShadowTree(Element*, const AtomicString&, const AtomicString&, Node*);
  bool matchesPropertyWhitelistInShadowTree(Element*, Element*, const ComputedStyle*, bool);
  Node* matchingNode(Node*, Node*);
  bool isDescendantOfUserAgentShadowRoot(Node*);
  void collectStyleChanges(Element*, const ComputedStyle*, const ComputedStyle*, bool);

  void outputElementInsertion(Element*, Element*);
  void outputAttributeModification(Element*, const AtomicString&, const AtomicString&);
  void outputPropertyModification(Element*, const String&, const CSSValue*);

  void executePendingAttributeChanges(Node *node);

  Member<LocalFrame> local_root_;
  Vector<CSSPropertyID> css_property_ids_;
  HeapVector<Member<CSSValue>> css_property_values_;
  Vector<bool> is_css_property_modified_;
  int modified_property_count_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DOM_GUARD_H_
