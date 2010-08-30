package com.stuffwithstuff.magpie.interpreter;

import java.util.ArrayList;
import java.util.List;
import java.util.Map.Entry;

import com.stuffwithstuff.magpie.Identifiers;
import com.stuffwithstuff.magpie.ast.*;

/**
 * Implements the visitor pattern on AST nodes, in order to evaluate
 * expressions. This is the heart of the interpreter and is where Magpie code is
 * actually executed.
 */
public class ExprEvaluator implements ExprVisitor<Obj, EvalContext> {
  public ExprEvaluator(Interpreter interpreter) {
    mInterpreter = interpreter;
  }
  
  /**
   * Evaluates the given expression in the given context.
   * @param   expr     The expression to evaluate.
   * @param   context  The context in which to evaluate the expression.
   * @return           The result of evaluating the expression.
   */
  public Obj evaluate(Expr expr, EvalContext context) {
    return expr.accept(this, context);
  }

  @Override
  public Obj visit(ArrayExpr expr, EvalContext context) {
    // Evaluate the elements.
    List<Obj> elements = new ArrayList<Obj>(expr.getElements().size());
    for (int i = 0; i < expr.getElements().size(); i++) {
      elements.add(evaluate(expr.getElements().get(i), context));
    }

    return mInterpreter.createArray(elements);
  }

  @Override
  public Obj visit(AndExpr expr, EvalContext context) {
    Obj left = evaluate(expr.getLeft(), context);
        
    if (isTruthy(expr, left)) {
      return evaluate(expr.getRight(), context);
    } else {
      return left;
    }
  }

  @Override
  public Obj visit(AssignExpr expr, EvalContext context) {
    String name = expr.getName();
    String setter = Identifiers.makeSetter(name);
    
    if (expr.getTarget() == null) {
      // No target means we're just assigning to a variable (or field of this)
      // with the given name.
      Obj value = evaluate(expr.getValue(), context);
      
      // Try to assign to a local.
      if (context.assign(name, value)) return value;
      
      // Otherwise, it must be a setter on this.
      return mInterpreter.invokeMethod(expr, context.getThis(), setter, value);
    } else {
      // The target of the assignment is an actual expression, like a.b = c
      Obj target = evaluate(expr.getTarget(), context);
      Obj value = evaluate(expr.getValue(), context);

      // If the assignment statement has an argument and a value, like:
      // a b(c) = v (c is the arg, v is the value)
      // then bundle them together:
      if (expr.getTargetArg() != null) {
        Obj targetArg = evaluate(expr.getTargetArg(), context);
        value = mInterpreter.createTuple(targetArg, value);
      }

      // Invoke the setter method.
      return mInterpreter.invokeMethod(expr, target, setter, value);
    }
  }

  @Override
  public Obj visit(BlockExpr expr, EvalContext context) {
    Obj result = null;
    
    // Create a lexical scope.
    EvalContext localContext = context.nestScope();
    
    // Evaluate all of the expressions and return the last.
    for (Expr thisExpr : expr.getExpressions()) {
      result = evaluate(thisExpr, localContext);
    }
    
    return result;
  }

  @Override
  public Obj visit(BoolExpr expr, EvalContext context) {
    return mInterpreter.createBool(expr.getValue());
  }

  @Override
  public Obj visit(ClassExpr expr, EvalContext context) {
    // Look up the class if we are extending one, otherwise create it.
    ClassObj metaclass;
    ClassObj classObj;
    if (expr.isExtend()) {
      // TODO(bob): Should this be a generic expression that returns a class?
      // Like: class foo.bar.bang
      Obj obj = context.lookUp(expr.getName());
      
      if (obj == null) {
        mInterpreter.runtimeError(expr,
            "Could not find a class object named \"%s\".", expr.getName());
        return mInterpreter.nothing();
      }
      
      if (!(obj instanceof ClassObj)) {
        mInterpreter.runtimeError(expr, "Object \"%s\" is not a class.",
            expr.getName());
        return mInterpreter.nothing();
      }
      
      classObj = (ClassObj)obj;
      metaclass = classObj.getClassObj();
    } else {
      classObj = mInterpreter.createClass(expr.getName(), context.getScope());
      metaclass = classObj.getClassObj();
      
      // Add the constructor method.
      metaclass.addMethod(Identifiers.NEW, new NativeMethod.ClassNew(expr.getName()));
    }
    
    // Add the constructors.
    for (FnExpr constructorFn : expr.getConstructors()) {
      FnObj fnObj = mInterpreter.createFn(constructorFn, context.getScope());
      classObj.addConstructor(fnObj);
    }
    
    // Evaluate and define the shared fields.
    EvalContext classContext = context.withThis(classObj);
    for (Entry<String, Expr> field : expr.getSharedFields().entrySet()) {
      Obj value = evaluate(field.getValue(), classContext);
      
      classObj.setField(field.getKey(), value);
      
      // Add a getter.
      metaclass.addMethod(field.getKey(),
          new NativeMethod.ClassFieldGetter(field.getKey(), Expr.name("Dynamic")));
      
      // Add a setter.
      String setter = Identifiers.makeSetter(field.getKey());
      metaclass.addMethod(setter,
          new NativeMethod.ClassFieldSetter(field.getKey(), Expr.name("Dynamic")));
    }
    
    // Define the shared methods.
    for (Entry<String, List<FnExpr>> methods : expr.getSharedMethods().entrySet()) {
      for (FnExpr method : methods.getValue()) {
        FnObj methodObj = mInterpreter.createFn(method, context.getScope());
        metaclass.addMethod(methods.getKey(), methodObj);
      }
    }
    
    // Define the instance methods.
    for (Entry<String, List<FnExpr>> methods : expr.getMethods().entrySet()) {
      for (FnExpr method : methods.getValue()) {
        FnObj methodObj = mInterpreter.createFn(method, context.getScope());
        classObj.addMethod(methods.getKey(), methodObj);
      }
    }
    
    // Define the getters and setters for the fields.
    for (String field : expr.getFields().keySet()) {
      // Note that we don't provide type expressions for the fields here because
      // we only know the initializer expression. We don't have an actual type
      // annotation.
      
      // Add a getter.
      classObj.addMethod(field,
          new NativeMethod.ClassFieldGetter(field, Expr.name("Dynamic")));
      
      // Add a setter.
      classObj.addMethod(Identifiers.makeSetter(field),
          new NativeMethod.ClassFieldSetter(field, Expr.name("Dynamic")));
    }
    
    for (Entry<String, Expr> entry : expr.getFieldDeclarations().entrySet()) {
      // Add a getter.
      classObj.addMethod(entry.getKey(),
          new NativeMethod.ClassFieldGetter(entry.getKey(), entry.getValue()));
      
      // Add a setter.
      classObj.addMethod(Identifiers.makeSetter(entry.getKey()),
          new NativeMethod.ClassFieldSetter(entry.getKey(), entry.getValue()));
    }
    
    // Add the field initializers to the class so it can evaluate them when an
    // object is constructed.
    classObj.defineFields(expr.getFields());
    
    // Define a variable for the class in the current scope.
    context.define(expr.getName(), classObj);
    return classObj;
  }

  @Override
  public Obj visit(FnExpr expr, EvalContext context) {
    return mInterpreter.createFn(expr, context.getScope());
  }

  @Override
  public Obj visit(IfExpr expr, EvalContext context) {
    // Put it in a block so that variables declared in conditions end when the
    // if expression ends.
    context = context.nestScope();
    
    // Evaluate all of the conditions.
    boolean passed = true;
    for (Condition condition : expr.getConditions()) {
      if (condition.isLet()) {
        // "let" condition.
        Obj result = evaluate(condition.getBody(), context);
        
        // If it evaluates to nothing, the condition fails. Otherwise, bind the
        // result to a name and continue.
        if (result != mInterpreter.nothing()) {
          // Success, bind the result.
          context.define(condition.getName(), result);
        } else {
          // Condition failed.
          passed = false;
          break;
        }
      } else {
        // Regular "if" condition.
        Obj result = evaluate(condition.getBody(), context);
        if (!isTruthy(expr, result)) {
          // Condition failed.
          passed = false;
          break;
        }
      }
    }
    
    // Evaluate the body.
    if (passed) {
      return evaluate(expr.getThen(), context);
    } else {
      return evaluate(expr.getElse(), context);
    }
  }

  @Override
  public Obj visit(IntExpr expr, EvalContext context) {
    return mInterpreter.createInt(expr.getValue());
  }

  @Override
  public Obj visit(LoopExpr expr, EvalContext context) {
    boolean done = false;
    while (true) {
      // Evaluate the conditions.
      for (Expr conditionExpr : expr.getConditions()) {
        // See if the while clause is still true.
        Obj condition = evaluate(conditionExpr, context);
        if (!isTruthy(conditionExpr, condition)) {
          done = true;
          break;
        }
      }
      
      // If any clause failed, stop the loop.
      if (done) break;
      
      evaluate(expr.getBody(), context);
    }
    
    // TODO(bob): It would be cool if loops could have "else" clauses and then
    // reliably return a value.
    return mInterpreter.nothing();
  }

  @Override
  public Obj visit(MessageExpr expr, EvalContext context) {
    Obj receiver = (expr.getReceiver() == null) ? null :
        evaluate(expr.getReceiver(), context);
    
    Obj arg = (expr.getArg() == null) ? null :
        evaluate(expr.getArg(), context);
    
    if (receiver == null) {
      // Just a name, so maybe it's a variable.
      Obj variable = context.lookUp(expr.getName());

      if (variable != null) {
        // If we have an argument, apply it.
        if (arg != null) {
          return mInterpreter.invokeMethod(
              expr, variable, Identifiers.CALL, arg);
        }
        return variable;
      }
      
      // Otherwise it must be a method on this.
      return mInterpreter.invokeMethod(
          expr, context.getThis(), expr.getName(), arg);
    }
    
    return mInterpreter.invokeMethod(expr, receiver, expr.getName(), arg);
  }
  
  @Override
  public Obj visit(NothingExpr expr, EvalContext context) {
    return mInterpreter.nothing();
  }

  @Override
  public Obj visit(OrExpr expr, EvalContext context) {
    Obj left = evaluate(expr.getLeft(), context);
    
    if (isTruthy(expr, left)) {
      return left;
    } else {
      return evaluate(expr.getRight(), context);
    }
  }

  @Override
  public Obj visit(ReturnExpr expr, EvalContext context) {
    Obj value = evaluate(expr.getValue(), context);
    throw new ReturnException(value);
  }

  @Override
  public Obj visit(StringExpr expr, EvalContext context) {
    return mInterpreter.createString(expr.getValue());
  }

  @Override
  public Obj visit(ThisExpr expr, EvalContext context) {
    return context.getThis();
  }

  @Override
  public Obj visit(TypeofExpr expr, EvalContext context) {
    Checker checker = new Checker(mInterpreter);
    return checker.evaluateExpressionType(expr.getBody());
  }

  @Override
  public Obj visit(TupleExpr expr, EvalContext context) {
    // Evaluate the fields.
    Obj[] fields = new Obj[expr.getFields().size()];
    for (int i = 0; i < fields.length; i++) {
      fields[i] = evaluate(expr.getFields().get(i), context);
    }

    return mInterpreter.createTuple(fields);
  }

  @Override
  public Obj visit(VariableExpr expr, EvalContext context) {
    Obj value = evaluate(expr.getValue(), context);

    context.define(expr.getName(), value);
    return value;
  }
  
  private boolean isTruthy(Expr expr, Obj receiver) {
    Obj truthy = mInterpreter.invokeMethod(expr, receiver, Identifiers.IS_TRUE, null);
    return truthy.asBool();
  }
  
  private final Interpreter mInterpreter;
}