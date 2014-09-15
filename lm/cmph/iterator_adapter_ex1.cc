#include "cmph_builder.h"
// Create minimal perfect hash function from in-memory vector

#pragma pack(1)
typedef struct {
    cmph_uint32 id;
    char key[11];
    cmph_uint32 year;
} rec_t;
#pragma pack(0)

class InputIterator {
public:
    InputIterator(rec_t* init)
        : iter_(init)
        , begin_(init)
    {}

    InputIterator& operator++() {
        ++iter_;
        return *this;
    }

    void* operator*() {
        return iter_->key;
    }

    void Rewind() {
        iter_ = begin_;
    }

private:
    rec_t* iter_;
    rec_t* begin_;
};

int main() {
    const int KEY_COUNT = 10;
    const int KEY_SIZE = 11;
    rec_t vector[KEY_COUNT] = { 
            { 1, "aaaaaaaaaa", 1999 }, { 2, "bbbbbbbbbb", 2000 },
            { 3, "cccccccccc", 2001 }, { 4, "dddddddddd", 2002 },
            { 5, "eeeeeeeeee", 2003 }, { 6, "ffffffffff", 2004 },
            { 7, "gggggggggg", 2005 }, { 8, "hhhhhhhhhh", 2006 },
            { 9, "iiiiiiiiii", 2007 }, { 10, "jjjjjjjjjj", 2008 } };

    CMPHBuilder builder;
    builder.BuildHash(InputIterator(vector), KEY_SIZE, KEY_COUNT);
    builder.Pack(0, 0);
    builder.Load(0, 0);

    for (size_t i = 0; i < KEY_COUNT; ++i) {
        const char *key = vector[i].key;
        uint32_t index = builder.Find(key, KEY_SIZE);
        std::cout << "key: [" << key << "] -- hash: [" << index << "]" << std::endl;
    }
    return 0;
}
