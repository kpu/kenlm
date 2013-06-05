import os

cdef bytes as_str(data):
    if isinstance(data, bytes):
        return data
    elif isinstance(data, unicode):
        return data.encode('utf8')
    raise TypeError('Cannot convert %s to string' % type(data))

cdef class LanguageModel:
    cdef Model* model
    cdef public bytes path
    cdef const_Vocabulary* vocab

    def __init__(self, path):
        self.path = os.path.abspath(as_str(path))
        try:
            self.model = LoadVirtual(self.path)
        except RuntimeError as exception:
            exception_message = str(exception).replace('\n', ' ')
            raise IOError('Cannot read model \'{}\' ({})'.format(path, exception_message))\
                    from exception
        self.vocab = &self.model.BaseVocabulary()

    def __dealloc__(self):
        del self.model

    property order:
        def __get__(self):
            return self.model.Order()
    
    def score(self, sentence):
        cdef list words = as_str(sentence).split()
        cdef State state
        self.model.BeginSentenceWrite(&state)
        cdef State out_state
        cdef float total = 0
        for word in words:
            total += self.model.Score(&state, self.vocab.Index(word), &out_state)
            state = out_state
        total += self.model.Score(&state, self.vocab.EndSentence(), &out_state)
        return total

    def full_scores(self, sentence):
        cdef list words = as_str(sentence).split()
        cdef State state
        self.model.BeginSentenceWrite(&state)
        cdef State out_state
        cdef FullScoreReturn ret
        cdef float total = 0
        for word in words:
            ret = self.model.FullScore(&state,
                self.vocab.Index(word), &out_state)
            yield (ret.prob, ret.ngram_length)
            state = out_state
        ret = self.model.FullScore(&state,
            self.vocab.EndSentence(), &out_state)
        yield (ret.prob, ret.ngram_length)
    
    def __contains__(self, word):
        cdef bytes w = as_str(word)
        return (self.vocab.Index(w) != 0)

    def __repr__(self):
        return '<LanguageModel from {0}>'.format(os.path.basename(self.path))

    def __reduce__(self):
        return (LanguageModel, (self.path,))
