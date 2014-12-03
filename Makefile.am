NULL = 
ACLOCAL_AMFLAGS = -I m4
bin_PROGRAMS = barf barf-compile
lib_LTLIBRARIES = libbarf.la
library_includedir=$(includedir)/barf
library_include_HEADERS = barf.hpp
#pkgconfigdir = $(libdir)/pkgconfig
#pkgconfig_DATA = barf.pc
#man1_MANS = barf.1 barf-compile.1

libbarf_la_CPPFLAGS = \
	-std=c++11 \
	$(LLVM_CPPFLAGS) -fexceptions \
	$(NULL)
libbarf_la_LIBS = \
	$(LLVM_LIBS) \
	$(NULL)
libbarf_la_LDFLAGS = \
	$(LLVM_LDFLAGS) \
	-version-info 0:0:0 \
	-no-undefined \
	$(NULL)
libbarf_la_SOURCES = \
	ast_node_logical.cpp \
	parser_misc.cpp \
	predicates.cpp \
	$(NULL)

barf_SOURCES = \
	main-run.cpp \
	$(NULL)
barf_CPPFLAGS = \
	-std=c++11 \
	$(HTS_CFLAGS) \
	$(LLVM_CPPFLAGS) -fexceptions \
	$(NULL)
barf_LDFLAGS =\
	$(LLVM_LDFLAGS) \
	$(NULL)
barf_LDADD = \
	libbarf.la \
	$(HTS_LIBS) \
	$(LLVM_LIBS) \
	$(NULL)

barf_compile_SOURCES = \
	main-compile.cpp \
	$(NULL)
barf_compile_CPPFLAGS = \
	-std=c++11 \
	$(LLVM_CPPFLAGS) -fexceptions \
	$(NULL)
barf_compile_LDFLAGS =\
	$(LLVM_LDFLAGS) \
	$(NULL)
barf_compile_LDADD = \
	libbarf.la \
	$(LLVM_LIBS) \
	$(NULL)

runtime.bc: runtime.c
	clang -c -emit-llvm -o $@ $<

runtime.inc: runtime.bc
	echo -e "#include <llvm/IR/IRBuilder.h>\n#include <llvm/IR/Module.h>\nusing namespace llvm;\nnamespace barf {" > $@
	for each in $$(llvm-nm -g -P $< | cut -f 1 -d " "); do llc -march=cpp -cppfname=define_$${each} --cppfor=$${each} -cppgen=function -o - $< | sed 's/ExternalLinkage/InternalLinkage/g' >> $@; done
	echo "}" >> $@

predicates.cpp: runtime.inc

EXTRA_DIST = \
	barf.hpp \
	runtime.c \
	$(NULL)

CLEANFILES = \
	runtime.bc \
	runtime.inc \
	$(NULL)
