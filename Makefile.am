NULL = 
ACLOCAL_AMFLAGS = -I m4
bin_PROGRAMS = bamql bamql-chain bamql-compile
lib_LTLIBRARIES = libbamql.la libbamql-jit.la
library_includedir=$(includedir)/bamql
library_include_HEADERS = bamql.hpp bamql-jit.hpp
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = bamql.pc bamql-jit.pc
man1_MANS = bamql.1 bamql-chain.1 bamql-compile.1
man7_MANS = bamql_queries.7

TESTS = check_bamql
check_PROGRAMS = check_bamql

libbamql_la_CPPFLAGS = \
	-std=c++11 \
	$(LLVM_CORE_CPPFLAGS) \
	$(HTS_CFLAGS) \
	$(PCRE_CFLAGS) \
	-g -O2 \
	$(NULL)
libbamql_la_LIBADD = \
	$(LLVM_CORE_LIBS) \
	$(PCRE_LIBS) \
	$(NULL)
libbamql_la_LDFLAGS = \
	$(LLVM_CORE_LDFLAGS) \
	-version-info 0:0:0 \
	-no-undefined \
	$(NULL)
libbamql_la_SOURCES = \
	ast_node_logical.cpp \
	misc.cpp \
	parser_misc.cpp \
	pcre.cpp \
	predicates.cpp \
	parser.cpp \
	runtime.cpp \
	version.cpp \
	$(NULL)

libbamql_jit_la_CPPFLAGS = \
	-std=c++11 \
	$(LLVM_RUN_CPPFLAGS) \
	$(HTS_CFLAGS) -g -O2 \
	$(NULL)
libbamql_jit_la_LIBADD = \
	libbamql.la \
	$(LLVM_RUN_LIBS) \
	$(HTS_LIBS) \
	$(NULL)
libbamql_jit_la_LDFLAGS = \
	$(LLVM_RUN_LDFLAGS) \
	-version-info 0:0:0 \
	-no-undefined \
	$(NULL)
libbamql_jit_la_SOURCES = \
	iterator.cpp \
	jit.cpp \
	$(NULL)

bamql_SOURCES = \
	main-run.cpp \
	$(NULL)
bamql_CPPFLAGS = \
	-std=c++11 \
	$(HTS_CFLAGS) -g -O2 \
	$(LLVM_RUN_CPPFLAGS) \
	$(UUID_CFLAGS) \
	$(NULL)
bamql_LDFLAGS =\
	$(LLVM_RUN_LDFLAGS) \
	$(NULL)
bamql_LDADD = \
	libbamql.la \
	libbamql-jit.la \
	$(HTS_LIBS) \
	$(LLVM_RUN_LIBS) \
	$(UUID_LIBS) \
	$(NULL)

bamql_chain_SOURCES = \
	main-chain.cpp \
	$(NULL)
bamql_chain_CPPFLAGS = \
	-std=c++11 \
	$(HTS_CFLAGS) -g -O2 \
	$(LLVM_RUN_CPPFLAGS) \
	$(UUID_CFLAGS) \
	$(NULL)
bamql_chain_LDFLAGS =\
	$(LLVM_RUN_LDFLAGS) \
	$(NULL)
bamql_chain_LDADD = \
	libbamql.la \
	libbamql-jit.la \
	$(HTS_LIBS) \
	$(LLVM_RUN_LIBS) \
	$(UUID_LIBS) \
	$(NULL)

bamql_compile_SOURCES = \
	main-compile.cpp \
	$(NULL)
bamql_compile_CPPFLAGS = \
	-std=c++11 \
	$(Z_CPPFLAGS) \
	$(LLVM_WRITE_CPPFLAGS) \
	-g -O2 \
	$(NULL)
bamql_compile_LDFLAGS =\
	$(Z_LDFLAGS) \
	$(LLVM_WRITE_LDFLAGS) \
	$(NULL)
bamql_compile_LDADD = \
	libbamql.la \
	$(Z_LIBS) \
	$(LLVM_WRITE_LIBS) \
	$(NULL)

check_bamql_SOURCES = \
	check.cpp \
	$(NULL)
check_bamql_CPPFLAGS = \
	-std=c++11 \
	$(HTS_CFLAGS) -g -O2 \
	$(LLVM_RUN_CPPFLAGS) \
	$(NULL)
check_bamql_LDFLAGS =\
	$(LLVM_RUN_LDFLAGS) \
	$(NULL)
check_bamql_LDADD = \
	libbamql.la \
	libbamql-jit.la \
	$(HTS_LIBS) \
	$(LLVM_RUN_LIBS) \
	$(NULL)

runtime.bc: runtime.c
	## For reasons that are totally incomprehensible, LLC will die if -g is
	## provided, so we strip it out.
	$(CLANG) -c -emit-llvm -o $@ $$(echo $(HTS_CFLAGS) | sed 's/-g//g') -O2 $<

runtime.cpp: runtime.bc
	echo -e "#include <llvm/IR/IRBuilder.h>\n#include <llvm/IR/Module.h>\nusing namespace llvm;\nnamespace bamql {" > $@
	$(LLC) -march=cpp -cppfname=define_runtime --cppfor=__dummy__ -cppgen=contents -o - $< | sed '/setModuleIdentifier/d' >> $@
	echo "}" >> $@

EXTRA_DIST = \
	bamql.hpp \
	runtime.c \
	$(NULL)

CLEANFILES = \
	runtime.bc \
	runtime.cpp \
	$(NULL)
