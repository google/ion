
NULL =

layoutincludedir = $(pkgincludedir)/layout
layoutinclude_HEADERS = \
	LayoutEngine.h \
	LEFontInstance.h \
	LEGlyphFilter.h \
	LEGlyphStorage.h \
	LEInsertionList.h \
	LELanguages.h \
	LEScripts.h \
	LESwaps.h \
	LETypes.h \
	loengine.h \
	$(NULL)

lib_LTLIBRARIES = libicu-le-hb.la

libicu_le_hb_la_LDFLAGS =
libicu_le_hb_la_CPPFLAGS = \
	$(HARFBUZZ_CFLAGS) \
	$(ICU_CFLAGS) \
	$(NULL)
libicu_le_hb_la_LIBADD = \
	$(HARFBUZZ_LIBS) \
	$(NULL)

libicu_le_hb_la_SOURCES = \
	$(include_HEADERS) \
	LayoutEngine.cpp \
	LEFontInstance.cpp \
	LEGlyphStorage.cpp \
	LEInsertionList.cpp \
	loengine.cpp \
	ScriptAndLanguageTags.cpp \
	ScriptAndLanguageTags.h \
	$(NULL)

-include $(top_srcdir)/git.mk
