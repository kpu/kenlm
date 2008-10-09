# the most important programs
bin_PROGRAMS = TranslateMEMT

TranslateMEMT_SOURCES = TranslateMEMT.cc


SUBDIRS = \
	Share \
	Text \
	LM \
	SRI \
	SALM \
	MEMT



TranslateMEMT_LDADD = \
	@top_builddir@/src/Share/*.o \
	@top_builddir@/src/Text/*.o \
	@top_builddir@/src/LM/*.o \
	@top_builddir@/src/SRI/*.o \
	@top_builddir@/src/SALM/*.o \
	@top_builddir@/src/MEMT/Aligner.o \
	@top_builddir@/src/MEMT/DecoderMEMT.o \
	@top_builddir@/src/MEMT/ECParserPhraseDetector.o \
	@top_builddir@/src/MEMT/LMPhraseDetector.o \
	@top_builddir@/src/MEMT/MatcherInterface.o \
	@top_builddir@/src/MEMT/MatcherPhraseDetector.o \
	@top_builddir@/src/MEMT/PhraseDetector.o \
	@top_builddir@/src/MEMT/PunctuationPhraseDetector.o \
	@top_builddir@/src/MEMT/SourceChunkPhraseDetector.o

AM_CPPFLAGS = \
	-I. \
	-I@top_srcdir@/src \
	-I@top_srcdir@/src/Share \
	-I@top_srcdir@/src/Text \
	-I@top_srcdir@/src/LM \
	-I@top_srcdir@/src/SRI \
	-I@top_srcdir@/src/SALM \
	-I@top_srcdir@/src/MEMT

