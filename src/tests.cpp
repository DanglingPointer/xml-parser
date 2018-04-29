#include <string>
#include <iostream>
#include <chrono>
#include "xmlparser.hpp"

const char *text =
    R"(<?xml version="1.0" encoding="UTF-8"?>
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
      <topping id="5002">Glazed</topping>
      <topping id="5005">Su&#39;gar</topping>
      <topping id="5006">&quot;Sprinkles&#x22;</topping>
      <topping id="5003">Chocolate</topping>
      <nm:topping nm:id="5004">Maple&amp;Apple</topping>
   </item>
   <item id="0000" type="empty" />
</items>
)";

// using TChar = char;
template <typename TChar>
std::ostream &operator<<(std::ostream &out, const xml::Element<TChar> &e)
{
   out << "Name: " << e.GetName() << ", Content: " << e.GetContent() << ", Attributes: ";
   for (int i = 0; i < e.GetAttributeCount(); ++i) {
      out << e.GetAttributeName(i) << ":" << e.GetAttributeValue(i) << " ";
   }
   out << "\n{\n";
   for (int i = 0; i < e.GetChildrenCount(); ++i) {
      out << e.GetChild(i) << std::endl;
   }
   return out << "}\n";
}

template <typename TChar>
std::ostream &operator<<(std::ostream &out, const xml::Document<TChar> &doc)
{
   out << "Encoding = " << doc.GetEncoding() << ", Version = " << doc.GetVersion() << ", Standalone = " << doc.GetStandalone() << std::endl;
   return out << doc.GetRoot() << std::endl;
}

int main()
{
   try {
      auto start = std::chrono::system_clock::now();

      xml::Document<char> doc(text, true);
      std::cout << doc;

      auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start);
      std::cout << "Time: " << elapsed.count() << std::endl;
   }
   catch (const xml::Exception &e) {
      std::cout << e.what() << std::endl;
   }


   return 0;
}