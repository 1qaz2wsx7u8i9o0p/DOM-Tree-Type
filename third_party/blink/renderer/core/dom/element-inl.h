// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_INL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_INL_H_

#include "third_party/blink/renderer/core/dom/element.h"

#include "third_party/blink/renderer/core/dom/element_rare_data.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"

namespace blink {

inline ElementRareData* Element::GetElementRareData() const {
  DCHECK(HasRareData());
  return static_cast<ElementRareData*>(RareData());
}

inline ElementRareData& Element::EnsureElementRareData() {
  return static_cast<ElementRareData&>(EnsureRareData());
}

inline void Element::SynchronizeAttribute(const QualifiedName& name) const {
  if (!GetElementData())
    return;
  if (UNLIKELY(name == html_names::kStyleAttr &&
               GetElementData()->style_attribute_is_dirty())) {
    DCHECK(IsStyledElement());
    SynchronizeStyleAttributeInternal();
    return;
  }
  if (UNLIKELY(GetElementData()->svg_attributes_are_dirty())) {
    // See comment in the AtomicString version of SynchronizeAttribute()
    // also.
    To<SVGElement>(this)->SynchronizeSVGAttribute(name);
  }
}

ALWAYS_INLINE void Element::SetAttributeInternal(
    wtf_size_t index,
    const QualifiedName& name,
    const AtomicString& new_value,
    SynchronizationOfLazyAttribute in_synchronization_of_lazy_attribute,
    bool delay_attribute_changed) {
  if (new_value.IsNull()) {
    if (index != kNotFound)
      RemoveAttributeInternal(index, in_synchronization_of_lazy_attribute, delay_attribute_changed);
    return;
  }

  if (index == kNotFound) {
    AppendAttributeInternal(name, new_value,
                            in_synchronization_of_lazy_attribute, delay_attribute_changed);
    return;
  }

  const Attribute& existing_attribute =
      GetElementData()->Attributes().at(index);
  AtomicString existing_attribute_value = existing_attribute.Value();
  QualifiedName existing_attribute_name = existing_attribute.GetName();

  if (!in_synchronization_of_lazy_attribute) {
    WillModifyAttribute(existing_attribute_name, existing_attribute_value,
                        new_value);
  }
  if (new_value != existing_attribute_value)
    EnsureUniqueElementData().Attributes().at(index).SetValue(new_value);
  if (!in_synchronization_of_lazy_attribute) {
    DidModifyAttribute(existing_attribute_name, existing_attribute_value,
                       new_value, delay_attribute_changed);
  }
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_INL_H_
