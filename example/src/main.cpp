#include <iostream>
import Print;

int main(int, const char**) {
  makeDotCpp::Context ctx;
  
  print();
  std::cout << "\033[1;31mbold red text\033[0m\n"<< std::endl;
  return 0;
}
