#include "util/file_piece.hh"

#include <iostream>

#include <ctype.h>

int main() {
  util::FilePiece f(0, "stdin");
  size_t highest = 0;
  try {
    while (true) {
      size_t count = 0;
      StringPiece l = f.ReadLine();
      for (const char *i = l.data(); i != l.data() + l.size(); ++i) {
        if (isspace(*i)) ++count;
      }
      highest = std::max(highest, count);
    }
  } catch (util::EndOfFileException &e) {}
  std::cout << highest << std::endl;
}
