#include "Fiber.h"

#include "Method.h"
#include "Module.h"
#include "Object.h"
#include "VM.h"

namespace magpie
{
  Fiber::Fiber(VM& vm)
  : vm_(vm),
    stack_(),
    callFrames_(),
    nearestCatch_()
  {}

  void Fiber::init(gc<Method> method)
  {
    ASSERT(stack_.count() == 0, "Cannot re-initialize Fiber.");
    ASSERT(callFrames_.count() == 0, "Cannot re-initialize Fiber.");

    // TODO(bob): What should the arg object be here?
    call(method, 0, vm_.nothing());
  }

  FiberResult Fiber::run()
  {
    while (true)
    {
      if (Memory::checkCollect()) return FIBER_DID_GC;
      
      CallFrame& frame = callFrames_[-1];
      instruction ins = frame.method->code()[frame.ip++];
      OpCode op = GET_OP(ins);

      switch (op)
      {
        case OP_MOVE:
        {
          int from = GET_A(ins);
          int to = GET_B(ins);
          store(frame, to, load(frame, from));
          break;
        }

        case OP_CONSTANT:
        {
          int index = GET_A(ins);
          int reg = GET_B(ins);
          store(frame, reg, frame.method->getConstant(index));
          break;
        }
          
        case OP_BUILT_IN:
        {
          int value = GET_A(ins);
          int reg = GET_B(ins);
          store(frame, reg, vm_.getBuiltIn(value));
          break;
        }
          
        case OP_RECORD:
        {
          int firstReg = GET_A(ins);
          gc<RecordType> type = vm_.getRecordType(GET_B(ins));
          gc<Object> record = RecordObject::create(type, stack_, firstReg);
          store(frame, GET_C(ins), record);
          break;
        }
          
        case OP_GET_FIELD:
        {
          bool success = false;
          RecordObject* record = load(frame, GET_A(ins))->toRecord();
          
          // We can't pull record fields out of something that isn't a record.
          // TODO(bob): Should you be able to destructure arbitrary objects by
          // invoking getters with the right name?
          if (record != NULL)
          {
            int symbol = GET_B(ins);
            gc<Object> field = record->getField(symbol);

            // If the record doesn't have the field, fail the match.
            // TODO(bob): Throw NoMatchError.
            if (!field.isNull())
            {
              store(frame, GET_C(ins), field);
              success = true;
            }
          }
          
          // TODO(bob): Throw NoMatchError.
          if (!success && !throwError(vm_.getBool(false)))
          {
            return FIBER_UNCAUGHT_ERROR;
          }
          break;
        }
          
        case OP_TEST_FIELD:
        {
          bool success = false;
          RecordObject* record = load(frame, GET_A(ins))->toRecord();
          
          // The next instruction is a pseudo-instruction containing the offset
          // to jump to.
          instruction jump = frame.method->code()[frame.ip++];
          ASSERT(GET_OP(jump) == OP_JUMP,
                 "Pseudo-instruction after OP_TEST_FIELD must be OP_JUMP.");
          
          // We can't pull record fields out of something that isn't a record.
          // TODO(bob): Should you be able to destructure arbitrary objects by
          // invoking getters with the right name?
          if (record != NULL)
          {
            int symbol = GET_B(ins);
            gc<Object> field = record->getField(symbol);
            
            // If the record doesn't have the field, fail the match.
            // TODO(bob): Throw NoMatchError.
            if (!field.isNull())
            {
              store(frame, GET_C(ins), field);
              success = true;
            }
          }
          
          // Jump if the match failed.
          if (!success)
          {
            int offset = GET_A(jump);
            frame.ip += offset;
          }
          break;
        }
          
        case OP_GET_MODULE:
        {
          int importIndex = GET_A(ins);
          int exportIndex = GET_B(ins);
          gc<Object> object = frame.method->module()->getImport(importIndex,
                                                                exportIndex);
          store(frame, GET_C(ins), object);
          break;
        }
          
        case OP_ADD:
        {
          gc<Object> a = loadRegisterOrConstant(frame, GET_A(ins));
          gc<Object> b = loadRegisterOrConstant(frame, GET_B(ins));
          
          // TODO(bob): Handle non-number types.
          double c = a->toNumber() + b->toNumber();
          gc<Object> num = new NumberObject(c);
          store(frame, GET_C(ins), num);
          break;
        }
          
        case OP_SUBTRACT:
        {
          gc<Object> a = loadRegisterOrConstant(frame, GET_A(ins));
          gc<Object> b = loadRegisterOrConstant(frame, GET_B(ins));
          
          // TODO(bob): Handle non-number types.
          double c = a->toNumber() - b->toNumber();
          gc<Object> num = new NumberObject(c);
          store(frame, GET_C(ins), num);
          break;
        }
          
        case OP_MULTIPLY:
        {
          gc<Object> a = loadRegisterOrConstant(frame, GET_A(ins));
          gc<Object> b = loadRegisterOrConstant(frame, GET_B(ins));
          
          // TODO(bob): Handle non-number types.
          double c = a->toNumber() * b->toNumber();
          gc<Object> num = new NumberObject(c);
          store(frame, GET_C(ins), num);
          break;
        }
          
        case OP_DIVIDE:
        {
          gc<Object> a = loadRegisterOrConstant(frame, GET_A(ins));
          gc<Object> b = loadRegisterOrConstant(frame, GET_B(ins));
          
          // TODO(bob): Handle non-number types.
          double c = a->toNumber() / b->toNumber();
          gc<Object> num = new NumberObject(c);
          store(frame, GET_C(ins), num);
          break;
        }
          
        case OP_EQUAL:
        {
          gc<Object> a = loadRegisterOrConstant(frame, GET_A(ins));
          gc<Object> b = loadRegisterOrConstant(frame, GET_B(ins));
          
          // See if the objects are equal. If they have the same identity, they
          // must be.
          bool equal;
          if (a.sameAs(b))
          {
            equal = true;
          }
          else if (a->type() != b->type())
          {
            // Different types, so not equal.
            equal = false;
          }
          else
          {
            // Same type, so compare values.
            switch (a->type())
            {
              case OBJECT_BOOL:
                equal = a->toBool() == b->toBool();
                break;
                
              case OBJECT_CLASS:
                equal = false;
                break;
                
              case OBJECT_NOTHING:
                ASSERT(false, "Should only be one instance of nothing.");
                break;
                
              case OBJECT_NUMBER:
                equal = a->toNumber() == b->toNumber();
                break;
                
              case OBJECT_RECORD:
                ASSERT(false, "Equality on records not implemented.");
                break;
                
              case OBJECT_STRING:
                equal = a->toString() == b->toString();
                break;
            }
          }

          store(frame, GET_C(ins), vm_.getBool(equal));
          break;
        }
          
        case OP_LESS_THAN:
        {
          gc<Object> a = loadRegisterOrConstant(frame, GET_A(ins));
          gc<Object> b = loadRegisterOrConstant(frame, GET_B(ins));
          
          // TODO(bob): Handle non-number types.
          bool c = a->toNumber() < b->toNumber();
          store(frame, GET_C(ins), vm_.getBool(c));
          break;
        }
          
        case OP_GREATER_THAN:
        {
          gc<Object> a = loadRegisterOrConstant(frame, GET_A(ins));
          gc<Object> b = loadRegisterOrConstant(frame, GET_B(ins));
          
          // TODO(bob): Handle non-number types.
          bool c = a->toNumber() > b->toNumber();
          store(frame, GET_C(ins), vm_.getBool(c));
          break;
        }
          
        case OP_NOT:
        {
          gc<Object> value = loadRegisterOrConstant(frame, GET_A(ins));
          
          // TODO(bob): Handle user-defined types.
          bool result = !value->toBool();
          store(frame, GET_A(ins), vm_.getBool(result));
          break;
        }
          
        case OP_IS:
        {
          gc<Object> value = load(frame, GET_A(ins));
          
          // TODO(bob): Handle it not being a class.
          const ClassObject* expected = load(frame, GET_B(ins))->toClass();
          
          gc<Object> type;
          switch (value->type())
          {
            case OBJECT_BOOL:    type = vm_.boolClass(); break;
            case OBJECT_CLASS:   type = vm_.classClass(); break;
            case OBJECT_NOTHING: type = vm_.nothingClass(); break;
            case OBJECT_NUMBER:  type = vm_.numberClass(); break;
            case OBJECT_RECORD:  type = vm_.recordClass(); break;
            case OBJECT_STRING:  type = vm_.stringClass(); break;
            default:
              ASSERT(false, "Unknown object type.");
          }
          
          store(frame, GET_C(ins), vm_.getBool(type->toClass()->is(*expected)));
          break;
        }
          
        case OP_JUMP:
        {
          int offset = GET_A(ins);
          frame.ip += offset;
          break;
        }
          
        case OP_JUMP_IF_FALSE:
        {
          gc<Object> a = load(frame, GET_A(ins));
          if (!a->toBool())
          {
            int offset = GET_B(ins);
            frame.ip += offset;
          }
          break;
        }
          
        case OP_JUMP_IF_TRUE:
        {
          gc<Object> a = load(frame, GET_A(ins));
          if (a->toBool())
          {
            int offset = GET_B(ins);
            frame.ip += offset;
          }
          break;
        }
          
        case OP_CALL:
        {
          gc<Method> method = vm_.methods().get(GET_A(ins));
          gc<Object> arg = load(frame, GET_B(ins));
          
          Primitive primitive = method->primitive();
          if (primitive != NULL) {
            gc<Object> result = primitive(arg);
            store(frame, GET_B(ins), result);
          } else {
            int stackStart = frame.stackStart + frame.method->numRegisters();
            call(method, stackStart, arg);
          }
          break;
        }
          
        case OP_RETURN:
        {
          gc<Object> result = loadRegisterOrConstant(frame, GET_A(ins));
          callFrames_.removeAt(-1);
          
          // Discard any try blocks enclosed in the current method.
          while (!nearestCatch_.isNull() &&
                 (nearestCatch_->callFrame() >= callFrames_.count()))
          {
            nearestCatch_ = nearestCatch_->parent();
          }
          
          if (callFrames_.count() > 0)
          {
            // Give the result back and resume the calling method.
            CallFrame& caller = callFrames_[-1];
            instruction callInstruction = caller.method->code()[caller.ip - 1];
            ASSERT(GET_OP(callInstruction) == OP_CALL,
                   "Should be returning to a call.");
            
            store(caller, GET_B(callInstruction), result);
          }
          else
          {
            // The last method has returned, so end the fiber.
            // TODO(bob): Do we care about the result value?
            return FIBER_DONE;
          }
          break;
        }
          
        case OP_THROW:
        {
          // TODO(bob): Throw an actual value.
          if (!throwError(vm_.getBool(false)))
          {
            return FIBER_UNCAUGHT_ERROR;
          }
          break;
        }
          
        case OP_ENTER_TRY:
        {
          int offset = frame.ip + GET_A(ins);
          nearestCatch_ = new CatchFrame(nearestCatch_, callFrames_.count() - 1,
                                          offset);
          break;
        }
          
        case OP_EXIT_TRY:
        {
          nearestCatch_ = nearestCatch_->parent();
          break;
        }
          
        case OP_TEST_MATCH:
        {
          gc<Object> pass = load(frame, GET_A(ins));
          // TODO(bob): Throw NoMatchError.
          if (!pass->toBool() && !throwError(vm_.getBool(false)))
          {
            return FIBER_UNCAUGHT_ERROR;
          }
          break;
        }
      
        default:
          ASSERT(false, "Unknown opcode.");
          break;
      }
    }
    
    ASSERT(false, "Should not get here.");
    return FIBER_DONE;
  }
  
  void Fiber::reach()
  {
    // Walk the stack.
    CallFrame& frame = callFrames_[-1];
    int numRegisters = frame.stackStart + frame.method->numRegisters();
    
    // Only reach registers that are still in use. We don't shrink the stack,
    // so it may have dead registers at the end that are safe to collect.
    int i;
    for (i = 0; i < numRegisters; i++)
    {
      Memory::reach(stack_[i]);
    }

    // For the remaining registers, clear them out now. When a new call is
    // pushed onto the stack, we allocate registers for it, but we don't clear
    // them out. This means that when a collection occurs, there may be a few
    // registers on the end of the stack that are stale: they are set to
    // whatever they were on some previous call. Since a collection may have
    // occurred between now and then, and dead registers aren't reached (see
    // above), we may have bad pointers. This clears those out so we don't get
    // into that situation. We do it here instead of in call() because call()
    // needs to be as fast as possible.
    for (; i < stack_.count(); i++)
    {
      stack_[i] = gc<Object>();
    }
    
    for (int i = 0; i < callFrames_.count(); i++)
    {
      Memory::reach(callFrames_[i].method);
    }
  }
  
  void Fiber::call(gc<Method> method, int stackStart, gc<Object> arg)
  {
    // Allocate registers for the method.
    // TODO(bob): Make this a single operation on Array.
    while (stack_.count() < stackStart + method->numRegisters())
    {
      stack_.add(gc<Object>());
    }
    
    // Bind the argument in the called method.
    stack_[stackStart] = arg;
    
    callFrames_.add(CallFrame(method, stackStart));
  }

  bool Fiber::throwError(gc<Object> error)
  {
    // If there is nothing to catch it, end the fiber.
    if (nearestCatch_.isNull()) return false;
    
    // Unwind any nested callframes above the one containing the catch
    // clause.
    callFrames_.truncate(nearestCatch_->callFrame() + 1);
    
    // Jump to the catch handler.
    // TODO(bob): Insert thrown value into appropriate register.
    callFrames_[-1].ip = nearestCatch_->offset();
    
    // Discard the try block now that we are outside of it.
    nearestCatch_ = nearestCatch_->parent();
    
    return true;
  }
  
  gc<Object> Fiber::loadRegisterOrConstant(const CallFrame& frame, int index)
  {
    if (IS_CONSTANT(index))
    {
      return frame.method->getConstant(GET_CONSTANT(index));
    }
    else
    {
      return load(frame, index);
    }
  }
  
  void CatchFrame::reach()
  {
    Memory::reach(parent_);
  }
}