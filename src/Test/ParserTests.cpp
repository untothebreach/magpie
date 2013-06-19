#include "Syntax/Ast.h"
#include "ParserTests.h"
#include "Memory/Memory.h"
#include "Syntax/Parser.h"
#include "Memory/RootSource.h"

namespace magpie
{
  void ParserTests::runTests()
  {
    parseModule();
  }

  // TODO: hacky, parses arbitrary code string into a module
  void ParserTests::testCode(const char* sourceString, const char* expected)
  {
    gc<String> code = String::create(sourceString);
    ErrorReporter reporter;
    gc<SourceFile> source = new SourceFile(String::create("<file>"), code);
    Parser parser(source, reporter);
    gc<ModuleAst> module = parser.parseModule();

    int numErrors = reporter.numErrors();

    EXPECT_EQUAL(0, numErrors);

    if (numErrors == 0) {
        gc<String> text = module->body()->toString();
        EXPECT_EQUAL(expected, *text);
    }
  }

  void ParserTests::parseModule()
  {
    testCode("def foo()\n1+2*3 and 4/5+6%7\nend",
             "(\n(def foo -> (and (1 + (2 * 3)) ((4 / 5) + (6 % 7))))\n)");
  }
}

