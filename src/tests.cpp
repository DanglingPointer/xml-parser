#include <string>
#include <iostream>
#include "xmlparser.hpp"

int main()
{
   const char * l = "Hello";
   std::string s(l, l + 5);

   std::cout << s << std::endl;



   return 0;
}