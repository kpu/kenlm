#include "util/fake_ofstream.hh"
#include "util/file_piece.hh"
#include "util/murmur_hash.hh"
#include "util/probing_hash_table.hh"

#include <boost/ptr_container/ptr_vector.hpp>

struct Entry {
  typedef uint64_t Key;
  uint64_t GetKey() const { return key; }
  void SetKey(uint64_t to) { key = to; }

  uint64_t key;
};

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Provide a list of vocabulary files to merge on the command line." << std::endl;
    return 1;
  }
  boost::ptr_vector<util::FilePiece> files;
  files.push_back(new util::FilePiece(argv[1], &std::cerr));
  for (int i = 2; i < argc; ++i) {
    files.push_back(new util::FilePiece(argv[i]));
  }
  util::FakeOFStream out(1);
  StringPiece word;
  try { while (true) {
    util::AutoProbing<Entry, util::IdentityHash> dedupe;
    for (boost::ptr_vector<util::FilePiece>::iterator i = files.begin(); i != files.end(); ++i) {
      try {
        while (i->ReadWordSameLine(word)) {
          Entry entry;
          entry.key = util::MurmurHashNative(word.data(), word.size());
          util::AutoProbing<Entry, util::IdentityHash>::MutableIterator ignored;
          if (!dedupe.FindOrInsert(entry, ignored)) {
            out << word << ' ';
          }
        }
        i->ReadLine();
      } catch (const util::EndOfFileException &e) {
        if (i == files.begin()) throw;
        std::cerr << "File " << i->FileName() << " is shorter than the others." << std::endl;
        return 1;
      }
    }
    out << '\n';
  } } catch (const util::EndOfFileException &e) {
    // Check they all ended.
    for (boost::ptr_vector<util::FilePiece>::iterator i = files.begin(); i != files.end(); ++i) {
      try {
        i->get();
        std::cerr << "File " << i->FileName() << " is longer than the others." << std::endl;
        return 1;
      } catch (const util::EndOfFileException &e) {}
    }
  }
  return 0;
}
