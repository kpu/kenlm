#pragma once
extern "C" {
#include <cmph.h>
}
#include <memory>
#include <memory.h>

#include "iterator_adapter.h"

class CMPHBuilder {
public:
    CMPHBuilder();
    ~CMPHBuilder();

public:
    template <class InputIterator>
    void BuildHash(InputIterator begin, size_t key_size, size_t key_count) {
        std::auto_ptr<cmph_io_adapter_t> source(
            cmph_io_iterator_adapter(
                begin,
                static_cast<cmph_uint32>(key_size),
                static_cast<cmph_uint32>(key_count)));
        std::auto_ptr<cmph_config_t> config(cmph_config_new(source.get()));

        cmph_config_set_algo(config.get(), CMPH_CHD);
        hash_.reset(cmph_new(config.get()));

        cmph_config_destroy(config.release());
        cmph_io_iterator_adapter_destroy<InputIterator>(source.release());
    }

    void Pack(int fd, uint64_t offset);
    void Load(int fd, uint64_t offset);
    uint32_t Find(const char *key, size_t key_size);

private:
    void CHDPack(cmph_t *mphf, int fd, uint64_t offset);
    
    template <class T>
    void Write(int fd, const T &value, uint64_t offset) {
        Write(fd, &value, sizeof(value), offset);
    }
    
    void Write(int fd, const void *data, size_t size, uint64_t offset);

private:
    std::auto_ptr<cmph_t> hash_;
    void *packed_hash_;

    // TODO: fix me
    char MY_TMP_BUF[1000000];
};