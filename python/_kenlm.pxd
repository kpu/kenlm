from libcpp.string cimport string
from libcpp.vector cimport vector
from libc.stdint cimport uint64_t
from libcpp cimport bool

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
        Config() except +
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

    int OpenReadOrThrow(char*) except +
    int CreateOrThrow(char*) except +
    string DefaultTempDirectory() except +
    void NormalizeTempPrefix(string) except +

cdef extern from "util/usage.hh" namespace "util":
    uint64_t GuessPhysicalMemory() except +
    uint64_t ParseSize(string) except +

cdef extern from "util/stream/config.hh" namespace "util::stream":
    cdef struct SortConfig:
        string temp_prefix
        size_t buffer_size
        size_t total_memory

    cdef struct ChainConfig:
        size_t entry_size
        size_t block_count
        size_t total_memory

cdef extern from "lm/builder/initial_probabilities.hh" namespace "lm::builder":
    cdef struct InitialProbabilitiesConfig:
        ChainConfig adder_in
        ChainConfig adder_out
        bool interpolate_unigrams

cdef extern from "lm/lm_exception.hh" namespace "lm":
    ctypedef enum WarningAction:
        THROW_UP
        COMPLAIN
        SILENT

cdef extern from "lm/builder/discount.hh" namespace "lm::builder":
    cdef struct Discount:
        float amount[4]

cdef extern from "lm/builder/adjust_counts.hh" namespace "lm::builder":
    cdef struct DiscountConfig:
        vector[Discount] overwrite
        Discount fallback
        WarningAction bad_action

cdef extern from "lm/builder/output.hh" namespace "lm::builder":

    cdef cppclass OutputHook:
        pass

    cdef cppclass Output:
        Output(char*, bool, bool) except +
        void Add(OutputHook*) except +

    cdef cppclass PrintHook(OutputHook):
        PrintHook(int, bool) except +

cdef extern from "lm/builder/pipeline.hh" namespace "lm::builder":
    struct PipelineConfig:
        size_t order
        SortConfig sort
        InitialProbabilitiesConfig initial_probs
        ChainConfig read_backoffs
        WordIndex vocab_estimate
        size_t minimum_block
        size_t block_count
        vector[uint64_t] prune_thresholds
        bool prune_vocab
        string prune_vocab_file
        bool renumber_vocabulary
        DiscountConfig discount
        bool output_q
        uint64_t vocab_size_for_unk
        WarningAction disallowed_symbol_action

    void Pipeline(PipelineConfig, int, Output) except +
