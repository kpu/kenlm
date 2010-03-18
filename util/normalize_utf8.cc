#include "util/utf8.hh"

#include <string>
#include <iostream>

int main() {
  std::string line, normalized;
  while (getline(std::cin, line)) {
    utf8::Normalize(line, normalized);
    std::cout << line << '\n';
  }
}
