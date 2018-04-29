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
void RemoveWhitespaces(std::list<const TChar *> *tokens)
{
   auto it_right = tokens->begin();
   auto it_left = it_right++;

   while (it_right != tokens->end()) {
      const TChar *pit = *it_left;
      const TChar *pend = *it_right;
      bool whitespaces = true;
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

enum class Tag
{
   OPEN,
   CLOSE,
   CONTENT,
   ERROR
};

// pbegin and pend must be from the list tokenized list
template <typename TChar>
Tag DetermineTag(const TChar *pbegin, const TChar *pend)
{
   if (*pbegin == (TChar)'<') {
      if (*(pend - 1) != (TChar)'>') return Tag::ERROR;

      if (*(pend - 1) == (TChar)'/') {
         return Tag::CLOSE;
      }
      else {
         return Tag::OPEN;
      }
   }
   return Tag::CONTENT;
}

// pbegin must point to '<'
template <typename TChar>
std::basic_string<TChar> ExtractName(const TChar *pbegin)
{
   const TChar *pend = pbegin;
   while (IsAlpha(*pend)) {
      ++pend;
   }
   return std::basic_string<TChar>(pbegin + 1, pend);
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

   for (; pit < ptagend; ++pit) {
      if (IsAlpha(*pit) && keybegin == nullptr) {
         keybegin = pit;
         continue;
      }
      if (*pit == (TChar)'=') {
         keyend = pit++;   // now *keyend=='=' and *pit=='\"'
         valbegin = ++pit; // now pit and valbegin are past '\"'
         continue;
      }
      if (*pit == (TChar)'\"' && valbegin != nullptr) {
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
struct ElementData
{
public:
   typedef TChar char_t;

   std::vector<std::unique_ptr<ElementData>> children;
   std::unordered_map<std::basic_string<char_t>, std::basic_string<char_t>> attrs;
   std::basic_string<char_t> name;
   std::basic_string<char_t> content;
};

// 'tokens' must NOT include declaration element
template <typename TChar>
std::unique_ptr<ElementData<TChar>> BuildElementTree(const std::list<const TChar *> &tokens)
{
   // Create stack and iterators
   std::stack<ElementData<TChar> *, std::list<ElementData<TChar> *>> tree;

   auto it_right = tokens.begin();
   auto it_left = it_right++;

   // Set up root and push on stack
   auto root = std::make_unique<ElementData<TChar>>();
   root->name = ExtractName(*it_left);
   root->attrs = ExtractAttributes(*it_left, *it_right);
   tree.push(root.get());

   ++it_right;
   ++it_left;

   // Last pointer in 'tokens' points to \0
   while (it_right != tokens.cend() && tree.size() > 0) {
      const TChar *pbegin = *it_left;
      const TChar *pend = *it_right;

      Tag what = DetermineTag(pbegin, pend);
      switch (what) {
         case Tag::CLOSE: {
            // Might be either a true closing tag or a new "inlined" element
            auto name = ExtractName(pbegin);
            if (name == tree.top()->name) {
               tree.pop();
               break;
            } // else goto Tag::OPEN
         }
         case Tag::OPEN: {
            // Create and anchor a new element
            ElementData<TChar> *pelem = new ElementData<TChar>();
            tree.top()->children.emplace_back(pelem); // creates std::unique_ptr implicitly

            pelem->name = ExtractName(pbegin);
            pelem->attrs = ExtractAttributes(pbegin, pend);

            tree.push(pelem);
            break;
         }
         case Tag::CONTENT: {
            // Add content to the uppermost (current) element in the tree
            std::basic_string<TChar> content(pbegin, pend);
            tree.top()->content = std::move(content);
            break;
         }
         case Tag::ERROR: {
            return nullptr;
         }
      }
      ++it_right;
      ++it_left;
   }
   return root;
}

template <typename TChar>
std::basic_string<TChar> *GetDeclarationAttrs() noexcept
{
   static std::basic_string<TChar> decl_attrs[] = {L"version", L"encoding", L"standalone"};
   return decl_attrs;
}
template <>
std::basic_string<char> *GetDeclarationAttrs<char>() noexcept
{
   static std::basic_string<char> decl_attrs[] = {"version", "encoding", "standalone"};
   return decl_attrs;
}


} // namespace details


class Exception : std::logic_error
{
public:
   Exception(const std::string &what) : std::logic_error(what)
   {}
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
   const std::basic_string<char_t> &GetAttributeValue(std::size_t index) const
   {
      auto it = pdata_->attrs.cbegin();
      for (std::size_t i = 0; i < index; ++i) {
         if (it == pdata_->attrs.cend()) {
            throw Exception("Attribute not found");
         }
         ++it;
      }
      return it->second;
   }
   std::size_t GetAttributeCount() const noexcept
   {
      return pdata_->attrs.size();
   }
   std::size_t GetChildrenCount() const noexcept
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
   details::ElementData<char_t> *pdata_;
};

template <typename TChar>
class Document
{
public:
   typedef TChar char_t;
   typedef Document my_t;

   explicit Document(const char_t *text)
   {
      std::list<const char_t *> tokens = details::Tokenize(text);
      details::RemoveWhitespaces(&tokens);

      auto it_right = tokens.begin();
      auto it_left = it_right++;


      const char_t *pfirst = *it_left;
      if (*pfirst != (char_t)'<') {
         throw Exception("Malformed xml");
      }
      if (*(pfirst + 1) == (char_t)'?') { // has declaration
         auto declaration = details::ExtractAttributes(*it_left, *it_right);

         std::basic_string<char_t> *decl_attrs = details::GetDeclarationAttrs<char_t>();
         std::basic_string<char_t> *decl_data[] = {&version_, &encoding_, &standalone_};

         for (int i = 0; i < 3; ++i) {
            auto it = declaration.find(decl_attrs[i]);
            if (it != declaration.cend()) {
               *(decl_data[i]) = it->second;
            }
         }
         tokens.pop_front();
      }
      proot_ = details::BuildElementTree(tokens);
      if (!proot_) {
         throw Exception("Parsing failed");
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
Document<TChar> ParseString(const TChar *text)
{
   return Document<TChar>(text);
}

using TChar = char;

// template <typename TChar>
Document<TChar> ParseString(const std::basic_string<TChar> &text)
{
   return Document<TChar>(text.c_str());
}

template <typename TChar>
Document<TChar> ParseStream(std::basic_istream<TChar> &stream)
{
   std::basic_string<TChar> s;
   char buf[4096];
   while (stream.read(buf, sizeof(buf))) {
      s.append(buf, sizeof(buf));
   }
   s.append(buf, stream.gcount());
   return Document<TChar>(s.c_str());
}

} // namespace xml