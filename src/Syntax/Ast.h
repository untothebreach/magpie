#pragma once

#include "Array.h"
#include "Macros.h"
#include "Managed.h"
#include "Token.h"

namespace magpie
{
  using std::ostream;

  class Def;
  class Expr;
  class Pattern;
  
  // A record field.
  struct Field
  {
    Field()
    : name(),
      value()
    {}
    
    Field(gc<String> name, gc<Expr> value)
    : name(name),
      value(value)
    {}

    gc<String> name;
    gc<Expr> value;
  };
  
  // A record pattern field.
  struct PatternField
  {
    PatternField()
    : name(),
      value()
    {}
    
    PatternField(gc<String> name, gc<Pattern> value)
    : name(name),
      value(value)
    {}
    
    gc<String> name;
    gc<Pattern> value;
  };
  
  // A pattern paired with the expression to execute when the pattern matches.
  // Used for match expressions and catch clauses.
  class MatchClause
  {
  public:
    MatchClause()
    : pattern_(),
      body_()
    {}
    
    MatchClause(gc<Pattern> pattern, gc<Expr> body)
    : pattern_(pattern),
      body_(body)
    {}
    
    gc<Pattern> pattern() const { return pattern_; }
    gc<Expr> body() const { return body_; }
    
  private:
    gc<Pattern> pattern_;
    gc<Expr> body_;
  };
  
  class ModuleAst : public Managed
  {
  public:
    ModuleAst(const Array<gc<Def> >& defs, gc<Expr> body)
    : defs_(defs),
      body_(body)
    {}
    
    const Array<gc<Def> >& defs() const { return defs_; }
    gc<Expr> body() const { return body_; }
    
    virtual void reach()
    {
      Memory::reach(defs_);
      Memory::reach(body_);
    }
    
  private:
    Array<gc<Def> > defs_;
    gc<Expr>        body_;
  };
  
#include "Ast.generated.h"
}

