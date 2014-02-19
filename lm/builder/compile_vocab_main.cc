#include "util/fake_ofstream.hh"
#include "util/file_piece.hh"
#include "util/murmur_hash.hh"
#include "util/pool.hh"
#include "util/probing_hash_table.hh"

struct Entry {
  typedef uint64_t Key;
  uint64_t key;
  uint64_t GetKey() const { return key; }
  void SetKey(uint64_t to) { key = to; }
};

struct Remember {
  bool operator<(const Remember &other) const {
    return hash < other.hash;
  }
  uint64_t hash;
  const char *str;
};

void Add(util::AutoProbing<Entry, util::IdentityHash> &dedupe, util::Pool &string_pool, std::vector<Remember> &words, StringPiece str) {
  Entry entry;
  entry.SetKey(util::MurmurHash64A(str.data(), str.size()));
  util::AutoProbing<Entry, util::IdentityHash>::MutableIterator ignored;
  if (!dedupe.FindOrInsert(entry, ignored)) {
    char *addr = static_cast<char*>(memcpy(string_pool.Allocate(str.size() + 1), str.data(), str.size()));
    addr[str.size()] = '\0';
    words.resize(words.size() + 1);
    words.back().str = addr;
    words.back().hash = entry.GetKey();
  }
}

int main(int argc, char *argv[]) {
  bool is_null[256];
  memset(is_null, 0, sizeof(is_null));
  is_null[0] = true;
  util::Pool string_pool;
  std::vector<Remember> words;
  std::cerr << "Deduping" << std::endl;
  {
    util::AutoProbing<Entry, util::IdentityHash> dedupe;
    Add(dedupe, string_pool, words, "<s>");
    Add(dedupe, string_pool, words, "</s>");
    for (int i = 1; i < argc; ++i) {
      util::FilePiece f(argv[i]);
      try { while (true) {
        Add(dedupe, string_pool, words, f.ReadDelimited(is_null));
      } } catch (const util::EndOfFileException &e) {}
    }
  }

  std::cerr << "Sorting" << std::endl;
  std::sort(words.begin(), words.end());

  std::cerr << "Writing text" << std::endl;
  util::FakeOFStream out(1);
  out << "<unk>" << '\0';
  for (std::vector<Remember>::const_iterator i = words.begin(); i != words.end(); ++i) {
    // Include null termination.
    out << StringPiece(i->str, strlen(i->str) + 1);
  }
}
