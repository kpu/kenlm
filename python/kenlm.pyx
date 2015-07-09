import os
cimport _kenlm

cdef bytes as_str(data):
    if isinstance(data, bytes):
        return data
    elif isinstance(data, unicode):
        return data.encode('utf8')
    raise TypeError('Cannot convert %s to string' % type(data))

cdef class FullScoreReturn:
    """
    Wrapper around FullScoreReturn.

    Notes:
        `prob` has been renamed to `log_prob`
        `oov` has been added to flag whether the word is OOV
    """

    cdef float log_prob
    cdef int ngram_length
    cdef bint oov

    def __cinit__(self, log_prob, ngram_length, oov):
        self.log_prob = log_prob
        self.ngram_length = ngram_length
        self.oov = oov

    def __repr__(self):
        return '{0}({1}, {2}, {3})'.format(self.__class__.__name__, repr(self.log_prob), repr(self.ngram_length), repr(self.oov))
    
    property log_prob:
        def __get__(self):
            return self.log_prob

    property ngram_length:
        def __get__(self):
            return self.ngram_length

    property oov:
        def __get__(self):
            return self.oov

cdef class State:
    """
    Wrapper around lm::ngram::State so that python code can make incremental queries.

    Notes:
        * rich comparisons 
        * hashable
    """

    cdef _kenlm.State _c_state

    def __richcmp__(State qa, State qb, int op):
        r = qa._c_state.Compare(qb._c_state)
        if op == 0:    # <
            return r < 0
        elif op == 1:  # <=
            return r <= 0
        elif op == 2:  # ==
            return r == 0
        elif op == 3:  # !=
            return r != 0
        elif op == 4:  # >
            return r > 0
        else:          # >=
            return r >= 0

    def __hash__(self):
        return _kenlm.hash_value(self._c_state)


cdef class LanguageModel:
    """
    This is not a strict wrapper, the interface is more pythonic.
    It loads models and queries full sentences.
    """

    cdef _kenlm.Model* model
    cdef public bytes path
    cdef _kenlm.const_Vocabulary* vocab

    def __init__(self, path):
        """
        Load the language model.

        :param path: path to an arpa file or a kenlm binary file.
        """
        self.path = os.path.abspath(as_str(path))
        try:
            self.model = _kenlm.LoadVirtual(self.path)
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
        cdef _kenlm.State state
        if bos:
            self.model.BeginSentenceWrite(&state)
        else:
            self.model.NullContextWrite(&state)
        cdef _kenlm.State out_state
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
        cdef _kenlm.State state
        if bos:
            self.model.BeginSentenceWrite(&state)
        else:
            self.model.NullContextWrite(&state)
        cdef _kenlm.State out_state
        cdef _kenlm.FullScoreReturn ret
        cdef float total = 0
        cdef _kenlm.WordIndex wid
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
        return (_kenlm.LanguageModel, (self.path,))

cdef class Model:
    """
    This is closer to a wrapper around lm::ngram::Model.
    """

    cdef _kenlm.Model* model
    cdef public bytes path
    cdef _kenlm.const_Vocabulary* vocab

    def __init__(self, path):
        """
        Load the language model.

        :param path: path to an arpa file or a kenlm binary file.
        """
        self.path = os.path.abspath(as_str(path))
        try:
            self.model = _kenlm.LoadVirtual(self.path)
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

    def BeginSentenceWrite(self, State state):
        """Change the given state to a BOS state."""
        self.model.BeginSentenceWrite(&state._c_state)

    def NullContextWrite(self, State state):
        """Change the given state to a NULL state."""
        self.model.NullContextWrite(&state._c_state)

    def BaseScore(self, State in_state, str word, State out_state):
        """
        Return p(word|in_state) and update the output state.
        Wrapper around model.BaseScore(in_state, Index(word), out_state)

        :param word: the suffix
        :param state: the context (defaults to NullContext)
        :returns: p(word|state)
        """
        cdef float total = self.model.BaseScore(&in_state._c_state, self.vocab.Index(as_str(word)), &out_state._c_state)
        return total
    
    def BaseFullScore(self, State in_state, str word, State out_state):
        """
        Wrapper around model.BaseScore(in_state, Index(word), out_state)

        :param word: the suffix
        :param state: the context (defaults to NullContext)
        :returns: FullScoreReturn(word|state)
        """
        cdef _kenlm.WordIndex wid = self.vocab.Index(as_str(word))
        cdef _kenlm.FullScoreReturn ret = self.model.BaseFullScore(&in_state._c_state, wid, &out_state._c_state)
        return FullScoreReturn(ret.prob, ret.ngram_length, wid == 0)
    
    def __contains__(self, word):
        cdef bytes w = as_str(word)
        return (self.vocab.Index(w) != 0)

    def __repr__(self):
        return '<Model from {0}>'.format(os.path.basename(self.path))

    def __reduce__(self):
        return (_kenlm.LanguageModel, (self.path,))

