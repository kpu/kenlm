cdef extern from "lm/word_index.hh":
    ctypedef unsigned WordIndex

cdef extern from "lm/model.hh" namespace "lm":
    cdef cppclass FullScoreReturn:
        FullScoreReturn(FullScoreReturn)
        float prob
        unsigned char ngram_length

cdef extern from "lm/model.hh" namespace "lm::ngram":
    cdef cppclass Vocabulary "const lm::base::Vocabulary":
        WordIndex Index(char*)
        WordIndex BeginSentence() 
        WordIndex EndSentence()
        WordIndex NotFound()

    cdef cppclass State:
        State()
        State(State& state)

    cdef cppclass Model:
        Model(char*) except +
        State &BeginSentenceState()
        State &NullContextState()
        unsigned int Order()
        Vocabulary& BaseVocabulary()
        float Score(State &in_state, WordIndex new_word, State &out_state)
        FullScoreReturn FullScore(State &in_state, WordIndex new_word, State &out_state)
