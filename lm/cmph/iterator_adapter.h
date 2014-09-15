#pragma once
#include <cmph.h>
#include <assert.h>
#include <iostream>

template <class InputIterator>
struct cmph_iterator_data_t
{
    InputIterator iterator;
    cmph_uint32 key_len;
};

template <class InputIterator>
cmph_io_adapter_t *cmph_io_iterator_new(InputIterator iterator, cmph_uint32 key_len, cmph_uint32 nkeys)
{
    cmph_io_adapter_t * key_source = (cmph_io_adapter_t *)malloc(sizeof(cmph_io_adapter_t));
    cmph_iterator_data_t<InputIterator> * cmph_iterator_data =
        (cmph_iterator_data_t<InputIterator> *)malloc(sizeof(cmph_iterator_data_t<InputIterator>));
    assert(key_source);
    assert(cmph_iterator_data);
    cmph_iterator_data->iterator = iterator;
    cmph_iterator_data->key_len = key_len;
    key_source->data = cmph_iterator_data;
    key_source->nkeys = nkeys;
    return key_source;
}

template <class InputIterator>
void cmph_io_iterator_destroy(cmph_io_adapter_t *key_source)
{
    cmph_iterator_data_t<InputIterator> *cmph_iterator_data =
        static_cast<cmph_iterator_data_t<InputIterator> *>(key_source->data);
    free(cmph_iterator_data);
    free(key_source);
}

template <class InputIterator>
static int key_iterator_read(void *data, char **key, cmph_uint32 *keylen)
{
    // TODO: fix me
    static char buffer[1000] = {};
    if (*key == NULL) {
        *key = buffer;
    }
    cmph_iterator_data_t<InputIterator>& iterator_data = *(cmph_iterator_data_t<InputIterator> *)data;
    InputIterator& iterator = iterator_data.iterator;
    *key = static_cast<char*>(*iterator);
    ++iterator;
    *keylen = iterator_data.key_len;
    return (int)(*keylen);
}

template <class InputIterator>
static void key_iterator_rewind(void *data)
{
    InputIterator& iterator = static_cast<cmph_iterator_data_t<InputIterator> *>(data)->iterator;
    iterator.Rewind();
}

static void key_iterator_dispose(void *data, char *key, cmph_uint32 keylen)
{
    // free(key);
}

template <class InputIterator>
cmph_io_adapter_t *cmph_io_iterator_adapter(InputIterator iterator, cmph_uint32 key_len, cmph_uint32 nkeys)
{
    cmph_io_adapter_t * key_source = cmph_io_iterator_new(iterator, key_len, nkeys);
    key_source->read = key_iterator_read<InputIterator>;
    key_source->dispose = key_iterator_dispose;
    key_source->rewind = key_iterator_rewind<InputIterator>;
    return key_source;
}

template <class InputIterator>
void cmph_io_iterator_adapter_destroy(cmph_io_adapter_t *key_source)
{
    cmph_io_iterator_destroy<InputIterator>(key_source);
}
