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
    
    def score(self, sentence, bos = True, eos = True):
        cdef list words = as_str(sentence).split()
        cdef State state
        if bos:
            self.model.BeginSentenceWrite(&state)
        else:
            self.model.NullContextWrite(&state)
        cdef State out_state
        cdef float total = 0
        for word in words:
            total += self.model.BaseScore(&state, self.vocab.Index(word), &out_state)
            state = out_state
        if eos:
            total += self.model.BaseScore(&state, self.vocab.EndSentence(), &out_state)
        return total
    
    def full_scores(self, sentence, bos = True, eos = True):
        """
        full_scores(sentence, bos = True, eos = Ture) -> generate full scores (prob, ngram length, oov)
        @param sentence is a string (do not use boundary symbols)
        @param bos should kenlm add a bos state
        @param eos should kenlm add an eos state
        """
        cdef list words = as_str(sentence).split()
        cdef State state
        if bos:
            self.model.BeginSentenceWrite(&state)
        else:
            self.model.NullContextWrite(&state)
        cdef State out_state
        cdef FullScoreReturn ret
        cdef float total = 0
        cdef WordIndex wid
        for word in words:
            wid = self.vocab.Index(word)
            ret = self.model.BaseFullScore(&state, wid, &out_state)
            yield (ret.prob, ret.ngram_length, wid == 0)
            state = out_state
        if eos:
            ret = self.model.BaseFullScore(&state,
                self.vocab.EndSentence(), &out_state)
            yield (ret.prob, ret.ngram_length, False)
    
    def __contains__(self, word):
        cdef bytes w = as_str(word)
        return (self.vocab.Index(w) != 0)

    def __repr__(self):
        return '<LanguageModel from {0}>'.format(os.path.basename(self.path))

    def __reduce__(self):
        return (LanguageModel, (self.path,))
