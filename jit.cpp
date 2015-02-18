#include <cstdio>
#include <iostream>
#include <sstream>
#include "barf-jit.hpp"

std::shared_ptr<llvm::ExecutionEngine> barf::createEngine(
    llvm::Module *module) {
  std::string error;
  std::vector<std::string> attrs;
  attrs.push_back("-avx"); // The AVX support (auto-vectoring) should be
                           // disabled since the JIT isn't smart enough to
                           // detect this, even though there is a detection
                           // routine. In particular, it makes Valgrind not
                           // work.
  std::shared_ptr<llvm::ExecutionEngine> engine(
      llvm::EngineBuilder(module)
          .setEngineKind(llvm::EngineKind::JIT)
          .setErrorStr(&error)
          .setMAttrs(attrs)
          .create());
  if (!engine) {
    std::cout << error << std::endl;
  }
  return engine;
}
