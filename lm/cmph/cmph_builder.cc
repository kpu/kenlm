#include "cmph_builder.h"
extern "C" {
#include <chd.h>
#include <chd_structs.h>
#include <cmph_structs.h>
}

CMPHBuilder::CMPHBuilder()
    : hash_(NULL)
    , packed_hash_(NULL)
{
    for (size_t i = 0; i < 1000000; ++i) {
        MY_TMP_BUF[i] = 0;
    }
}

CMPHBuilder::~CMPHBuilder() {
    if (hash_.get()) {
        cmph_destroy(hash_.release());
    }
}

void CMPHBuilder::Pack(int fd, uint64_t offset) {
    CHDPack(hash_.get(), fd, offset);
}

void CMPHBuilder::Load(int fd, uint64_t offset) {
    // TODO: fix me
    packed_hash_ = MY_TMP_BUF + offset;
}

uint32_t CMPHBuilder::Find(const char *key, size_t key_size) {
    return chd_search_packed(packed_hash_, key, static_cast<cmph_uint32>(key_size));
}

void CMPHBuilder::CHDPack(cmph_t *mphf, int fd, uint64_t offset) {
    chd_data_t *data = (chd_data_t *)mphf->data;

    Write(fd, data->packed_cr_size, offset);
    offset += sizeof(cmph_uint32);

    Write(fd, data->packed_cr, data->packed_cr_size, offset);
    offset += data->packed_cr_size;

    Write(fd, data->packed_chd_phf_size, offset);
    offset += sizeof(cmph_uint32);

    Write(fd, data->packed_chd_phf, data->packed_chd_phf_size, offset);
}

void CMPHBuilder::Write(int fd, const void *data, size_t size, uint64_t offset) {
    // TODO: use util/file.hh : ErsatzPWrite
    memcpy(MY_TMP_BUF + offset, data, size);
}
