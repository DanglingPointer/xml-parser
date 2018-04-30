#include <utility>
#include <stack>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <istream>
#include <cctype>
#include <list>

namespace xml {
namespace details {

template <typename TChar>
inline bool IsSpace(TChar symbol)
{
   return std::iswspace(symbol);
}
template <>
inline bool IsSpace<char>(char symbol)
{
   return std::isspace(static_cast<unsigned char>(symbol));
}

template <typename TChar>
inline bool IsAlpha(TChar symbol)
{
   return std::iswalpha(symbol);
}
template <>
inline bool IsAlpha<char>(char symbol)
{
   return std::isalpha(static_cast<unsigned char>(symbol));
}

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
   return std::move(tokens);
}

template <typename TChar>
void RemoveGaps(std::list<const TChar *> *tokens)
{
   auto it_right = tokens->begin();
   auto it_left  = it_right++;

   while (it_right != tokens->end()) {
      const TChar *pit  = *it_left;
      const TChar *pend = *it_right;
      bool whitespaces  = true;
      for (; pit < pend; ++pit) {
         if (!IsSpace(*pit)) {
            whitespaces = false;
            break;
         }
      }
      ++it_right;
      if (whitespaces) {
         it_left = tokens->erase(it_left);
      }
      else {
         ++it_left;
      }
   }
}

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

template <typename TChar>
void RemoveComments(std::list<const TChar *> *tokens)
{
   auto it_right = tokens->begin();
   auto it_left  = it_right++;

   while (it_right != tokens->end()) {
      const TChar *pit  = *it_left;
      const TChar *pend = *it_right;

      if (IsCommentStart(pit)) {
         bool comment_end = false;
         do {
            // Finding whether comment end lies between pit and pend
            // Always decrement at least once since no tokens point to '>' as required by IsCommentEnd()
            --pend;
         } while (pend > (pit + 1) && !(comment_end = IsCommentEnd(pend)));

         if (!comment_end) {
            // it_right moves forward, it_left stays
            ++it_right;
         }
         else {
            tokens->erase(it_left, it_right);
            it_left = it_right++;
         }
      }
      else {
         ++it_right;
         ++it_left;
      }
   }
}

enum Tag
{
   OPEN    = 0x01,
   CLOSE   = 0x02,
   CONTENT = 0x04,
   ERROR   = 0x00
};

// pbegin and pend must be from the list tokenized list
template <typename TChar>
int DetermineTag(const TChar *pbegin, const TChar *pend)
{
   if (*pbegin == (TChar)'<') {
      if (*(pbegin + 1) == (TChar)'/') {
         return Tag::CLOSE;
      }

      bool inside_comment = false;
      while (pend > pbegin && (*pend != (TChar)'>' || inside_comment)) {
         --pend;
         if (IsCommentEnd(pend))
            inside_comment = true;
         else if (IsCommentStart(pend))
            inside_comment = false;
      }
      if (*pend != (TChar)'>') {
         return Tag::ERROR;
      }

      int what = Tag::OPEN;
      if (*(pend - 1) == (TChar)'/')
         what |= Tag::CLOSE;
      return what;
   }
   return Tag::CONTENT;
}

// pbegin must point to '<'
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

// ptagbegin must point to '<', ptagend must point past '>'
template <typename TChar>
std::unordered_map<std::basic_string<TChar>, std::basic_string<TChar>> ExtractAttributes(const TChar *ptagbegin, const TChar *ptagend)
{
   const TChar *pit = ptagbegin;
   // skip element name
   while (!IsSpace(*pit)) {
      ++pit;
   }
   std::unordered_map<std::basic_string<TChar>, std::basic_string<TChar>> attrs;

   const TChar *keybegin, *keyend, *valbegin, *valend;
   keybegin = keyend = valbegin = valend = nullptr;

   for (; pit < ptagend && *pit != (TChar)'>'; ++pit) {
      if (IsAlpha(*pit) && keybegin == nullptr) {
         keybegin = pit;
         continue;
      }
      if (*pit == (TChar)'=') {
         keyend   = pit++; // now *keyend=='=' and *pit=='\"'
         valbegin = ++pit; // now pit and valbegin are past '\"'
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

template <typename TChar>
const TChar **EntityRefTable(std::size_t index) noexcept;

#define ENTITY_REF_TABLE(prefix, type)                                                                       \
   template <>                                                                                               \
   inline const type **EntityRefTable<type>(std::size_t index) noexcept                                      \
   {                                                                                                         \
      static const type *table[][4] = {{prefix##"&amp;", prefix##"&#38", prefix##"&#x26;", prefix##"&"},     \
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


// returns pointer to substitution string, writes length of entity reference to count
template <typename TChar>
const TChar *IsEntityRef(const TChar *from, std::size_t *count, std::size_t er_index)
{
   const TChar **table_line = EntityRefTable<TChar>(er_index);
   for (int i = 0; i < 3; ++i) {
      const TChar *word = table_line[i];
      const TChar *pit  = from;
      bool equal        = true;
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

template <typename TChar>
void SubstituteEntityRef(std::basic_string<TChar> &content)
{
   if (content.empty())
      return;

   const TChar *from = &content.front();
   size_t count      = 0;

   for (const TChar *from = &content.front(); from < &content.back(); ++from) {
      for (int er_index = 0; er_index < 5; ++er_index) {
         const TChar *replacement = IsEntityRef(from, &count, er_index);
         if (replacement) {
            std::size_t pos = from - &content.front();
            content.replace(pos, count, replacement);
            from = &content.front();
         }
      }
   }
}

template <typename TChar>
struct ElementData
{
   typedef TChar char_t;

   std::vector<std::unique_ptr<ElementData>> children;
   std::unordered_map<std::basic_string<char_t>, std::basic_string<char_t>> attrs;
   std::basic_string<char_t> name;
   std::basic_string<char_t> content;
};

// 'tokens' must NOT include declaration element
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
   root->attrs = ExtractAttributes(*it_left, *it_right);
   tree.push(root.get());

   ++it_right;
   ++it_left;

   // Last pointer in 'tokens' points to \0
   for (; it_right != tokens.cend() && tree.size() > 0; ++it_right, ++it_left) {
      const TChar *pbegin = *it_left;
      const TChar *pend   = *it_right;

      int what = DetermineTag(pbegin, pend);

      if (what & Tag::OPEN) {
         // Create and anchor a new element
         ElementData<TChar> *pelem = new ElementData<TChar>();
         tree.top()->children.emplace_back(pelem); // creates std::unique_ptr implicitly

         pelem->name  = ExtractName(pbegin);
         pelem->attrs = ExtractAttributes(pbegin, pend);

         tree.push(pelem);
      }
      if (what & Tag::CLOSE) {
         tree.pop();
         continue;
      }
      if (what == Tag::CONTENT) {
         std::basic_string<TChar> content(pbegin, pend);
         tree.top()->content = std::move(content);
         if (replace_er) {
            SubstituteEntityRef(tree.top()->content);
         }
         continue;
      }
      if (what == Tag::ERROR) {
         return nullptr;
      }
   }
   return root;
}

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


} // namespace details


class Exception : std::logic_error
{
public:
   Exception(const std::string &what) : std::logic_error(what)
   {}
   virtual const char *what() const noexcept
   {
      return std::logic_error::what();
   }
};

// wrapper for ElementData
template <typename TChar>
class Element
{
public:
   typedef TChar char_t;

   Element(details::ElementData<char_t> *pdata) : pdata_(pdata)
   {
      if (!pdata_) {
         throw Exception("Failded to create element");
      }
   }
   const std::basic_string<char_t> &GetName() const noexcept
   {
      return pdata_->name;
   }
   std::basic_string<char_t> GetNamePrefix() const
   {
      size_t pos = pdata_->name.find_first_of((char_t)':');
      if (pos != pdata_->name.npos) {
         return pdata_->name.substr(0, pos);
      }
      return "";
   }
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
         throw Exception("Attribute not found");
      }
      return it->second;
   }
   const std::basic_string<char_t> &GetAttributeName(std::size_t index) const
   {
      return GetAttribute(index)->first;
   }
   const std::basic_string<char_t> &GetAttributeValue(std::size_t index) const
   {
      return GetAttribute(index)->second;
   }
   std::size_t GetAttributeCount() const noexcept
   {
      return pdata_->attrs.size();
   }
   std::size_t GetChildCount() const noexcept
   {
      return pdata_->children.size();
   }
   Element<char_t> GetChild(std::size_t index) const
   {
      if (index >= pdata_->children.size()) {
         throw Exception("Child not found");
      }
      details::ElementData<char_t> *pnode = pdata_->children[index].get();
      return pnode;
   }

private:
   const std::pair<const std::basic_string<char_t>, std::basic_string<char_t>> *GetAttribute(std::size_t index) const
   {
      auto it = pdata_->attrs.cbegin();
      for (std::size_t i = 0; i < index; ++i) {
         if (it == pdata_->attrs.cend()) {
            throw Exception("Attribute not found");
         }
         ++it;
      }
      return &(*it);
   }
   details::ElementData<char_t> *pdata_;
};

template <typename TChar>
class Document
{
public:
   typedef TChar char_t;
   typedef Document my_t;

   explicit Document(const char_t *text, bool replace_er)
   {
      std::list<const char_t *> tokens = details::Tokenize(text);
      details::RemoveGaps(&tokens);
      details::RemoveComments(&tokens);

      auto it_right = tokens.begin();
      auto it_left  = it_right++;


      const char_t *pfirst = *it_left;
      if (*pfirst != (char_t)'<') {
         throw Exception("Malformed beginning");
      }
      if (*(pfirst + 1) == (char_t)'?') { // has declaration
         auto declaration = details::ExtractAttributes(*it_left, *it_right);

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


template <typename TChar>
inline std::unique_ptr<Document<TChar>> ParseString(const TChar *text, bool entity_references = true)
{
   return std::make_unique<Document<TChar>>(text, entity_references);
}

template <typename TChar>
inline std::unique_ptr<Document<TChar>> ParseString(const std::basic_string<TChar> &text, bool entity_references = true)
{
   return std::make_unique<Document<TChar>>(text.c_str(), entity_references);
}

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