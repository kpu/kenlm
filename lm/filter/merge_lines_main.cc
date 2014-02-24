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
  boost::ptr_vector<util::FilePiece> files;
  for (int i = 1; i < argc; ++i) {
    files.push_back(new util::FilePiece(argv[i]));
  }
  if (files.empty()) {
    std::cerr << "Provide a list of vocabulary files to merge on the command line." << std::endl;
    return 1;
  }
  util::FakeOFStream out(1);
  StringPiece word;
  try { while (true) {
    std::cerr << "New dedupe" << std::endl;
    util::AutoProbing<Entry, util::IdentityHash> dedupe;
    for (boost::ptr_vector<util::FilePiece>::iterator i = files.begin(); i != files.end(); ++i) {
      while (i->ReadWordSameLine(word)) {
        std::cerr << "Read " << word << std::endl;
        Entry entry;
        entry.key = util::MurmurHashNative(word.data(), word.size());
        util::AutoProbing<Entry, util::IdentityHash>::MutableIterator ignored;
        if (!dedupe.FindOrInsert(entry, ignored)) {
          std::cerr << "Identified word " << word << std::endl;
          out << word << ' ';
        }
      }
      i->ReadLine();
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
