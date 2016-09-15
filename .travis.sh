#!/bin/sh

set -eu
autoreconf -i
if ./configure --enable-static-llvm=${LLVM_STATIC} --with-llvm=/usr/bin/llvm-config-${LLVM_TARGET_VERSION}
then
	make all check
else
	cat config.log
	exit 1
fi
