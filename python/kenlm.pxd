cdef extern from "lm/word_index.hh":
    ctypedef unsigned WordIndex

cdef extern from "lm/model.hh" namespace "lm":
    cdef cppclass FullScoreReturn:
        FullScoreReturn(FullScoreReturn)
        float prob
        unsigned char ngram_length

cdef extern from "lm/model.hh" namespace "lm::ngram":
    cdef cppclass State:
        State()
        State(State& state)

    cdef cppclass Vocabulary:
        WordIndex Index(char*)
        WordIndex BeginSentence() 
        WordIndex EndSentence()
        WordIndex NotFound()

    cdef cppclass SortedVocabulary:
        WordIndex Index(char*)
        WordIndex BeginSentence() 
        WordIndex EndSentence()
        WordIndex NotFound()

    ctypedef Vocabulary const_Vocabulary "const lm::ngram::Vocabulary"
    ctypedef SortedVocabulary const_SortedVocabulary "const lm::ngram::SortedVocabulary"

    cdef cppclass Model:
        Model(char*) except +
        State &BeginSentenceState()
        State &NullContextState()
        unsigned int Order()
        const_Vocabulary& GetVocabulary()
        float Score(State &in_state, WordIndex new_word, State &out_state)
        FullScoreReturn FullScore(State &in_state, WordIndex new_word, State &out_state)

    cdef cppclass QuantArrayTrieModel:
        QuantArrayTrieModel(char*) except +
        State &BeginSentenceState()
        State &NullContextState()
        unsigned int Order()
        const_SortedVocabulary& GetVocabulary()
        float Score(State &in_state, WordIndex new_word, State &out_state)
        FullScoreReturn FullScore(State &in_state, WordIndex new_word, State &out_state)
