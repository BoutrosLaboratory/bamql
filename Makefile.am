NULL = 
ACLOCAL_AMFLAGS = -I m4
bin_PROGRAMS = \
	bamql \
	bamql-chain \
	bamql-compile \
	bamql-pairs \
	bamql-script \
	$(NULL)
lib_LTLIBRARIES = \
	libbamql-cpl.la \
	libbamql-itr.la \
	libbamql-rt.la \
	libbamql-jit.la \
	$(NULL)
library_includedir=$(includedir)/bamql
library_include_HEADERS = \
	compiler/bamql-compiler.hpp \
	iterator/bamql-iterator.hpp \
	jit/bamql-jit.hpp \
	runtime/bamql-runtime.h \
	$(NULL)
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = \
	compiler/bamql-cpl.pc \
	iterator/bamql-itr.pc \
	jit/bamql-jit.pc \
	runtime/bamql-rt.pc \
	$(NULL)
man1_MANS = \
	tools/bamql.1 \
	tools/bamql-chain.1 \
	tools/bamql-compile.1 \
	tools/bamql-pairs.1 \
	tools/bamql-script.1 \
	$(NULL)
man7_MANS = \
	bamql_queries.7 \
	$(NULL)

TESTS = \
	check_bamql \
	test/check_compiler \
	test/nt_exact_test.bamql \
	$(NULL)
check_PROGRAMS = \
	check_bamql \
	$(NULL)

TEST_EXTENSIONS = \
	.bamql \
	$(NULL)
BAMQL_LOG_COMPILER = ./test/bamql_pass_all

libbamql_cpl_la_CPPFLAGS = \
	$(LLVM_CORE_CPPFLAGS) \
	$(HTS_CFLAGS) \
	$(PCRE_CFLAGS) \
	-g -O2 \
	$(NULL)
libbamql_cpl_la_LIBADD = \
	$(LLVM_CORE_LIBS) \
	$(PCRE_LIBS) \
	$(NULL)
libbamql_cpl_la_LDFLAGS = \
	$(LLVM_CORE_LDFLAGS) \
	-version-info 1:0:0 \
	-no-undefined \
	$(NULL)
libbamql_cpl_la_SOURCES = \
	compiler/ast_node_binding.cpp \
	compiler/ast_node_chromosome.cpp \
	compiler/ast_node_compare.cpp \
	compiler/ast_node_contains.cpp \
	compiler/ast_node.cpp \
	compiler/ast_node_function.cpp \
	compiler/ast_node_if.cpp \
	compiler/ast_node_literal.cpp \
	compiler/ast_node_logical.cpp \
	compiler/ast_node_loop.cpp \
	compiler/ast_node_match_binding.cpp \
	compiler/ast_node_optima.cpp \
	compiler/ast_node_regex.cpp \
	compiler/bed.cpp \
	compiler/generator.cpp \
	compiler/misc.cpp \
	compiler/parser.cpp \
	compiler/parser_misc.cpp \
	compiler/pcre.cpp \
	compiler/predicates.cpp \
	compiler/version.cpp \
	$(NULL)

libbamql_rt_la_CPPFLAGS = \
	-std=c99 \
	$(HTS_CFLAGS) \
	$(PCRE_CFLAGS) \
	-g -O2 \
	$(NULL)
libbamql_rt_la_LIBADD = \
	$(PCRE_LIBS) \
	$(HTS_LIBS) \
	$(NULL)
libbamql_rt_la_LDFLAGS = \
	-version-info 1:0:1 \
	-no-undefined \
	$(NULL)
libbamql_rt_la_SOURCES = \
	runtime/runtime.c \
	$(NULL)

libbamql_itr_la_CPPFLAGS = \
	$(HTS_CFLAGS) \
	$(UUID_CFLAGS) \
	-std=c++14 \
	-g -O2 \
	$(NULL)
libbamql_itr_la_LIBADD = \
	$(HTS_LIBS) \
	$(UUID_LIBS) \
	$(NULL)
libbamql_itr_la_LDFLAGS = \
	-version-info 0:0:0 \
	-no-undefined \
	$(NULL)
libbamql_itr_la_SOURCES = \
	iterator/harness.cpp \
	iterator/misc.cpp \
	iterator/reader.cpp \
	$(NULL)

libbamql_jit_la_CPPFLAGS = \
	$(LLVM_RUN_CPPFLAGS) \
	$(HTS_CFLAGS) -g -O2 \
	-Icompiler \
	-Iiterator \
	-Iruntime \
	$(NULL)
libbamql_jit_la_LIBADD = \
	libbamql-cpl.la \
	libbamql-itr.la \
	libbamql-rt.la \
	$(LLVM_RUN_LIBS) \
	$(HTS_LIBS) \
	$(NULL)
libbamql_jit_la_LDFLAGS = \
	$(LLVM_RUN_LDFLAGS) \
	-version-info 0:0:0 \
	-no-undefined \
	$(NULL)
libbamql_jit_la_SOURCES = \
	jit/misc.cpp \
	jit/reader.cpp \
	$(NULL)

bamql_pairs_SOURCES = \
	tools/pairs.cpp \
	$(NULL)
bamql_pairs_CPPFLAGS = \
	$(HTS_CFLAGS) -g -O2 \
	$(LLVM_RUN_CPPFLAGS) \
	$(UUID_CFLAGS) \
	-Icompiler \
	-Iiterator \
	-Ijit \
	-Iruntime \
	$(NULL)
bamql_pairs_LDFLAGS =\
	$(LLVM_RUN_LDFLAGS) \
	$(NULL)
bamql_pairs_LDADD = \
	libbamql-cpl.la \
	libbamql-itr.la \
	libbamql-jit.la \
	$(HTS_LIBS) \
	$(LLVM_RUN_LIBS) \
	$(UUID_LIBS) \
	$(NULL)

bamql_SOURCES = \
	tools/run.cpp \
	$(NULL)
bamql_CPPFLAGS = \
	$(HTS_CFLAGS) -g -O2 \
	$(LLVM_RUN_CPPFLAGS) \
	$(UUID_CFLAGS) \
	-Icompiler \
	-Iiterator \
	-Ijit \
	-Iruntime \
	$(NULL)
bamql_LDFLAGS =\
	$(LLVM_RUN_LDFLAGS) \
	$(NULL)
bamql_LDADD = \
	libbamql-cpl.la \
	libbamql-itr.la \
	libbamql-jit.la \
	$(HTS_LIBS) \
	$(LLVM_RUN_LIBS) \
	$(UUID_LIBS) \
	$(NULL)

bamql_chain_SOURCES = \
	tools/chain.cpp \
	$(NULL)
bamql_chain_CPPFLAGS = \
	$(HTS_CFLAGS) -g -O2 \
	$(LLVM_RUN_CPPFLAGS) \
	$(UUID_CFLAGS) \
	-Icompiler \
	-Iiterator \
	-Ijit \
	-Iruntime \
	$(NULL)
bamql_chain_LDFLAGS =\
	$(LLVM_RUN_LDFLAGS) \
	$(NULL)
bamql_chain_LDADD = \
	libbamql-cpl.la \
	libbamql-itr.la \
	libbamql-jit.la \
	$(HTS_LIBS) \
	$(LLVM_RUN_LIBS) \
	$(UUID_LIBS) \
	$(NULL)

bamql_compile_SOURCES = \
	tools/compile.cpp \
	$(NULL)
bamql_compile_CPPFLAGS = \
	$(LLVM_WRITE_CPPFLAGS) \
	-g -O2 \
	-Icompiler \
	$(NULL)
bamql_compile_LDFLAGS =\
	$(LLVM_WRITE_LDFLAGS) \
	$(NULL)
bamql_compile_LDADD = \
	libbamql-cpl.la \
	$(LLVM_WRITE_LIBS) \
	$(NULL)

bamql_script_SOURCES = \
	tools/script.c \
	$(NULL)
bamql_script_CPPFLAGS = \
	-std=c99 \
	$(NULL)

check_bamql_SOURCES = \
	test/check.cpp \
	$(NULL)
check_bamql_CPPFLAGS = \
	$(HTS_CFLAGS) -g -O2 \
	$(LLVM_RUN_CPPFLAGS) \
	-Icompiler \
	-Iiterator \
	-Ijit \
	-Iruntime \
	$(NULL)
check_bamql_LDFLAGS = \
	$(LLVM_RUN_LDFLAGS) \
	$(NULL)
check_bamql_LDADD = \
	libbamql-cpl.la \
	libbamql-itr.la \
	libbamql-jit.la \
	$(HTS_LIBS) \
	$(LLVM_RUN_LIBS) \
	$(NULL)

CLEANFILES = \
	compiler_check.h \
	compiler_check.o \
	$(libbamql_cpl_la_OBJECTS:.lo=.dwo) \
	$(libbamql_itr_la_OBJECTS:.lo=.dwo) \
	$(libbamql_jit_la_OBJECTS:.lo=.dwo) \
	$(libbamql_rt_la_OBJECTS:.lo=.dwo) \
	$(bamql_OBJECTS:.lo=.dwo) \
	$(bamql_chain_OBJECTS:.lo=.dwo) \
	$(bamql_compile_OBJECTS:.lo=.dwo) \
	$(NULL)
