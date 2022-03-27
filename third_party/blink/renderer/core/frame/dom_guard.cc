#include "third_party/blink/renderer/core/frame/dom_guard.h"

#include "third_party/blink/renderer/core/core_probe_sink.h"
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_property_equality.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/css/css_uri_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/v8_scanner/scanner.h"
#include "third_party/blink/renderer/core/frame/v8_scanner/scanner-character-streams.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/html/parser/html_tree_builder.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"

namespace blink {

bool DOMGuard::stringEquals(const String& shadow_string, wtf_size_t shadow_start_position, const String& actual_string, wtf_size_t actual_start_position) {
  wtf_size_t shadow_ptr = shadow_start_position;
  wtf_size_t actual_ptr = actual_start_position;
  bool is_escaped_character = false;
  while (true) {
    if (shadow_ptr == shadow_string.length()) {
      return actual_ptr == actual_string.length();
    } else if (actual_ptr == actual_string.length()) {
      return shadow_string[shadow_ptr] == '*' && shadow_ptr == shadow_string.length() - 1;
    }

    bool ignore_control_character = false;
    if (is_escaped_character) {
      ignore_control_character = true;
      is_escaped_character = false;
    } else {
      if (shadow_string[shadow_ptr] == '\\') {
        is_escaped_character = true;
        shadow_ptr += 1;
        continue;
      } else if (shadow_string[shadow_ptr] == '*') {
        return stringEquals(shadow_string, shadow_ptr + 1, actual_string, actual_ptr) || stringEquals(shadow_string, shadow_ptr, actual_string, actual_ptr + 1);
      }
    }

    if (shadow_string[shadow_ptr] == actual_string[actual_ptr] || (shadow_string[shadow_ptr] == '?' && !ignore_control_character)) {
      shadow_ptr += 1;
      actual_ptr += 1;
    } else {
      return false;
    }
  }
}

bool DOMGuard::stringEquals(const AtomicString& shadow_string, wtf_size_t shadow_start_position, const AtomicString& actual_string, wtf_size_t actual_start_position) {
  return stringEquals(shadow_string.GetString(), shadow_start_position, actual_string.GetString(), actual_start_position);
}

bool DOMGuard::scriptEquals(const String& shadow_string, const String& actual_string) {
  uint16_t shadow_16[shadow_string.length() + 1];
  if (shadow_string.Is8Bit()) {
    for (unsigned int i = 0; i < shadow_string.length(); ++i) {
      shadow_16[i] = shadow_string.Characters8()[i];
    }
  } else {
    for (unsigned int i = 0; i < shadow_string.length(); ++i) {
      shadow_16[i] = shadow_string.Characters16()[i];
    }
  }
  shadow_16[shadow_string.length()] = 0;
  std::unique_ptr<::v8_scanner::Utf16CharacterStream> shadow_stream = ::v8_scanner::ScannerStream::ForTesting(shadow_16, shadow_string.length());
  std::unique_ptr<::v8_scanner::Scanner> shadow_scanner = std::unique_ptr<::v8_scanner::Scanner>(new ::v8_scanner::Scanner(shadow_stream.get()));
  shadow_scanner->Initialize();

  uint16_t actual_16[actual_string.length() + 1];
  if (actual_string.Is8Bit()) {
    for (unsigned int i = 0; i < actual_string.length(); ++i) {
      actual_16[i] = actual_string.Characters8()[i];
    }
  } else {
    for (unsigned int i = 0; i < actual_string.length(); ++i) {
      actual_16[i] = actual_string.Characters16()[i];
    }
  }
  actual_16[actual_string.length()] = 0;
  std::unique_ptr<::v8_scanner::Utf16CharacterStream> actual_stream = ::v8_scanner::ScannerStream::ForTesting(actual_16, actual_string.length());
  std::unique_ptr<::v8_scanner::Scanner> actual_scanner = std::unique_ptr<::v8_scanner::Scanner>(new ::v8_scanner::Scanner(actual_stream.get()));
  actual_scanner->Initialize();

  do {
    if (shadow_scanner->Next() != actual_scanner->Next()) {
      return false;
    }
  } while (shadow_scanner->current_token() != ::v8_scanner::Token::EOS && shadow_scanner->current_token() != ::v8_scanner::Token::ILLEGAL 
            && actual_scanner->current_token() != ::v8_scanner::Token::EOS && actual_scanner->current_token() != ::v8_scanner::Token::ILLEGAL);
  return true;
}

bool DOMGuard::idEquals(const AtomicString& shadow_string, const AtomicString& actual_string, const String& dom_constraint_mode) {
  bool prefix_match = false;
  if (dom_constraint_mode.length() > 1) {
    Vector<String> prefixes;
    dom_constraint_mode.Substring(1).Split(" ", false, prefixes);
    for (const String& prefix : prefixes) {
      if (shadow_string.StartsWith(prefix) && actual_string.StartsWith(prefix)) {
        prefix_match = true;
        break;
      }
    }
  }
  return prefix_match || stringEquals(shadow_string, 0, actual_string, 0);
}

bool DOMGuard::urlEquals(const KURL& url_constraint, const KURL& new_url) {
  if (!new_url.IsValid()) {
    return true;
  }

  return new_url.Protocol() == url_constraint.Protocol() && stringEquals(DecodeURLEscapeSequences(url_constraint.Host(), url::DecodeURLMode::kUTF8), 0, DecodeURLEscapeSequences(new_url.Host(), url::DecodeURLMode::kUTF8), 0) && new_url.Port() == url_constraint.Port();
}

bool DOMGuard::urlEquals(const Vector<KURL>& url_constraints, const KURL& new_url) {
  if (!new_url.IsValid()) {
    return true;
  }
  if (url_constraints.size() == 0) {
    return false;
  }
  // Consider implementing a SOP here? Or a same-site check similar to the one used in site isolation?
  // Here we infer a SOP constraint from a list of KURLs (i.e. url_constraints).

  for (Vector<KURL>::const_iterator it = url_constraints.begin(); it != url_constraints.end(); ++it) {
    if (new_url.Protocol() != it->Protocol()) {
      continue;
    }
    if (new_url.ProtocolIsJavaScript()) {
      url::Component new_url_content = new_url.GetParsed().GetContent();
      url::Component it_content = it->GetParsed().GetContent();

      if (scriptEquals(DecodeURLEscapeSequences(it->GetString().Substring(it_content.begin, it_content.len), url::DecodeURLMode::kUTF8), DecodeURLEscapeSequences(new_url.GetString().Substring(new_url_content.begin, new_url_content.len), url::DecodeURLMode::kUTF8))) {
        return true;
      }
    } else if (new_url.Port() == it->Port() && stringEquals(DecodeURLEscapeSequences(it->Host(), url::DecodeURLMode::kUTF8), 0, DecodeURLEscapeSequences(new_url.Host(), url::DecodeURLMode::kUTF8), 0)) {
      return true;
    }
  }
  return false;
}

bool DOMGuard::isScriptAttribute(const Element* element, const AtomicString& attribute_name) {
  Attribute attribute(QualifiedName(g_null_atom, attribute_name, g_null_atom), g_null_atom);
  if (attribute.GetName().NamespaceURI().IsNull() && attribute.GetName().LocalName().StartsWith("on")) {
    return true;
  }
  return false;
}

bool DOMGuard::isURLAttribute(const Element* element, const AtomicString& attribute_name) {
  Attribute attribute(QualifiedName(g_null_atom, attribute_name, g_null_atom), g_null_atom);
  if (element->IsURLAttribute(attribute)) {
    return true;
  } else if (element->tagName() == "A" && attribute_name == "ping") {
    return true;
  } else if (element->tagName() == "WEBVIEW" && attribute_name == "src") {
    return true;
  }
  return false;
}

bool DOMGuard::attributeEquals(Element *element, const AtomicString& attribute_name, const AtomicString& shadow_attribute_value, const AtomicString& attribute_value) {
  // TODO: should we consider `g_null_atom` equal to `g_empty_atom`?
  if (shadow_attribute_value == g_null_atom) {
    return attribute_value == g_null_atom;
  }
  wtf_size_t shadow_attribute_value_length = shadow_attribute_value.length();
  bool found_in_shadow_attribute_value = false;
  bool is_escaped_character = false;
  StringBuilder unescaped_current_part_builder;
  
  Vector<KURL> url_constraints;

  String dom_constraint_mode = element->GetDocument().GetFrame() ? element->GetDocument().GetFrame()->DOMConstraintMode() : "r";

  for (wtf_size_t i = 0; i < shadow_attribute_value_length; ++i) {
    if (is_escaped_character) {
      is_escaped_character = false;
      unescaped_current_part_builder.Append(shadow_attribute_value[i]);
    } else {
      if (shadow_attribute_value[i] == '\\') {
        is_escaped_character = true;
      } else if (shadow_attribute_value[i] == '|') {
        if (attribute_name == "dtt-id" || attribute_name == "id") {
          if (idEquals(unescaped_current_part_builder.ToAtomicString(), attribute_value, dom_constraint_mode)) {
            found_in_shadow_attribute_value = true;
            break;
          }
        } else if (isScriptAttribute(element, attribute_name)) {
          if (scriptEquals(unescaped_current_part_builder.ToString(), attribute_value)) {
            found_in_shadow_attribute_value = true;
            break;
          }
        } else if (isURLAttribute(element, attribute_name)) {
          url_constraints.push_back(KURL(unescaped_current_part_builder.ToString()));
        } else if (stringEquals(unescaped_current_part_builder.ToAtomicString(), 0, attribute_value, 0)) {
          found_in_shadow_attribute_value = true;
          break;
        }
        unescaped_current_part_builder.Clear();
      } else {
        unescaped_current_part_builder.Append(shadow_attribute_value[i]);
      }
    }
  }
  if (attribute_name == "dtt-id" || attribute_name == "id") {
    return found_in_shadow_attribute_value || idEquals(unescaped_current_part_builder.ToAtomicString(), attribute_value, dom_constraint_mode);
  } else if (isScriptAttribute(element, attribute_name)) {
    return found_in_shadow_attribute_value || scriptEquals(unescaped_current_part_builder.ToString(), attribute_value);
  } else if (isURLAttribute(element, attribute_name)) {
    url_constraints.push_back(KURL(unescaped_current_part_builder.ToString()));
    return urlEquals(url_constraints, KURL(attribute_value));
  } else {
    return found_in_shadow_attribute_value || stringEquals(unescaped_current_part_builder.ToAtomicString(), 0, attribute_value, 0);
  }
}

void DOMGuard::cssValueEquals(const CSSProperty& property, const CSSValue* shadow_css_value, const CSSValue* actual_css_value, const CSSParserContext* parser_context, int& match_state) {
  if (shadow_css_value->GetClassType() == actual_css_value->GetClassType()) {
    if (shadow_css_value->IsValueList()) {
      const CSSValueList *shadow_css_value_list = DynamicTo<CSSValueList>(shadow_css_value);
      const CSSValueList *actual_css_value_list = DynamicTo<CSSValueList>(actual_css_value);
      if (shadow_css_value_list->value_list_separator_ != actual_css_value->value_list_separator_) {
        return;
      }
      if (shadow_css_value_list->length() != actual_css_value_list->length()) {
        return;
      }
      for (wtf_size_t i = 0; i < shadow_css_value_list->length(); ++i) {
        cssValueEquals(property, &shadow_css_value_list->Item(i), &actual_css_value_list->Item(i), parser_context, match_state);
        if (match_state != -1) {
          return;
        }
      }
      match_state = -1;
    } else if (shadow_css_value->IsNumericLiteralValue()) {
      const CSSNumericLiteralValue *shadow_css_numeric_literal_value = DynamicTo<CSSNumericLiteralValue>(shadow_css_value);
      const CSSNumericLiteralValue *actual_css_numeric_literal_value = DynamicTo<CSSNumericLiteralValue>(actual_css_value);
      if (shadow_css_numeric_literal_value->DoubleValue() == actual_css_numeric_literal_value->DoubleValue()) {
        match_state = -1;
      } else if (shadow_css_numeric_literal_value->DoubleValue() > actual_css_numeric_literal_value->DoubleValue()) {
        if (match_state == 0) {
          match_state = 1;
        } else if (match_state == 2) {
          match_state = -1;
        }
      } else {
        if (match_state == 0) {
          match_state = 2;
        } else if (match_state == 1) {
          match_state = -1;
        }
      }
    } else if (shadow_css_value->IsURIValue()) {
      const cssvalue::CSSURIValue *shadow_css_uri_value = DynamicTo<cssvalue::CSSURIValue>(shadow_css_value);
      const cssvalue::CSSURIValue *actual_css_uri_value = DynamicTo<cssvalue::CSSURIValue>(actual_css_value);
      if (urlEquals(shadow_css_uri_value->AbsoluteUrl(), actual_css_uri_value->AbsoluteUrl())) {
        match_state = -1;
      }
    } else if (shadow_css_value->IsImageValue()) {
      const CSSImageValue *shadow_css_image_value = DynamicTo<CSSImageValue>(shadow_css_value);
      const CSSImageValue *actual_css_image_value = DynamicTo<CSSImageValue>(actual_css_value);
      if (urlEquals(KURL(shadow_css_image_value->Url()), KURL(actual_css_image_value->Url()))) {
        match_state = -1;
      }
    } else if (shadow_css_value->IsColorValue()) {
      if (actual_css_value->IsColorValue()) {
        match_state = -1;
      }
    }
  }
}

void DOMGuard::cssValueEquals(const CSSProperty& property, const String& shadow_css_text, const CSSValue* actual_css_value, const CSSParserContext* parser_context, int& match_state) {
  if (shadow_css_text.length() == 0) {
    if (actual_css_value == nullptr) {
      match_state = -1;
    }
    return;
  }
  if (actual_css_value == nullptr) {
    match_state = 0;
    return;
  }

  if (stringEquals(shadow_css_text, 0, actual_css_value->CssText(), 0)) {
    match_state = -1;
  } else {
    const CSSValue *shadow_css_value = CSSParser::ParseSingleValue(property.PropertyID(), shadow_css_text, parser_context);
    if (shadow_css_value) {
      cssValueEquals(property, shadow_css_value, actual_css_value, parser_context, match_state);
    }
  }
}

bool DOMGuard::propertyEquals(Element *element, const CSSProperty& property, const AtomicString& current_value, const CSSValue* new_value, const CSSParserContext* parser_context) {
  if (current_value == g_null_atom) {
    return new_value == nullptr;
  }
  wtf_size_t current_value_length = current_value.length();
  bool is_escaped_character = false;
  StringBuilder unescaped_current_part_builder;
  int match_state = 0;
  for (wtf_size_t i = 0; i < current_value_length; ++i) {
    if (is_escaped_character) {
      is_escaped_character = false;
      unescaped_current_part_builder.Append(current_value[i]);
    } else {
      if (current_value[i] == '\\') {
        is_escaped_character = true;
      } else if (current_value[i] == '|') {
        cssValueEquals(property, unescaped_current_part_builder.ToString(), new_value, parser_context, match_state);
        if (match_state == -1) {
          return true;
        }
        unescaped_current_part_builder.Clear();
      } else {
        unescaped_current_part_builder.Append(current_value[i]);
      }
    }
  }

  cssValueEquals(property, unescaped_current_part_builder.ToString(), new_value, parser_context, match_state);
  if (match_state == -1) {
    return true;
  } else {
    return false;
  }
}
bool DOMGuard::isEqualInShadowTree(Element* shadow, Element* actual) {
  if (shadow->tagName() != actual->tagName()) {
    return false;
  } else if (!attributeEquals(actual, "dtt-id", shadow->getAttribute("dtt-id"), actual->GetIdAttribute())) {
    return false;
  }
  return true;
}

void DOMGuard::createShadowNode(Document* dom_constraint, Element* shadow_ptr, Node* node) {
  auto *document_fragment = DynamicTo<DocumentFragment>(node);
  if (document_fragment) {
    for (auto *child = node->firstChild(); child; child = child->nextSibling()) {
      createShadowNode(dom_constraint, shadow_ptr, child);
    }
    return;
  }

  Element *element = DynamicTo<Element>(node);
  if (!element) { // we don't create shadow for non-element nodes for now, as they are flat and usually benign. 
    return;
  }

  Element *shadow_element = nullptr;
  for (auto *sibling = shadow_ptr->firstChild(); sibling; sibling = sibling->nextSibling()) {
    Element *sibling_element = DynamicTo<Element>(sibling);
    if (!sibling_element) {
      continue;
    }
    if (isEqualInShadowTree(sibling_element, element)) {
      shadow_element = sibling_element;
      break;
    }
  }

  if (!shadow_element) {
    shadow_element = dom_constraint->CreateRawElement(QualifiedName(g_null_atom, AtomicString(element->tagName()), g_null_atom));
    for (const Attribute& attribute : element->Attributes()) {
      if (attribute.GetName().LocalName() == "id") {
        shadow_element->setAttribute("dtt-id", attribute.Value());
      }
      if (shouldMonitorAttribute(element, attribute.GetName())) {
        shadow_element->setAttribute(attribute.GetName(), attribute.Value());
      }
    }

    if (shadow_ptr->tagName() == "HTML" && element->tagName() != "HEAD" && element->tagName() != "BODY") {
      shadow_element->setAttribute("dtt-dangling", "");
    }
    
    shadow_ptr->appendChild(shadow_element);
    // outputElementInsertion(shadow_ptr, shadow_element);
  } else {
    for (const Attribute& attribute : element->Attributes()) {
      if (shouldMonitorAttribute(element, attribute.GetName())) {
        shadow_element->setAttribute(attribute.GetName(), mergeShadowAttribute(element, attribute.GetName().LocalName(), shadow_element->getAttribute(attribute.GetName()), attribute.Value()));
      }
    }
  }
  
  for (auto *child = node->firstChild(); child; child = child->nextSibling()) {
    createShadowNode(dom_constraint, shadow_element, child);
  }
}

Node* DOMGuard::locateNodeInShadowTree(Node* node, ShadowTreeMatchResult& result) {
  Node *ptr = node;
  NodeVector ancestors;
  do {
    ancestors.push_back(ptr);
  } while ((ptr = ptr->ParentOrShadowHostNode()));

  auto *root = DynamicTo<Document>(ancestors.back().Get());
  if (!root) {
    result = ShadowTreeMatchResult::RootIsNotDocument;
    return nullptr;
  }
  ancestors.pop_back();

  Node *shadow_ptr = node->GetDocument().GetFrame()->DOMConstraint();
    
  // 1. go through shadow ancestors of `node` in `dom_constraint` until we can no longer find a matching shadow element 
  auto ancestor = ancestors.rbegin();
  for (;ancestor != ancestors.rend(); ++ancestor) {
    auto *ancestor_document_fragment = DynamicTo<DocumentFragment>((*ancestor).Get());
    if (ancestor_document_fragment) {
      continue;
    }
    auto *ancestor_element = DynamicTo<Element>((*ancestor).Get());
    DCHECK(ancestor_element); // A non-Element and non-DocumentFragment ancestor would trigger this DCHECK
    Element *found_child = nullptr;
    for (auto *child = shadow_ptr->firstChild(); child; child = child->nextSibling()) {
      auto *child_element = DynamicTo<Element>(child);
      if (!child_element) {
        continue;
      }
      if (!isEqualInShadowTree(child_element, ancestor_element)) {
        continue;
      }
      found_child = child_element;
      break;
    }

    if (found_child) {
      shadow_ptr = found_child;
      if (found_child->getAttribute("dtt-whitelist") != g_null_atom) {
        if (ancestor + 1 != ancestors.rend()) {
          result = ShadowTreeMatchResult::WhitelistMatch;
        } else {
          result = ShadowTreeMatchResult::Found;
        }
        return shadow_ptr;
      }
    } else {
      break;
    }
  }

  if (ancestor != ancestors.rend()) {
    result = ShadowTreeMatchResult::NotFound;
    return nullptr;
  } else {
    result = ShadowTreeMatchResult::Found;
    return shadow_ptr;
  }
}

Node* DOMGuard::locateNodeAndCreateAncestorsInShadowTree(Node* node, ShadowTreeMatchResult& result) {
  Node *ptr = node;
  NodeVector ancestors;
  do {
    ancestors.push_back(ptr);
  } while ((ptr = ptr->ParentOrShadowHostNode()));

  auto *root = DynamicTo<Document>(ancestors.back().Get());
  if (!root || root != node->GetDocument()) {
    result = ShadowTreeMatchResult::RootIsNotDocument;
    return nullptr;
  }
  ancestors.pop_back();

  Document *dom_constraint = node->GetDocument().GetFrame()->DOMConstraint();
  Node *shadow_ptr = node->GetDocument().GetFrame()->DOMConstraint();
  bool shadow_ptr_is_root = true;
  bool shadow_ptr_is_html = true;
    
  // 1. go through shadow ancestors of `node` in `dom_constraint` until we can no longer find a matching shadow element 
  auto ancestor = ancestors.rbegin();
  for (;ancestor != ancestors.rend(); ++ancestor) {
    auto *ancestor_document_fragment = DynamicTo<DocumentFragment>((*ancestor).Get());
    if (ancestor_document_fragment) {
      continue;
    }
    auto *ancestor_element = DynamicTo<Element>((*ancestor).Get());
    DCHECK(ancestor_element); // A non-Element and non-DocumentFragment ancestor would trigger this DCHECK
    Element *found_child = nullptr;
    for (auto *child = shadow_ptr->firstChild(); child; child = child->nextSibling()) {
      auto *child_element = DynamicTo<Element>(child);
      if (!child_element) {
        continue;
      }
      if (!isEqualInShadowTree(child_element, ancestor_element)) {
        continue;
      }
      found_child = child_element;
      break;
    }

    if (found_child) {
      shadow_ptr = found_child;
      if (shadow_ptr_is_root) {
        shadow_ptr_is_root = false;
      } else if (shadow_ptr_is_html) {
        shadow_ptr_is_html = false;
      }
    } else {
      break;
    }
  }

  // 2. add missing shadow ancestors until we reach `node`
  for (;ancestor != ancestors.rend(); ++ancestor) {
    auto *ancestor_document_fragment = DynamicTo<DocumentFragment>((*ancestor).Get());
    if (ancestor_document_fragment) {
      continue;
    }
    auto *ancestor_element = DynamicTo<Element>((*ancestor).Get());
    DCHECK(ancestor_element); // A non-Element and non-DocumentFragment ancestor would trigger this DCHECK
    Element *shadow_element = dom_constraint->CreateRawElement(QualifiedName(g_null_atom, AtomicString(ancestor_element->tagName()), g_null_atom));
    shadow_element->setAttribute("dtt-id", ancestor_element->GetIdAttribute());
    // Should we also clone other attributes here, similar to createShadowNode?
    // The shadow_element created here is not a shadow of any node being inserted, 
    // rather, it is something already in the DOM tree but previously unknown to us.
    // Therefore, we should not clone them.

    if (shadow_ptr_is_html && shadow_element->tagName() != "HEAD" && shadow_element->tagName() != "BODY") {
      shadow_element->setAttribute("dtt-dangling", "");
    }
    shadow_ptr = shadow_ptr->appendChild(shadow_element);
  }
  result = ShadowTreeMatchResult::Found;
  return shadow_ptr;
}

bool DOMGuard::shouldMonitorAttribute(const Element* element, const QualifiedName& attribute_name) {
  if (attribute_name.LocalName().StartsWith("dtt-")) {
    // "dtt-*" attributes are for internal use only, and should not be merged or shadowed like regular attributes.
    return false;
  } else if (attribute_name.LocalName() == "id") {
    // This changes an element's identifier.
    return true;
  } else if (attribute_name.LocalName() == "name") {
    return true;
  } else if (element->ExpectedTrustedTypeForAttribute(attribute_name) != SpecificTrustedType::kNone) {
    return true;
  } else {
    Attribute attribute(attribute_name, g_null_atom);
    if (isURLAttribute(element, attribute_name.LocalName())) {
      return true;
    } else if (element->IsHTMLContentAttribute(attribute)) {
      return true;
    } else if (element->IsSVGAnimationAttributeSettingJavaScriptURL(attribute)) {
      return true;
    } else if (element->tagName() == "FORM") {
      return attribute_name.LocalName() == "target" || attribute_name.LocalName() == "method";
    }
  }
  return false;
}

AtomicString DOMGuard::escapeAndAddToAttributeValue(const AtomicString& current_value, const AtomicString& new_value) {
  StringBuilder escaped_new_value_builder;
  escaped_new_value_builder.Append(current_value);
  escaped_new_value_builder.Append('|');
  wtf_size_t new_value_length = new_value.length();
  for (wtf_size_t i = 0; i < new_value_length; ++i) {
    if (new_value[i] == '\\') {
      escaped_new_value_builder.Append("\\\\");
    } else if (new_value[i] == '|') {
      escaped_new_value_builder.Append("\\|");
    } else if (new_value[i] == '*') {
      escaped_new_value_builder.Append("\\*");
    } else {
      escaped_new_value_builder.Append(new_value[i]);
    }
  }
  return escaped_new_value_builder.ToAtomicString();
}

AtomicString DOMGuard::mergeShadowAttribute(Element *element, const AtomicString& attribute_name, const AtomicString& current_value, const AtomicString& new_value) {
  if (attributeEquals(element, attribute_name, current_value, new_value)) {
    return current_value;
  }

  // outputAttributeModification(element, attribute_name, new_value);

  return escapeAndAddToAttributeValue(current_value, new_value);
}

AtomicString DOMGuard::mergeShadowProperty(Element *element, const CSSProperty& property, const AtomicString& current_value, const CSSValue* new_value, const CSSParserContext* parser_context) {
  if (current_value.length() == 0) {
    // if (new_value) {
    //   outputPropertyModification(element, property.GetPropertyNameString(), new_value);
    // }
    return new_value ? AtomicString(new_value->CssText()) : g_null_atom;
  }

  if (propertyEquals(element, property, current_value, new_value, parser_context)) {
    return current_value;
  }

  // outputPropertyModification(element, property.GetPropertyNameString(), new_value);
  
  return escapeAndAddToAttributeValue(current_value, new_value ? AtomicString(new_value->CssText()) : g_null_atom);
}

bool DOMGuard::hasMatchingSubtreeInShadowTree(Node *node, Node *shadow_parent) {
  auto *document_fragment = DynamicTo<DocumentFragment>(node);
  if (document_fragment) {
    for (auto *child = node->firstChild(); child; child = child->nextSibling()) {
      if (!hasMatchingSubtreeInShadowTree(child, shadow_parent)) {
        return false;
      }
    }
    return true;
  }

  Element *element = DynamicTo<Element>(node);
  if (!element) {
    return true;
  }

  Node *shadow_node = nullptr;
  for (Node* child = shadow_parent->firstChild(); child; child = child->nextSibling()) {
    if ((shadow_node = matchingNode(node, child))) {
      break;
    }
  }
  if (!shadow_node) {
    LOG(INFO) << "Matching shadow node not found for " << CreateMarkup(node).Utf8();
    shadow_parent->PrintNodePathTo(LOG_STREAM(INFO));
    return false;
  }

  for (Node* child = node->firstChild(); child; child = child->nextSibling()) {
    if (!hasMatchingSubtreeInShadowTree(child, shadow_node)) {
      return false;
    }
  }
  return true;
}

bool DOMGuard::hasMatchingNodeInShadowTree(Node *node, Node *shadow_parent) {
  for (Node* child = shadow_parent->firstChild(); child; child = child->nextSibling()) {
    if (matchingNode(node, child) || hasMatchingNodeInShadowTree(node, child)) {
      return true;
    }
  }
  return false;
}

bool DOMGuard::matchesNodeWhitelistInShadowTree(Node *node, Node *shadow_parent) {
  auto *document_fragment = DynamicTo<DocumentFragment>(node);
  if (document_fragment) {
    for (auto *child = node->firstChild(); child; child = child->nextSibling()) {
      if (!matchesNodeWhitelistInShadowTree(child, shadow_parent)) {
        return false;
      }
    }
    return true;
  }

  if (!DynamicTo<Element>(node)) {
    return true;
  }

  if (!hasMatchingNodeInShadowTree(node, shadow_parent)) {
    LOG(INFO) << "hasMatchingNodeInShadowTree failed";
    LOG(INFO) << CreateMarkup(node).Utf8();
    node->PrintNodePathTo(LOG_STREAM(INFO));
    return false;
  }

  for (Node* child = node->firstChild(); child; child = child->nextSibling()) {
    if (!matchesNodeWhitelistInShadowTree(child, shadow_parent)) {
      return false;
    }
  }
  return true;
}

bool DOMGuard::matchesAttributeWhitelistInShadowTree(Element *element, const AtomicString& attribute_name, const AtomicString& attribute_value, Node *shadow_parent) {
  for (Node* child = shadow_parent->firstChild(); child; child = child->nextSibling()) {
    Element *child_element = DynamicTo<Element>(child);
    if (!child_element) {
      continue;
    } 
    if (attributeEquals(element, attribute_name, child_element->getAttribute(attribute_name), attribute_value) || matchesAttributeWhitelistInShadowTree(element, attribute_name, attribute_value, child)) {
      return true;
    }
  }
  return false;
}

bool DOMGuard::matchesPropertyWhitelistInShadowTree(Element *element, Element *shadow_parent, const ComputedStyle *style, bool slow_path = false) {
  for (Node* child = shadow_parent->firstChild(); child; child = child->nextSibling()) {
    Element *child_element = DynamicTo<Element>(child);
    if (!child_element) {
      continue;
    }
    int count = 0;
    for (CSSPropertyID property_id : css_property_ids_) {
      if (!is_css_property_modified_[count]) {
        count += 1;
        continue;
      }
      const CSSProperty& property_class = CSSProperty::Get(ResolveCSSPropertyID(property_id));
      const CSSValue* new_value = css_property_values_[count];

      if (slow_path) {
        AtomicString shadow_attribute_name = "dtt-s-" + property_class.GetPropertyNameString();
        if (propertyEquals(element, property_class, child_element->getAttribute(shadow_attribute_name), new_value, element->GetDocument().ElementSheet().Contents()->ParserContext())) {
          is_css_property_modified_[count] = false;
          modified_property_count_ -= 1;
        }
      } else {
        const ComputedStyle* shadow_computed_style = child_element->GetComputedStyle();
        if (shadow_computed_style) {
          int fast_match_result = CSSPropertyEquality::PropertiesEqualForDOMGuard(PropertyHandle(property_class), *shadow_computed_style, *style); 
          if (fast_match_result == 1) {
            is_css_property_modified_[count] = false;
            modified_property_count_ -= 1;
          } else {
            const CSSValue* shadow_css_value = ComputedStyleUtils::ComputedPropertyValue(property_class, *shadow_computed_style);
            String shadow_css_text = shadow_css_value ? shadow_css_value->CssText() : "";
            String new_css_text = new_value ? new_value->CssText() : "";
            if (shadow_css_text == new_css_text) {
              is_css_property_modified_[count] = false;
              modified_property_count_ -= 1;
            }
          }
        }
      }
      count += 1;
    }
    if (modified_property_count_ == 0 || matchesPropertyWhitelistInShadowTree(element, child_element, style, slow_path)) {
      return true;
    }
  }
  return false;
}

Node* DOMGuard::matchingNode(Node *node, Node *shadow_node) {
  Element *element = DynamicTo<Element>(node);
  Element *shadow_element = DynamicTo<Element>(shadow_node);

  if (!shadow_element) { // shadow_node can be a text node
    return nullptr;
  }

  if (element->tagName() != shadow_element->tagName()) {
    return nullptr;
  }

  if (!attributeEquals(element, "dtt-id", shadow_element->getAttribute("dtt-id"), element->GetIdAttribute())) {
    return nullptr;
  }

  for (const Attribute& attribute : element->Attributes()) {
    if (!shouldMonitorAttribute(element, attribute.GetName())) {
      continue;
    }

    if (!attributeEquals(element, attribute.GetName().LocalName(), shadow_element->getAttribute(attribute.GetName()), attribute.Value())) {
      return nullptr;
    }
  }

  // TODO: Sometimes `element` should be required to have a certain attribute with a certain value (e.g. `<a target="some_window"`).
  // We should maintain a list of such attributes, and iterate through them here.

  return shadow_node;
}

bool DOMGuard::isDescendantOfUserAgentShadowRoot(Node* node) {
  do {
    ShadowRoot *shadow_root = DynamicTo<ShadowRoot>(node);
    if (shadow_root && shadow_root->IsUserAgent()) {
      return true;
    }
  } while ((node = node->ParentOrShadowHostNode()));

  return false;
}

void DOMGuard::WillInsertDOMNodeExtended(Node* parent, Node *node, Node *next, bool &allowed) {
  allowed = true;

  // parent->PrintNodePathTo(LOG_STREAM(INFO));

  if (!parent->GetDocument().domWindow()) {
    return;
  }

  // LOG(INFO) << "1";
  
  if (isDescendantOfUserAgentShadowRoot(parent)) {
    executePendingAttributeChanges(node);
    return;
  }

  // LOG(INFO) << "2";

  DocumentParser *parser = parent->GetDocument().Parser();
  if (parser && parser->IsParsing()) {
    executePendingAttributeChanges(node);
    return;
  }

  // LOG(INFO) << "3";

  String dom_constraint_mode = parent->GetDocument().GetFrame()->DOMConstraintMode();
  if (dom_constraint_mode.length() && dom_constraint_mode[0] == 'r') {
  // if (dom_constraint_mode == "record") {
    ShadowTreeMatchResult match_result = ShadowTreeMatchResult::NotFound;
    Element *shadow_ptr = DynamicTo<Element>(locateNodeAndCreateAncestorsInShadowTree(parent, match_result));
    // LOG(INFO) << "match_result = " << match_result;
    if (match_result != ShadowTreeMatchResult::Found) {
      return;
    }

    // 3. create a shadow of `node` under `shadow_ptr`
    Document *dom_constraint = parent->GetDocument().GetFrame()->DOMConstraint();
    createShadowNode(dom_constraint, shadow_ptr, node);
    executePendingAttributeChanges(node);
  } else if (dom_constraint_mode.length() && dom_constraint_mode[0] == 'e') {
  // } else if (dom_constraint_mode == "enforce") {
    ShadowTreeMatchResult match_result = ShadowTreeMatchResult::NotFound;
    Node *shadow_parent = locateNodeInShadowTree(parent, match_result);

    if (match_result == ShadowTreeMatchResult::RootIsNotDocument) {
      allowed = true;
    } else if (match_result == ShadowTreeMatchResult::Found) {
      allowed = hasMatchingSubtreeInShadowTree(node, shadow_parent);
    } else if (match_result == ShadowTreeMatchResult::WhitelistMatch) {
      allowed = matchesNodeWhitelistInShadowTree(node, shadow_parent);
    } else {
      allowed = false;
    }
    if (!allowed) {
      LOG(INFO) << "InsertDOMNode rejected, match_result = " << match_result; 
      parent->PrintNodePathTo(LOG_STREAM(INFO));
      // Element *parent_element = DynamicTo<Element>(parent);
      // LOG(INFO) << CreateMarkup(parent).Utf8();
      // LOG(INFO) << parent_element->tagName() << " "  << parent_element->getAttribute("id");
    } else if (match_result != ShadowTreeMatchResult::RootIsNotDocument) {
      executePendingAttributeChanges(node);
    }
  }
}

void DOMGuard::WillModifyDOMAttrExtended(Element* element,
                                const QualifiedName& name,
                                const AtomicString& old_value,
                                const AtomicString& new_value,
                                bool &allowed) {
  allowed = true;

  if (!element->GetDocument().domWindow()) {
    return;
  }

  if (isDescendantOfUserAgentShadowRoot(element)) {
    return;
  }

  if (!shouldMonitorAttribute(element, name)) {
    return;
  }

  String dom_constraint_mode = element->GetDocument().GetFrame()->DOMConstraintMode();
  if (dom_constraint_mode.length() && dom_constraint_mode[0] == 'r') {
  // if (dom_constraint_mode == "record") {
    ShadowTreeMatchResult match_result = ShadowTreeMatchResult::NotFound;
    Element *shadow_ptr = DynamicTo<Element>(locateNodeAndCreateAncestorsInShadowTree(element, match_result));
    if (match_result != ShadowTreeMatchResult::Found) {
      return;
    }
    shadow_ptr->setAttribute(name, mergeShadowAttribute(shadow_ptr, name.LocalName(), shadow_ptr->getAttribute(name), new_value));
  } else if (dom_constraint_mode.length() && dom_constraint_mode[0] == 'e') {
  // } else if (dom_constraint_mode == "enforce") {
    ShadowTreeMatchResult match_result = ShadowTreeMatchResult::NotFound;
    Element *shadow_ptr = DynamicTo<Element>(locateNodeInShadowTree(element, match_result));

    if (match_result == ShadowTreeMatchResult::RootIsNotDocument) {
      allowed = true;
    } else if (match_result == ShadowTreeMatchResult::Found) {
      allowed = attributeEquals(element, name.LocalName(), shadow_ptr->getAttribute(name), new_value);
    } else if (match_result == ShadowTreeMatchResult::WhitelistMatch) {
      allowed = matchesAttributeWhitelistInShadowTree(element, name.LocalName(), new_value, shadow_ptr);
    } else {
      allowed = false;
    }
    if (!allowed) {
      LOG(INFO) << "ModifyDOMAttr rejected, match_result = " << match_result << ", attribute_name = " << name.LocalName().Utf8() << ", attribute_value = " << new_value.Utf8() << ", allowed_values = " << shadow_ptr->getAttribute(name).Utf8();
      element->PrintNodePathTo(LOG_STREAM(INFO));
    }
  }
}

void DOMGuard::WillRemoveDOMNodeExtended(Node* node, bool &allowed) {
  allowed = true;

  if (!node->GetDocument().domWindow()) {
    return;
  }

  if (isDescendantOfUserAgentShadowRoot(node)) {
    return;
  }
}

void DOMGuard::collectStyleChanges(Element *element, const ComputedStyle* current_style, const ComputedStyle* new_style, bool is_whitelist_match = true) {
  int count = -1;
  modified_property_count_ = 0;
  for (CSSPropertyID property_id : css_property_ids_) {
    count += 1;
    is_css_property_modified_[count] = false;
    const CSSProperty& property = CSSProperty::Get(ResolveCSSPropertyID(property_id));
    const CSSValue* new_css_value = ComputedStyleUtils::ComputedPropertyValue(property, *new_style);
    if (is_whitelist_match) {
      css_property_values_[count] = const_cast<CSSValue *>(new_css_value);
    }

    if (!current_style) {
      String new_css_text = new_css_value ? new_css_value->CssText() : "";

      if (new_css_text != "") {
        is_css_property_modified_[count] = true;
        modified_property_count_ += 1;
      }
    } else {
      int fast_match_result = CSSPropertyEquality::PropertiesEqualForDOMGuard(PropertyHandle(property), *current_style, *new_style); 
      if (fast_match_result == 0) {
        is_css_property_modified_[count] = true;
        modified_property_count_ += 1;
      } else if (fast_match_result == -1) {
        const CSSValue* current_css_value = ComputedStyleUtils::ComputedPropertyValue(property, *current_style);
        String current_css_text = current_css_value ? current_css_value->CssText() : "";
        String new_css_text = new_css_value ? new_css_value->CssText() : "";

        if (current_css_text != new_css_text) {
          is_css_property_modified_[count] = true;
          modified_property_count_ += 1;
        }
      }     
    }
  }
}

void DOMGuard::WillSetStyle(Element* element, const ComputedStyle* style, bool& allowed) {
  allowed = true;
  if (!element->GetDocument().domWindow()) { // Moving an element into a DOMWindow always triggers WillSetStyle
    return;
  }

  if (isDescendantOfUserAgentShadowRoot(element)) {
    return;
  }

  String dom_constraint_mode = element->GetDocument().GetFrame()->DOMConstraintMode();
  if (dom_constraint_mode.length() && dom_constraint_mode[0] == 'r') {
    ShadowTreeMatchResult match_result = ShadowTreeMatchResult::NotFound;
    Element *shadow_ptr = DynamicTo<Element>(locateNodeAndCreateAncestorsInShadowTree(element, match_result));
    if (match_result != ShadowTreeMatchResult::Found) {
      return;
    }

    const ComputedStyle* current_style = element->GetComputedStyle();
    collectStyleChanges(element, current_style, style, false);
    int count = 0;
    for (CSSPropertyID property_id : css_property_ids_) {
      if (is_css_property_modified_[count]) {
        const CSSProperty& property = CSSProperty::Get(ResolveCSSPropertyID(property_id));
        AtomicString shadow_attribute_name = "dtt-s-" + property.GetPropertyNameString();
        const CSSValue* new_css_value = ComputedStyleUtils::ComputedPropertyValue(property, *style);
        shadow_ptr->setAttribute(shadow_attribute_name, mergeShadowProperty(shadow_ptr, property, shadow_ptr->getAttribute(shadow_attribute_name), new_css_value, element->GetDocument().ElementSheet().Contents()->ParserContext()));
      }
      count += 1;
    }
  } else if (dom_constraint_mode.length() && dom_constraint_mode[0] == 'e') {
    ShadowTreeMatchResult match_result = ShadowTreeMatchResult::NotFound;
    Element *shadow_ptr = DynamicTo<Element>(locateNodeInShadowTree(element, match_result));
    if (!shadow_ptr) {
      allowed = false;
      return;
    }

    if (match_result == ShadowTreeMatchResult::RootIsNotDocument) {
      allowed = true;
    } else if (match_result == ShadowTreeMatchResult::Found) {
      const ComputedStyle* current_style = element->GetComputedStyle();
      for (CSSPropertyID property_id : css_property_ids_) {
        const CSSProperty& property = CSSProperty::Get(ResolveCSSPropertyID(property_id));
        const CSSValue* new_value = ComputedStyleUtils::ComputedPropertyValue(property, *style);
        String new_css_text = new_value ? new_value->CssText() : "";
        if (!current_style) {
          if (new_css_text == "") {
            continue;
          }
        } else {
          int fast_match_result_current = CSSPropertyEquality::PropertiesEqualForDOMGuard(PropertyHandle(property), *current_style, *style);
          if (fast_match_result_current == 1) {
            continue;
          } else if (fast_match_result_current == -1) {
            const CSSValue* current_css_value = ComputedStyleUtils::ComputedPropertyValue(property, *current_style);
            String current_css_text = current_css_value ? current_css_value->CssText() : "";

            if (current_css_text == new_css_text) {
              continue;
            }
          }
        }
        const ComputedStyle* shadow_computed_style = shadow_ptr->GetComputedStyle();
        if (shadow_computed_style) {
          int fast_match_result_shadow = CSSPropertyEquality::PropertiesEqualForDOMGuard(PropertyHandle(property), *shadow_computed_style, *style); 
          if (fast_match_result_shadow == 1) {
            continue;
          }

          const CSSValue* shadow_css_value = ComputedStyleUtils::ComputedPropertyValue(property, *shadow_computed_style);
          String shadow_css_text = shadow_css_value ? shadow_css_value->CssText() : "";

          if (shadow_css_text == new_css_text) {
            continue;
          }
        }
        AtomicString shadow_attribute_name = "dtt-s-" + property.GetPropertyNameString();
        allowed &= propertyEquals(element, property, shadow_ptr->getAttribute(shadow_attribute_name), new_value, element->GetDocument().ElementSheet().Contents()->ParserContext());
        if (!allowed) {
          LOG(INFO) << "SetStyle rejected, match_result = " << match_result << ", property = " << property.GetPropertyNameString().Utf8() << ", value = " << (new_value ? new_value->CssText().Utf8() : "") << ", allowed_values = " << shadow_ptr->getAttribute(shadow_attribute_name).Utf8();
          element->PrintNodePathTo(LOG_STREAM(INFO));
          return;
        }
      }
    } else if (match_result == ShadowTreeMatchResult::WhitelistMatch) {
      const ComputedStyle* current_style = element->GetComputedStyle();
      collectStyleChanges(element, current_style, style, true);
      allowed = matchesPropertyWhitelistInShadowTree(element, shadow_ptr, style, false);
      if (!allowed) {
        allowed = matchesPropertyWhitelistInShadowTree(element, shadow_ptr, style, true);
      }
      if (!allowed) {
        int count = 0;
        for (CSSPropertyID property_id : css_property_ids_) {
          if (is_css_property_modified_[count]) {
            const CSSProperty& property = CSSProperty::Get(ResolveCSSPropertyID(property_id));
            AtomicString shadow_attribute_name = "dtt-s-" + property.GetPropertyNameString();
            const CSSValue* new_value = ComputedStyleUtils::ComputedPropertyValue(property, *style);
            LOG(INFO) << "SetStyle rejected, match_result = " << match_result << ", property = " << property.GetPropertyNameString().Utf8() << ", value = " << (new_value ? new_value->CssText().Utf8() : "");
          }
          count += 1;
        }
        element->PrintNodePathTo(LOG_STREAM(INFO));
      }
    } else {
      LOG(INFO) << "SetStyle rejected, match_result = " << match_result;
      element->PrintNodePathTo(LOG_STREAM(INFO));
      allowed = false;
    }
  }
}

void DOMGuard::FrameAttachedToParent(LocalFrame* frame) {
  modified_property_count_ = 0;
  css_property_ids_.clear();
  is_css_property_modified_.clear();
  for (CSSPropertyID property_id : CSSPropertyIDList()) {
    const CSSProperty& property = CSSProperty::Get(ResolveCSSPropertyID(property_id));
    if (property.IsWebExposed(frame->DomWindow()) && !property.IsShorthand() && property.IsProperty() && !property.IsLayoutDependentProperty() && !property.IsInternal() && !property.IsSurrogate()) {
      css_property_ids_.push_back(property_id);
      is_css_property_modified_.push_back(false);
      css_property_values_.push_back(nullptr);
    }
  }
  frame->SetDOMConstraintHTML("");
  frame->SetDOMConstraintMode("r");
}

void DOMGuard::DidParseHTML(Document* document, HTMLDocumentParser* parser) {
  if (parser->CanExecuteScript()) {
    return;
  }

  // LOG(INFO) << "DidParseHTML " << CreateMarkup(parser->TreeBuilder()->AttachmentRoot()).Utf8();
}

void DOMGuard::Will(const probe::ParseHTML& probe) {
  if (!probe.parser->CanExecuteScript()) {
    return;
  }

  // LOG(INFO) << "Will(ParseHTML) " << probe.parser->GetDocument()->ExecutingWindow()->Url();
}

void DOMGuard::Did(const probe::ParseHTML& probe) {
  // Do nothing here
}

void DOMGuard::Trace(Visitor* visitor) const {
  visitor->Trace(local_root_);
  visitor->Trace(css_property_values_);
}

DOMGuard::DOMGuard(LocalFrame* local_root)
    : local_root_(local_root) {
  local_root_->GetProbeSink()->AddDOMGuard(this);
}

DOMGuard::~DOMGuard() {
  DCHECK(!local_root_);
}

void DOMGuard::Shutdown() {
  if (!local_root_)
    return;
  
  local_root_->GetProbeSink()->RemoveDOMGuard(this);
  local_root_ = nullptr;
}

void DOMGuard::outputElementInsertion(Element *shadow_ptr, Element *shadow_element) {
  LOG(INFO) << "ELEMENT " << CreateMarkup(shadow_element).Utf8();
  shadow_ptr->PrintNodePathTo(LOG_STREAM(INFO));
}

void DOMGuard::outputAttributeModification(Element* shadow_element, const AtomicString& attribute_name, const AtomicString& attribute_value) {
  LOG(INFO) << "ATTRIBUTE " << attribute_name.Utf8() << " = " << attribute_value.Utf8();
  shadow_element->PrintNodePathTo(LOG_STREAM(INFO));
}

void DOMGuard::outputPropertyModification(Element* shadow_element, const String& property_name, const CSSValue* value) {
  if (!value->MayContainUrl()) {
    return;
  }
  LOG(INFO) << "PROPERTY " << property_name.Utf8() << " = " << value->CssText().Utf8();
  shadow_element->PrintNodePathTo(LOG_STREAM(INFO));
}

void DOMGuard::executePendingAttributeChanges(Node *node) {
  auto *document_fragment = DynamicTo<DocumentFragment>(node);
  if (document_fragment) {
    for (auto *child = node->firstChild(); child; child = child->nextSibling()) {
      executePendingAttributeChanges(child);
    }
    return;
  }

  Element *element = DynamicTo<Element>(node);
  if (!element) {
    return;
  }

  element->ExecutePendingAttributeChanges();

  for (auto *child = node->firstChild(); child; child = child->nextSibling()) {
    executePendingAttributeChanges(child);
  }

  ShadowRoot *shadow_root = element->AuthorShadowRoot();
  if (shadow_root) {
    executePendingAttributeChanges(shadow_root);
  }
}

}  // namespace blink
