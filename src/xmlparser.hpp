#ifndef XMLPARSER_HPP
#define XMLPARSER_HPP

#include <utility>
#include <stack>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <istream>
#include <cctype>
#include <cwctype>
#include <list>

namespace xml {
namespace details {

// Some helpers for resolving the correct standard library functions

template <typename TChar>
bool IsSpace(TChar symbol)
{
   unsigned val = static_cast<unsigned>(symbol);
   return val >= 9 && val <= 13;
}

#define IS_SPACE(func, type)                           \
   template <>                                         \
   inline bool IsSpace<type>(type symbol)              \
   {                                                   \
      return func(static_cast<unsigned type>(symbol)); \
   }

IS_SPACE(std::isspace, char)
IS_SPACE(std::iswspace, wchar_t)

#undef IS_SPACE


template <typename TChar>
bool IsAlpha(TChar symbol)
{
   unsigned val = static_cast<unsigned>(symbol);
   return (val >= 65 && val <= 90) || (val >= 97 && val <= 122);
}

#define IS_ALPHA(func, type)                           \
   template <>                                         \
   inline bool IsAlpha<type>(type symbol)              \
   {                                                   \
      return func(static_cast<unsigned type>(symbol)); \
   }

IS_ALPHA(std::isalpha, char)
IS_ALPHA(std::iswalpha, wchar_t)

#undef IS_ALPHA

// Tables for different encodings, mapping entity references to ascii symbols
template <typename TChar>
const TChar **EntityRefTable(std::size_t index) noexcept;

#define ENTITY_REF_TABLE(prefix, type)                                                                       \
   template <>                                                                                               \
   inline const type **EntityRefTable<type>(std::size_t index) noexcept                                      \
   {                                                                                                         \
      static const type *table[][4] = {{prefix##"&amp;", prefix##"&#38;", prefix##"&#x26;", prefix##"&"},    \
                                       {prefix##"&lt;", prefix##"&#60;", prefix##"&#x3C;", prefix##"<"},     \
                                       {prefix##"&gt;", prefix##"&#62;", prefix##"&#x3E;", prefix##">"},     \
                                       {prefix##"&quot;", prefix##"&#34;", prefix##"&#x22;", prefix##"\""},  \
                                       {prefix##"&apos;", prefix##"&#39;", prefix##"&#x27;", prefix##"\'"}}; \
      return table[index];                                                                                   \
   }

ENTITY_REF_TABLE(, char)
ENTITY_REF_TABLE(L, wchar_t)
ENTITY_REF_TABLE(u, char16_t)
ENTITY_REF_TABLE(U, char32_t)

#undef ENTITY_REF_TABLE

// Keys of attributes in xml declaration, in different encodings
template <typename TChar>
std::basic_string<TChar> *GetDeclarationAttrs() noexcept;

#define GET_DECLARATION_ATTRS(prefix, type)                                                                        \
   template <>                                                                                                     \
   inline std::basic_string<type> *GetDeclarationAttrs<type>() noexcept                                            \
   {                                                                                                               \
      static std::basic_string<type> decl_attrs[] = {prefix##"version", prefix##"encoding", prefix##"standalone"}; \
      return decl_attrs;                                                                                           \
   }

GET_DECLARATION_ATTRS(, char)
GET_DECLARATION_ATTRS(L, wchar_t)
GET_DECLARATION_ATTRS(u, char16_t)
GET_DECLARATION_ATTRS(U, char32_t)

#undef GET_DECLARATION_ATTRS


// Creates a list of pointers: each one pointing either to a '<' or behind a '>'.
// Text between each pair of successive pointers is a token.
template <typename TChar>
std::list<const TChar *> Tokenize(const TChar *text)
{
   std::list<const TChar *> tokens;
   tokens.emplace_back(text);
   const TChar *pit;
   for (pit = text + 1; *pit; ++pit) {
      if (*pit == '<')
         tokens.emplace_back(pit);
      else if (*(pit - 1) == '>')
         tokens.emplace_back(pit);
   }
   if (*tokens.back()) {
      // add null-terminator
      tokens.emplace_back(pit);
   }
   return tokens;
}

// Removes pointers surrounding regions of whitespaces, so that no tokens consist entirely of whitespaces.
template <typename TChar>
void RemoveGaps(std::list<const TChar *> *tokens)
{
   auto it_right = tokens->begin();
   auto it_left  = it_right++;

   while (it_right != tokens->end()) {
      const TChar *pit  = *it_left;
      const TChar *pend = *it_right;
      bool wspace_only  = true;
      for (; pit < pend; ++pit) {
         if (!IsSpace(*pit)) {
            wspace_only = false;
            break;
         }
      }
      ++it_right;
      if (wspace_only) {
         it_left = tokens->erase(it_left);
      }
      else {
         ++it_left;
      }
   }
}

// Detect comment start and end. Using strncmp was much slower!
template <typename TChar>
inline bool IsCommentStart(const TChar *pit) noexcept
{
   return pit[0] == (TChar)'<' && pit[1] == (TChar)'!' && pit[2] == (TChar)'-' && pit[3] == (TChar)'-';
}
template <typename TChar>
inline bool IsCommentEnd(const TChar *pit) noexcept
{
   return *pit == (TChar)'>' && *(pit - 1) == (TChar)'-' && *(pit - 2) == (TChar)'-';
}

// Erases pointers pointing to positions inside comments, so that each comment is exactly one token.
template <typename TChar>
void RemoveInsideComments(std::list<const TChar *> *tokens)
{
   auto it_right = tokens->begin();
   auto it_left  = it_right++;

   auto it_erase_from = tokens->end();
   auto it_erase_to   = tokens->end();

   while (it_right != tokens->end()) {

      const TChar *pend = *it_right;
      for (const TChar *pit = *it_left; pit < pend; ++pit) {
         if (it_erase_from == tokens->end() && IsCommentStart(pit)) {
            it_erase_from = it_right; // erase next token
            pit += 3;                 // IsCommentStart() checks next 3 symbols
         }
         else if (it_erase_from != tokens->end() && IsCommentEnd(pit)) {
            it_erase_to = it_right; // erase untill (but not inclusive) next token
            break;
         }
      }
      if (it_erase_from != tokens->end() && it_erase_to != tokens->end()) {
         tokens->erase(it_erase_from, it_erase_to); // no-op if it_erase_from==it_erase_to i.e. if there are no tokens inside comment
         it_right = it_erase_to;
         it_left  = it_right++;

         it_erase_from = tokens->end();
         it_erase_to   = tokens->end();
      }
      else {
         ++it_right;
         ++it_left;
      }
   }
}

enum Token
{
   OPEN    = 0x01, // opening xml tag
   CLOSE   = 0x02, // closing xml tag
   CONTENT = 0x04, // free text between opening and closing tags
   COMMENT = 0x08, // everything inside <!--  -->
   ERROR   = 0x00
};

// Determines type of the token whose first symbol is *pbegin and last symbol is *(pend-1).
template <typename TChar>
int DetermineToken(const TChar *pbegin, const TChar *pend) noexcept
{
   if (*pbegin == (TChar)'<') {

      if (*(pbegin + 1) == (TChar)'/') {
         return Token::CLOSE;
      }
      if (IsCommentStart(pbegin)) {
         return Token::COMMENT;
      }

      // roll forward till we reach closing symbol '>'
      while (pbegin < pend && *pbegin != (TChar)'>') {
         ++pbegin;
      }
      if (*pbegin != (TChar)'>') {
         return Token::ERROR;
      }

      int what = Token::OPEN;
      if (*(pbegin - 1) == (TChar)'/')
         what |= Token::CLOSE;
      return what;
   }
   return Token::CONTENT;
}

// Reads element name from the opening tag starting at pbegin (it must point to a '<').
template <typename TChar>
std::basic_string<TChar> ExtractName(const TChar *pbegin)
{
   ++pbegin;
   const TChar *pend = pbegin;
   while (!IsSpace(*pend) && *pend != (TChar)'>' && *pend != (TChar)'/') {
      ++pend;
   }
   return std::basic_string<TChar>(pbegin, pend);
}

// Reads attribute pairs from the tag starting at pbegin (it must point to a '<').
template <typename TChar>
std::unordered_map<std::basic_string<TChar>, std::basic_string<TChar>> ExtractAttributes(const TChar *pbegin)
{
   const TChar *pit = pbegin;
   // skip element name
   while (!IsSpace(*pit) && *pit != (TChar)'>') {
      ++pit;
   }
   std::unordered_map<std::basic_string<TChar>, std::basic_string<TChar>> attrs;

   const TChar *keybegin, *keyend, *valbegin, *valend;
   keybegin = keyend = valbegin = valend = nullptr;

   for (; *pit != (TChar)'>'; ++pit) {
      if (IsAlpha(*pit) && keybegin == nullptr) {
         keybegin = pit;
         continue;
      }
      if (*pit == (TChar)'=') {
         keyend   = pit++;     // now *keyend=='=' and *pit=='\"'
         valbegin = (pit + 1); // now valbegin is behind the left '\"', pit will be there at the end of the iteration
         continue;
      }
      if ((*pit == (TChar)'\"' || *pit == (TChar)'\'') && valbegin != nullptr) {
         valend = pit;
         // extract key and value:
         std::basic_string<TChar> key(keybegin, keyend);
         std::basic_string<TChar> value(valbegin, valend);
         attrs.emplace(std::move(key), std::move(value));
         // reset pointers
         keybegin = keyend = valbegin = valend = nullptr;
      }
   }
   return attrs;
}

// Checks whether from points to start of an entity reference. If so, returns a pointer to the substitution string
// and writes the length of the entity reference to count. Returns nullptr if no entity reference at from.
template <typename TChar>
const TChar *CheckEntityRef(const TChar *from, std::size_t *count, std::size_t er_index)
{
   const TChar **table_line = EntityRefTable<TChar>(er_index);
   for (int col = 0; col < 3; ++col) {
      // Compare with all 3 representations
      const TChar *word = table_line[col];
      const TChar *pit  = from;

      bool equal = true;
      while (*word && *pit) {
         if (*word++ != *pit++) {
            equal = false;
            break;
         }
      }
      if (equal) {
         *count = pit - from;
         return table_line[3];
      }
   }
   return nullptr;
}

// Replaces all entity references in 'content' by the corresponding ascii symbols.
template <typename TChar>
void SubstituteEntityRef(std::basic_string<TChar> &content)
{
   if (content.empty()) {
      return;
   }
   size_t count = 0;
   for (const TChar *from = &content.front(); from < &content.back(); ++from) {
      // Compare 'from' with all 5 entity references
      for (int er_index = 0; er_index < 5; ++er_index) {
         const TChar *repl_str = CheckEntityRef(from, &count, er_index);
         if (repl_str) {
            std::size_t pos = from - &content.front();
            content.replace(pos, count, repl_str); // no better overload :(
            from = &content.front();
         }
      }
   }
}

// Node in the resulting tree. Contains all data about one xml element and pointers to its children.
template <typename TChar>
struct ElementData
{
   typedef TChar char_t;

   std::vector<std::unique_ptr<ElementData>> children;
   std::unordered_map<std::basic_string<char_t>, std::basic_string<char_t>> attrs;
   std::basic_string<char_t> name;
   std::basic_string<char_t> content;
};

// Builds the element tree and returns pointer to its root. Declaration token must be removed from
// 'tokens' prior to calling this function. Ignores the rest after the root element has been closed.
template <typename TChar>
std::unique_ptr<ElementData<TChar>> BuildElementTree(const std::list<const TChar *> &tokens, bool replace_er)
{
   // Create stack and iterators
   std::stack<ElementData<TChar> *, std::list<ElementData<TChar> *>> tree;

   auto it_right = tokens.begin();
   auto it_left  = it_right++;

   // Set up root and push on stack
   auto root   = std::make_unique<ElementData<TChar>>();
   root->name  = ExtractName(*it_left);
   root->attrs = ExtractAttributes(*it_left);
   tree.push(root.get());

   ++it_right;
   ++it_left;

   // Last pointer in 'tokens' points to \0 so checking it_right instead of it_left
   for (; it_right != tokens.cend() && tree.size() > 0; ++it_right, ++it_left) {
      const TChar *pbegin = *it_left;
      const TChar *pend   = *it_right;

      int what = DetermineToken(pbegin, pend);

      if (what & Token::OPEN) {
         // Create and anchor a new element
         ElementData<TChar> *pelem = new ElementData<TChar>();
         tree.top()->children.emplace_back(pelem); // creates std::unique_ptr implicitly

         pelem->name  = ExtractName(pbegin);
         pelem->attrs = ExtractAttributes(pbegin);

         tree.push(pelem);
      }
      if (what & Token::CLOSE) {
         tree.pop();
         continue;
      }
      if (what == Token::CONTENT) {
         tree.top()->content.append(pbegin, pend);
         if (replace_er) {
            SubstituteEntityRef(tree.top()->content);
         }
         continue;
      }
      if (what == Token::ERROR) {
         return nullptr;
      }
      // if (what == Token::COMMENT) continue;
   }
   return root;
}

} // namespace details


class Exception : std::logic_error
{
public:
   Exception(const char *what) : std::logic_error(what)
   {}
   Exception(const std::string &what) : std::logic_error(what)
   {}
   virtual const char *what() const noexcept
   {
      return std::logic_error::what();
   }
};

// Wrapper containing pointer to a node in the element tree, and defining user interface functions
// to access the parsed data. Move is a shallow copy, deep copy unavailable.
template <typename TChar>
class Element
{
public:
   typedef TChar char_t;
   typedef Element<char_t> my_t;

   Element(details::ElementData<char_t> *pdata) : pdata_(pdata)
   {
      if (!pdata_) {
         throw Exception("Failed to create element");
      }
   }
   Element(my_t &&) = default;
   my_t &operator=(my_t &&) = default;

   Element(const my_t &) = delete;
   my_t &operator=(const my_t &) = delete;

   const std::basic_string<char_t> &GetName() const noexcept
   {
      return pdata_->name;
   }
   // Namespace name or empty.
   std::basic_string<char_t> GetNamePrefix() const
   {
      size_t pos = pdata_->name.find_first_of((char_t)':');
      if (pos != pdata_->name.npos) {
         return pdata_->name.substr(0, pos);
      }
      return "";
   }
   // Returns copy of the whole name if no namespace prefix.
   std::basic_string<char_t> GetNamePostfix() const
   {
      size_t pos = pdata_->name.find_first_of((char_t)':');
      if (pos != pdata_->name.npos) {
         return pdata_->name.substr(pos + 1);
      }
      return pdata_->name;
   }
   const std::basic_string<char_t> &GetContent() const noexcept
   {
      return pdata_->content;
   }
   const std::basic_string<char_t> &GetAttributeValue(const std::basic_string<char_t> &attribute) const
   {
      auto it = pdata_->attrs.find(attribute);
      if (it == pdata_->attrs.cend()) {
         throw Exception("Attribute " + attribute + " not found");
      }
      return it->second;
   }
   const std::basic_string<char_t> &GetAttributeName(std::size_t index) const
   {
      return GetAttr(index).first;
   }
   const std::basic_string<char_t> &GetAttributeValue(std::size_t index) const
   {
      return GetAttr(index).second;
   }
   std::size_t GetAttributeCount() const noexcept
   {
      return pdata_->attrs.size();
   }
   std::size_t GetChildCount() const noexcept
   {
      return pdata_->children.size();
   }
   // Creates wrapper for a child element and returns it.
   Element<char_t> GetChild(std::size_t index) const
   {
      if (index >= pdata_->children.size()) {
         throw Exception("Child " + std::to_string(index) + " not found, child count = " + std::to_string(pdata_->children.size()));
      }
      details::ElementData<char_t> *pnode = pdata_->children[index].get();
      return pnode;
   }
   Element<char_t> GetChild(const std::basic_string<char_t> &name) const
   {
      for (auto it = pdata_->children.cbegin(); it != pdata_->children.cend(); ++it) {
         details::ElementData<char_t> *pnode = it->get();
         if (pnode->name == name) {
            return pnode;
         }
      }
      throw Exception("Child " + name + " not found");
   }

private:
   const std::pair<const std::basic_string<char_t>, std::basic_string<char_t>> &GetAttr(std::size_t index) const
   {
      auto it = pdata_->attrs.cbegin();
      for (std::size_t i = 0; i < index; ++i) {
         if (it == pdata_->attrs.cend()) {
            throw Exception("Attribute " + std::to_string(index) + " not found");
         }
         ++it;
      }
      return *it;
   }
   details::ElementData<char_t> *pdata_;
};

// Represents the whole xml document with (or without) declaration and one element tree.
template <typename TChar>
class Document
{
public:
   typedef TChar char_t;
   typedef Document my_t;

   Document(const char_t *text, bool replace_er)
   {
      std::list<const char_t *> tokens = details::Tokenize(text);
      details::RemoveGaps(&tokens);
      details::RemoveInsideComments(&tokens);

      auto it_right = tokens.begin();
      auto it_left  = it_right++;


      const char_t *pfirst = *it_left;
      if (*pfirst != (char_t)'<') {
         throw Exception("Malformed beginning");
      }
      if (*(pfirst + 1) == (char_t)'?') { // has declaration
         auto declaration = details::ExtractAttributes(*it_left);

         std::basic_string<char_t> *decl_attrs  = details::GetDeclarationAttrs<char_t>();
         std::basic_string<char_t> *decl_data[] = {&version_, &encoding_, &standalone_};

         for (int i = 0; i < 3; ++i) {
            auto it = declaration.find(decl_attrs[i]);
            if (it != declaration.cend()) {
               *(decl_data[i]) = it->second;
            }
         }
         tokens.pop_front();
      }
      proot_ = details::BuildElementTree(tokens, replace_er);
      if (!proot_) {
         throw Exception("Malformed xml");
      }
   }
   Document(my_t &&) = default;
   my_t &operator=(my_t &&) = default;

   Document(const my_t &) = delete;
   my_t &operator=(const my_t &) = delete;

   const std::basic_string<char_t> &GetVersion() const noexcept
   {
      return version_;
   }
   const std::basic_string<char_t> &GetEncoding() const noexcept
   {
      return encoding_;
   }
   const std::basic_string<char_t> &GetStandalone() const noexcept
   {
      return standalone_;
   }
   Element<char_t> GetRoot() const noexcept
   {
      return proot_.get();
   }

private:
   std::unique_ptr<details::ElementData<char_t>> proot_;
   std::basic_string<char_t> version_;
   std::basic_string<char_t> encoding_;
   std::basic_string<char_t> standalone_;
};

// Creates xml::Document that reads and parses 'text'. Parsing entity references might slow down the process,
// set entity_references to 'false' if that is undesirable.
template <typename TChar>
inline std::unique_ptr<Document<TChar>> ParseString(const TChar *text, bool entity_references = true)
{
   return std::make_unique<Document<TChar>>(text, entity_references);
}

// Creates xml::Document that reads and parses 'text'. Parsing entity references might slow down the process,
// set entity_references to 'false' if that is undesirable.
template <typename TChar>
inline std::unique_ptr<Document<TChar>> ParseString(const std::basic_string<TChar> &text, bool entity_references = true)
{
   return std::make_unique<Document<TChar>>(text.c_str(), entity_references);
}

// Reads data from 'stream' into cache and parses it into an xml::Document. Lower performance than the other
// overloads. Parsing entity references might slow down the process, set entity_references to 'false' if that is undesirable.
template <typename TChar>
std::unique_ptr<Document<TChar>> ParseStream(std::basic_istream<TChar> &stream, bool entity_references = true)
{
   std::basic_string<TChar> s;
   char buf[4096];

   while (stream.read(buf, sizeof(buf)))
      s.append(buf, sizeof(buf));
   s.append(buf, stream.gcount());

   return std::make_unique<Document<TChar>>(s.c_str(), entity_references);
}

} // namespace xml

#endif // XMLPARSER_HPP