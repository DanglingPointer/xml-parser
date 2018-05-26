#include "xmlparser.hpp"
#include <fstream>
#include <iostream>
#include <chrono>

#define UNICODE

#ifdef UNICODE

typedef wchar_t char_t;
#define STDOUT std::wcout
#define _T(x) L##x

#else

typedef char char_t;
#define STDOUT std::cout
#define _T(x) x

#endif



const char_t *text = _T(R"(<?xml version="1.0" encoding="UTF-8"?>
   <items>
   <item id="0001" type="donut">
      <name>Cake</name>
      <ppu>0.55</ppu>
      <batters>
         <batter id="1001">Regular</batter>
         <batter id="1002">Chocolate</batter>
         <batter id="1003">Blueberry</batter>
      </batters>
      <topping id="5001">None</topping>
      <topping id="5002"/>
      <topping id="5003" />
      <topping id="5004">Su&#39;gar</topping>
      <topping id="5005">&quot;Sprinkles&#x22;</topping>
      <topping id="5006">Chocolate</topping>
      <!--<topping></topping> -->
      <!-- blablabal-->
      <nm:topping nm:id="5007">Maple&amp;Apple</nm:topping>
   </item>
   <item id="0000" type="empty" />
</items>
)");

void TestParseString(const char_t *text)
{
   auto doc = xml::ParseString(text);
   STDOUT << doc->ToString() << std::endl;

   auto root = doc->GetRoot();
   STDOUT << root.GetName() << std::endl;
}

void TestNewDocument()
{
   auto doc  = xml::NewDocument(_T("root"), _T("1.0"), _T("UTF-8"), _T("yes"));
   auto root = doc->GetRoot();
   root.AddAttribute(_T("attr1"), _T("vaLue1"));
   root.AddAttribute(_T("attr2"), _T("value2"));
   auto child1 = root.AddChild(_T("child"));
   child1.SetContent(_T("Content 3 goes here"));
   auto child2 = root.AddChild(0, _T("child"));
   child2.SetContent(_T("Content 1 goes here"));
   auto child3 = root.AddChild(1, _T("child"));
   child3.SetContent(_T("Content 2 goes here"));
   child3.AddAttribute(_T("last"), _T("False"));
   auto child4 = root.AddChild(_T("last"));
   child4.AddAttribute(_T("last"), _T("True"));
   STDOUT << doc->ToString() << std::endl;

   root.SetContent(_T("illegal content")); // should throw
}

void TestParseFile(char *filename)
{
   std::basic_ifstream<char_t> file(filename);
   auto doc = xml::ParseStream(file, true);
   STDOUT << doc->ToString() << std::endl;
}



int main(int argc, char *argv[])
{
   using namespace std::chrono_literals;

   auto start = std::chrono::system_clock::now();
   try {
      if (argc > 1)
         TestParseFile(argv[1]);

      TestParseString(text);

      TestNewDocument();
   }
   catch (const xml::Exception &e) {
      STDOUT << e.what() << std::endl;
   }

   auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start);
   STDOUT << _T("\nTime: ") << elapsed.count() << _T("ms (including reading file)\n");

   return 0;
}