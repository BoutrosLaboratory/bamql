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
	$(LLVM_CPPFLAGS) \
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
	predicates.cpp \
	$(NULL)

runtime.bc: runtime.c
	clang -c -emit-llvm -o $@ $<

runtime.inc: runtime.bc
	echo -e "#include <llvm/IR/IRBuilder.h>\n#include <llvm/IR/Module.h>\nusing namespace llvm;\nnamespace barf {" > $@
	llc -march=cpp -cppgen=functions -o - $< | sed 's/ExternalLinkage/InternalLinkage/g' >> $@
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
