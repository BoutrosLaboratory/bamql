NULL = 
ACLOCAL_AMFLAGS = -I m4
bin_PROGRAMS = \
	bamql \
	bamql-chain \
	bamql-compile \
	$(NULL)
lib_LTLIBRARIES = \
	libbamql-cpl.la \
	libbamql-itr.la \
	libbamql-rt.la \
	libbamql-jit.la \
	$(NULL)
library_includedir=$(includedir)/bamql
library_include_HEADERS = \
	bamql-compiler.hpp \
	bamql-iterator.hpp \
	bamql-jit.hpp \
	bamql-runtime.h \
	$(NULL)
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = \
	bamql-cpl.pc \
	bamql-itr.pc \
	bamql-jit.pc \
	bamql-rt.pc \
	$(NULL)
man1_MANS = \
	bamql.1 \
	bamql-chain.1 \
	bamql-compile.1 \
	$(NULL)
man7_MANS = \
	bamql_queries.7 \
	$(NULL)

TESTS = \
	check_bamql \
	check_compiler \
	nt_exact_test.bamql \
	$(NULL)
check_PROGRAMS = \
	check_bamql \
	$(NULL)

TEST_EXTENSIONS = \
	.bamql \
	$(NULL)
BAMQL_LOG_COMPILER = ./bamql_pass_all

libbamql_cpl_la_CPPFLAGS = \
	-std=c++11 \
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
	-version-info 0:0:0 \
	-no-undefined \
	$(NULL)
libbamql_cpl_la_SOURCES = \
	ast_node_logical.cpp \
	misc.cpp \
	parser.cpp \
	parser_misc.cpp \
	pcre.cpp \
	predicates.cpp \
	version.cpp \
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
	-version-info 0:0:0 \
	-no-undefined \
	$(NULL)
libbamql_rt_la_SOURCES = \
	runtime.c \
	$(NULL)

libbamql_itr_la_CPPFLAGS = \
	-std=c++11 \
	$(HTS_CFLAGS) \
	-g -O2 \
	$(NULL)
libbamql_itr_la_LIBADD = \
	$(HTS_LIBS) \
	$(NULL)
libbamql_itr_la_LDFLAGS = \
	-version-info 0:0:0 \
	-no-undefined \
	$(NULL)
libbamql_itr_la_SOURCES = \
	iterator-misc.cpp \
	iterator-reader.cpp \
	$(NULL)

libbamql_jit_la_CPPFLAGS = \
	-std=c++11 \
	$(LLVM_RUN_CPPFLAGS) \
	$(HTS_CFLAGS) -g -O2 \
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
	jit-misc.cpp \
	jit-reader.cpp \
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
	libbamql-cpl.la \
	libbamql-itr.la \
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
	libbamql-cpl.la \
	libbamql-itr.la \
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
	libbamql-cpl.la \
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
	$(NULL)
