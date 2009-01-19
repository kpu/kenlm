# the most important programs
bin_PROGRAMS = TranslateMEMT Server

TranslateMEMT_SOURCES = TranslateMEMT.cc

Server_SOURCES = Server.cc

SUBDIRS = \
        Client \
	Share \
	LM \
	MEMT

TranslateMEMT_LDADD = \
	@top_builddir@/src/MEMT/Input/libInput.a \
	@top_builddir@/src/Share/libShare.a \
	@top_builddir@/src/MEMT/Input/Phrase/libPhrase.a \
	@top_builddir@/src/MEMT/Decoder/libDecoder.a \
	@top_builddir@/src/MEMT/Output/libOutput.a \
	@top_builddir@/src/MEMT/OldPhrase/libPhrase.a \
	@top_builddir@/src/LM/libLm.a \
	@top_builddir@/src/LM/SALM/libSA.a

Server_LDADD = \
	@top_builddir@/src/MEMT/Input/libInput.a \
	@top_builddir@/src/Share/libShare.a \
	@top_builddir@/src/MEMT/Input/Phrase/libPhrase.a \
	@top_builddir@/src/MEMT/Decoder/libDecoder.a \
	@top_builddir@/src/MEMT/Output/libOutput.a \
	@top_builddir@/src/LM/libLm.a \
	@top_builddir@/src/LM/SALM/libSA.a
       
AM_CPPFLAGS = \
	-I@top_srcdir@/src \
	-I@top_srcdir@/src/Share \
	-Wall
