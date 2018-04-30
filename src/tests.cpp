#include <fstream>
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
      <topping id="5002"/>
      <topping id="5003" />
      <topping id="5004">Su&#39;gar</topping>
      <topping id="5005">&quot;Sprinkles&#x22;</topping>
      <topping id="5006">Chocolate</topping>
      <!--<topping></topping> -->
      <!-- blablabal-->
      <nm:topping nm:id="5007">Maple&amp;Apple</topping>
   </item>
   <item id="0000" type="empty" />
</items>
)";

template <typename TChar>
std::basic_ostream<TChar> &operator<<(std::basic_ostream<TChar> &out, const xml::Element<TChar> &e)
{
   out << "\nName: " << e.GetName() << "\nChildCount: " << e.GetChildCount() << "\nContent: " << e.GetContent() << "\nAttributes: ";
   for (int i = 0; i < e.GetAttributeCount(); ++i) {
      out << e.GetAttributeName(i) << ":" << e.GetAttributeValue(i) << " ";
   }
   out << "\nName prefix: " << e.GetNamePrefix() << "\nName postfix: " << e.GetNamePostfix() << "\n{\n";
   for (int i = 0; i < e.GetChildCount(); ++i) {
      out << e.GetChild(i) << std::endl;
   }
   return out << "} // " << e.GetName() << "\n";
}

template <typename TChar>
std::basic_ostream<TChar> &operator<<(std::basic_ostream<TChar> &out, const xml::Document<TChar> &doc)
{
   out << "Encoding = " << doc.GetEncoding() << ", Version = " << doc.GetVersion() << ", Standalone = " << doc.GetStandalone() << std::endl;
   return out << doc.GetRoot() << std::endl;
}

int main(int argc, char *argv[])
{
   std::unique_ptr<xml::Document<char>> doc;
   try {
      auto start = std::chrono::system_clock::now();

      if (argc > 1) {
         std::ifstream file(argv[1]);
         doc = xml::ParseStream(file);
      }
      else {
         doc = xml::ParseString(text);
      }
      std::cout << *doc;

      auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start);
      std::cout << "\nTime: " << elapsed.count() << "us\n";
   }
   catch (const xml::Exception &e) {
      std::cout << e.what() << std::endl;
   }


   return 0;
}