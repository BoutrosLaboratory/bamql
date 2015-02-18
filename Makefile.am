NULL = 
ACLOCAL_AMFLAGS = -I m4
bin_PROGRAMS = barf barf-chain barf-compile
lib_LTLIBRARIES = libbarf.la
library_includedir=$(includedir)/barf
library_include_HEADERS = barf.hpp barf-runtime.hpp
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = barf.pc
man1_MANS = barf.1 barf-chain.1 barf-compile.1
man7_MANS = barf_queries.7

libbarf_la_CPPFLAGS = \
	-std=c++11 \
	$(LLVM_CORE_CPPFLAGS) -fexceptions \
	$(HTS_CFLAGS) -O2 \
	$(NULL)
libbarf_la_LIBADD = \
	$(LLVM_CORE_LIBS) \
	$(NULL)
libbarf_la_LDFLAGS = \
	$(LLVM_CORE_LDFLAGS) \
	-version-info 0:0:0 \
	-no-undefined \
	$(NULL)
libbarf_la_SOURCES = \
	ast_node_logical.cpp \
	misc.cpp \
	parser_misc.cpp \
	predicates.cpp \
	parser.cpp \
	runtime.cpp \
	$(NULL)

barf_SOURCES = \
	main-run.cpp \
	$(NULL)
barf_CPPFLAGS = \
	-std=c++11 \
	$(HTS_CFLAGS) -O2 \
	$(LLVM_RUN_CPPFLAGS) -fexceptions \
	$(NULL)
barf_LDFLAGS =\
	$(LLVM_RUN_LDFLAGS) \
	$(NULL)
barf_LDADD = \
	libbarf.la \
	$(HTS_LIBS) \
	$(LLVM_RUN_LIBS) \
	$(NULL)

barf_chain_SOURCES = \
	main-chain.cpp \
	$(NULL)
barf_chain_CPPFLAGS = \
	-std=c++11 \
	$(HTS_CFLAGS) -O2 \
	$(LLVM_RUN_CPPFLAGS) -fexceptions \
	$(NULL)
barf_chain_LDFLAGS =\
	$(LLVM_RUN_LDFLAGS) \
	$(NULL)
barf_chain_LDADD = \
	libbarf.la \
	$(HTS_LIBS) \
	$(LLVM_RUN_LIBS) \
	$(NULL)

barf_compile_SOURCES = \
	main-compile.cpp \
	$(NULL)
barf_compile_CPPFLAGS = \
	-std=c++11 \
	$(LLVM_WRITE_CPPFLAGS) -fexceptions \
	$(NULL)
barf_compile_LDFLAGS =\
	$(LLVM_WRITE_LDFLAGS) \
	$(NULL)
barf_compile_LDADD = \
	libbarf.la \
	$(LLVM_WRITE_LIBS) \
	$(NULL)

runtime.bc: runtime.c
	## For reasons that are totally incomprehensible, LLC will die if -g is
	## provided, so we strip it out.
	$(CLANG) -c -emit-llvm -o $@ $$(echo $(HTS_CFLAGS) | sed 's/-g//g') -O2 $<

runtime.cpp: runtime.bc
	echo -e "#include <llvm/IR/IRBuilder.h>\n#include <llvm/IR/Module.h>\nusing namespace llvm;\nnamespace barf {" > $@
	$(LLC) -march=cpp -cppfname=define_runtime --cppfor=__dummy__ -cppgen=contents -o - $< | sed '/setModuleIdentifier/d' >> $@
	echo "}" >> $@

EXTRA_DIST = \
	barf.hpp \
	runtime.c \
	$(NULL)

CLEANFILES = \
	runtime.bc \
	runtime.cpp \
	$(NULL)
