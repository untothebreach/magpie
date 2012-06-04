#!/usr/bin/python

# Generates the C++ code for defining the AST classes. The classes for defining
# the Nodes are pretty much complete boilerplate. They are basically dumb
# data structures. This generates that boilerplate.
from os.path import dirname, join, realpath

magpie_dir = dirname(dirname(realpath(__file__)))
header_path = join(magpie_dir, 'src', 'Syntax', 'Ast.generated.h')

# Define the AST classes.
defs = sorted({
    'Method': [
        ('leftParam',   'gc<Pattern>'),
        ('name',        'gc<String>'),
        ('rightParam',  'gc<Pattern>'),
        ('body',        'gc<Expr>')],
}.items())

exprs = sorted({
    'And': [
        ('left',        'gc<Expr>'),
        ('right',       'gc<Expr>')],
    'BinaryOp': [
        ('left',        'gc<Expr>'),
        ('type',        'TokenType'),
        ('right',       'gc<Expr>')],
    'Bool': [
        ('value',       'bool')],
    'Call': [
        ('leftArg',     'gc<Expr>'),
        ('name',        'gc<String>'),
        ('rightArg',    'gc<Expr>')],
    'Catch': [
        ('body',        'gc<Expr>'),
        ('catches',     'Array<MatchClause>')],
    'Do': [
        ('body',        'gc<Expr>')],
    'If': [
        ('condition',   'gc<Expr>'),
        ('thenArm',     'gc<Expr>'),
        ('elseArm',     'gc<Expr>')],
    'Is': [
        ('value',       'gc<Expr>'),
        ('type',        'gc<Expr>')],
    'Match': [
        ('value',       'gc<Expr>'),
        ('cases',       'Array<MatchClause>')],
    'Name': [
        ('name',        'gc<String>'),
        ('resolved*',   'ResolvedName')],
    'Not': [
        ('value',       'gc<Expr>')],
    'Nothing': [],
    'Number': [
        ('value',       'double')],
    'Or': [
        ('left',        'gc<Expr>'),
        ('right',       'gc<Expr>')],
    'Record': [
        ('fields',      'Array<Field>')],
    'Return': [
        ('value',       'gc<Expr>')],
    'Sequence': [
        ('expressions', 'Array<gc<Expr> >')],
    'String': [
        ('value',       'gc<String>')],
    'Throw': [
        ('value',       'gc<Expr>')],
    'Variable': [
        ('isMutable',   'bool'),
        ('pattern',     'gc<Pattern>'),
        ('value',       'gc<Expr>')]
}.items())

patterns = sorted({
    'Record': [
        ('fields',      'Array<PatternField>')],
    'Type': [
        ('type',        'gc<Expr>')],
    'Value': [
        ('value',       'gc<Expr>')],
    'Variable': [
        ('name',        'gc<String>'),
        ('pattern',     'gc<Pattern>')],
    'Wildcard': []
}.items())

HEADER = '''// Automatically generated by script/generate_ast.py.
// Do not hand-edit.

'''

REACH_METHOD = '''
  virtual void reach()
  {{
{0}  }}
'''

REACH_FIELD_ARRAY = '''
    for (int i = 0; i < {0}_.count(); i++)
    {{
        Memory::reach({0}_[i].name);
        Memory::reach({0}_[i].value);
    }}
'''

# TODO(bob): The "int arg" param is useless for the Def visitor. Should omit it.
BASE_CLASS = '''
class {0} : public Managed
{{
public:
  {0}(const SourcePos& pos)
  : pos_(pos){2}
  {{}}

  virtual ~{0}() {{}}

  // The visitor pattern.
  virtual void accept({0}Visitor& visitor, int arg) const = 0;

  // Dynamic casts.
{1}
  const SourcePos& pos() const {{ return pos_; }}
{4}
private:
  SourcePos pos_;{3}
}};
'''

# TODO(bob): The "int arg" param is useless for the Def visitor. Should omit it.
SUBCLASS = '''
class {0}{6} : public {6}
{{
public:
  {0}{6}(const SourcePos& pos{1})
  : {6}(pos){2}
  {{}}

  virtual void accept({6}Visitor& visitor, int arg) const
  {{
    visitor.visit(*this, arg);
  }}

  virtual const {0}{6}* as{0}{6}() const {{ return this; }}

{3}{4}
  virtual void trace(std::ostream& out) const;

private:{5}
}};
'''

VISITOR_HEADER = '''
class {0}Visitor
{{
public:
  virtual ~{0}Visitor() {{}}

'''

VISITOR_FOOTER = '''
protected:
  {0}Visitor() {{}}

private:
  NO_COPY({0}Visitor);
}};
'''

EXPR_INITIALIZERS = ''',
    numLocals_(-1)'''

EXPR_FIELDS = '''
  int numLocals_;
'''

EXPR_METHODS = '''
  int numLocals() const {
    ASSERT(numLocals_ != -1, "Expression has not been resolved yet.");
    return numLocals_;
  }

  void setNumLocals(int numLocals) {
    ASSERT(numLocals_ == -1, "Expression is already resolved.");
    numLocals_ = numLocals;
  }
'''

num_types = 0

def main():
    # Create the AST header.
    with open(header_path, 'w') as file:
        file.write(HEADER)

        # Write the forward declarations.
        forwardDeclare(file, defs, 'Def')
        forwardDeclare(file, exprs, 'Expr')
        forwardDeclare(file, patterns, 'Pattern')

        makeAst(file, 'Def', defs)
        makeAst(file, 'Expr', exprs)
        makeAst(file, 'Pattern', patterns)

    print 'Created', num_types, 'types.'


def forwardDeclare(file, nodes, name):
    for className, fields in nodes:
        file.write('class {0}{1};\n'.format(className, name))


def makeAst(file, name, types):
    makeVisitor(file, name, types)
    makeBaseClass(file, name, types)
    for type, fields in types:
        makeClass(file, name, type, fields)


def makeVisitor(file, name, types):
    result = VISITOR_HEADER.format(name)

    for className, fields in types:
        result += ('  virtual void visit(const {1}{0}& node, int dest) = 0;\n'
            .format(name, className))

    file.write(result + VISITOR_FOOTER.format(name))


def makeClass(file, baseClass, className, fields):
    global num_types
    num_types += 1
    ctorParams = ''
    ctorArgs = ''
    accessors = ''
    memberVars = ''
    reachFields = ''
    for name, type in fields:
        mutable = False
        if name.endswith('*'):
            mutable = True
            name = name[:-1]

        if type.find('gc<') != -1:
            reachFields += '    Memory::reach(' + name + '_);\n'
        if type == 'Array<Field>' or type == 'Array<PatternField>':
            reachFields += REACH_FIELD_ARRAY.format(name)

        if not mutable:
            ctorParams += ', '
            if type.startswith('Array'):
                # Array fields are passed to the constructor by reference.
                ctorParams += 'const ' + type + '&'
            else:
                ctorParams += type
            ctorParams += ' ' + name
            ctorArgs += ',\n    {0}_({0})'.format(name)
        else:
            ctorArgs += ',\n    {0}_()'.format(name)

        if type.startswith('Array'):
            # Accessors for arrays do not copy.
            accessors += '  const {1}& {0}() const {{ return {0}_; }}\n'.format(
                name, type)
        else:
            accessors += '  {1} {0}() const {{ return {0}_; }}\n'.format(
                name, type)

        # Include a setter too if it's mutable.
        if mutable:
            accessors += '  void set{2}({1} {0}) {{ {0}_ = {0}; }}\n'.format(
                name, type, name.title())

        memberVars += '\n  {1} {0}_;'.format(name, type)

    reach = ''
    # Only generate a reach() method if the class contains a GC-ed field.
    if reachFields != '':
        reach = REACH_METHOD.format(reachFields)

    file.write(SUBCLASS.format(className, ctorParams, ctorArgs, accessors,
                               reach, memberVars, baseClass))

def makeBaseClass(file, name, types):
    casts = ''
    for subclass, fields in types:
        casts += '  virtual const {1}{0}* as{1}{0}()'.format(name, subclass)
        casts += ' const { return NULL; }\n'

    initializers = ''
    fields = ''
    methods = ''

    if name == 'Expr':
        initializers = EXPR_INITIALIZERS
        fields = EXPR_FIELDS
        methods = EXPR_METHODS

    file.write(BASE_CLASS.format(name, casts, initializers, fields, methods))

main()