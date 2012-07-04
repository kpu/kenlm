cdef bytes as_str(data):
    if isinstance(data, bytes):
        return data
    elif isinstance(data, unicode):
        return data.encode('utf8')
    raise TypeError('Cannot convert %s to string' % type(data))

cdef class LanguageModel:
    cdef Model* model
    cdef Vocabulary* vocab

    def __cinit__(self, path):
        cdef bytes path_str = as_str(path)
        try:
            self.model = new Model(path_str)
        except RuntimeError as exception:
            raise IOError('Cannot read model \'%s\'' % path) from exception
        self.vocab = &self.model.BaseVocabulary()

    def __dealloc__(self):
        del self.model

    property order:
        def __get__(self):
            return self.model.Order()
    
    def score(self, sentence):
        cdef list words = as_str(sentence).split()
        cdef State* state = new State(self.model.BeginSentenceState())
        cdef State* out_state = new State()
        cdef float total = 0
        for word in words:
            total += self.model.Score(state[0], self.vocab.Index(word), out_state[0])
            state[0] = out_state[0]
        total += self.model.Score(state[0], self.vocab.EndSentence(), out_state[0])
        del state, out_state
        return total

    def full_scores(self, sentence):
        cdef list words = as_str(sentence).split()
        cdef State* state = new State(self.model.BeginSentenceState())
        cdef State* out_state = new State()
        cdef FullScoreReturn* ret
        cdef float total = 0
        try:
            for word in words:
                ret = new FullScoreReturn(self.model.FullScore(state[0],
                    self.vocab.Index(word), out_state[0]))
                try:
                    yield (ret.prob, ret.ngram_length)
                finally:
                    del ret
                state[0] = out_state[0]
            ret = new FullScoreReturn(self.model.FullScore(state[0], 
                self.vocab.EndSentence(), out_state[0]))
            try:
                yield (ret.prob, ret.ngram_length)
            finally:
                del ret
        finally:
            del state, out_state
    
    def __contains__(self, word):
        cdef bytes w = as_str(word)
        return (self.vocab.Index(w) != 0)
