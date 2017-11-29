cdef extern from "lm/word_index.hh" namespace "lm":
    ctypedef unsigned WordIndex

cdef extern from "lm/return.hh" namespace "lm":
    cdef struct FullScoreReturn:
        float prob
        unsigned char ngram_length

cdef extern from "lm/state.hh" namespace "lm::ngram":
    cdef cppclass State :
        int Compare(const State &other) const

    int hash_value(const State &state) 

cdef extern from "lm/virtual_interface.hh" namespace "lm::base":
    cdef cppclass Vocabulary:
        WordIndex Index(char*)
        WordIndex BeginSentence() 
        WordIndex EndSentence()
        WordIndex NotFound()

    ctypedef Vocabulary const_Vocabulary "const lm::base::Vocabulary"

    cdef cppclass Model:
        void BeginSentenceWrite(void *)
        void NullContextWrite(void *)
        unsigned int Order()
        const_Vocabulary& BaseVocabulary()
        float BaseScore(void *in_state, WordIndex new_word, void *out_state)
        FullScoreReturn BaseFullScore(void *in_state, WordIndex new_word, void *out_state)

cdef extern from "util/mmap.hh" namespace "util":
    cdef enum LoadMethod:
        LAZY
        POPULATE_OR_LAZY
        POPULATE_OR_READ
        READ
        PARALLEL_READ

cdef extern from "lm/config.hh" namespace "lm::ngram":
    cdef cppclass Config:
        Config()
        float probing_multiplier
        LoadMethod load_method

cdef extern from "lm/model.hh" namespace "lm::ngram":
    cdef Model *LoadVirtual(char *, Config &config) except +
    #default constructor
    cdef Model *LoadVirtual(char *) except +

cdef extern from "util/file.hh" namespace "util":
    cdef cppclass scoped_fd:
        scoped_fd() except +
        scoped_fd(int) except +
        void reset(int) except +
        int release() except +

    int OpenReadOrThrow(const char *) except +
    int CreateOrThrow(const char *) except +

# cdef extern from "lm/builder/pipeline.hh" namespace "lm::builder":
#     cdef cppclass PipelineConfig:


cdef extern from "lm/builder/output.hh" namespace "lm::builder":
    cdef cppclass Output:
        Output(char*, bool, bool) except +

    cdef cppclass PrintHook:
        PrintHook(int, bool) except +
