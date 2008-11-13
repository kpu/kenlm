# the most important programs
bin_PROGRAMS = TranslateMEMT

TranslateMEMT_SOURCES = TranslateMEMT.cc

SUBDIRS = \
        Client \
	Share \
	Text \
	LM \
	MEMT

SRI_LD = LM/sri/lib/@SRILM_MACHINE_TYPE@/liboolm.a \
	 LM/sri/lib/@SRILM_MACHINE_TYPE@/libflm.a \
	 LM/sri/lib/@SRILM_MACHINE_TYPE@/liblattice.a \
	 LM/sri/lib/@SRILM_MACHINE_TYPE@/libdstruct.a \
	 LM/sri/lib/@SRILM_MACHINE_TYPE@/libmisc.a

TranslateMEMT_LDADD = \
	@top_builddir@/src/Share/libShare.a \
	@top_builddir@/src/Text/libText.a \
	@top_builddir@/src/MEMT/libDecoder.a \
	@top_builddir@/src/MEMT/Phrase/libPhrase.a \
	@top_builddir@/src/LM/libLm.a \
	@top_builddir@/src/LM/SALM/libSA.a \
	$(SRI_LD)

AM_CPPFLAGS = \
	-I. \
	-I@top_srcdir@/src \
	-I@top_srcdir@/src/Share \
	-I@top_srcdir@/src/Text \
	-I@top_srcdir@/src/LM \
	-I@top_srcdir@/src/LM/sri/include \
	-I@top_srcdir@/src/LM/SALM \
	-I@top_srcdir@/src/MEMT
