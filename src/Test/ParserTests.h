#pragma once

#include "Test.h"

namespace magpie
{
  class ParserTests : public Test
  {
  public:
    virtual void runTests();

  private:
    void parseModule();
    void testCode(const char*, const char*);
  };
}

