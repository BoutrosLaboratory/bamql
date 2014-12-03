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
	runtime.cpp \
	compiler.cpp \
	$(NULL)

runtime.bc: runtime.c
	clang -c -emit-llvm -o $@ $<

runtime.cpp: runtime.bc
	echo -e "#include <llvm/IR/IRBuilder.h>\n#include <llvm/IR/Module.h>\nusing namespace llvm;\nnamespace barf {" > $@
	for each in $$(llvm-nm -g -P $< | cut -f 1 -d " "); do llc -march=cpp -cppfname=generate_$${each} --cppfor=$${each} -cppgen=inline -o - $< >> $@; done
	echo "}" >> $@

EXTRA_DIST = \
	barf.hpp \
	runtime.c \
	$(NULL)

CLEANFILES = \
	runtime.bc \
	runtime.cpp \
	$(NULL)
