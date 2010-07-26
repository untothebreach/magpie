package com.stuffwithstuff.magpie;

import java.util.*;

public abstract class Parser {
  public Parser(Lexer lexer) {
    mLexer = lexer;
    mRead = new LinkedList<Token>();
    mConsumed = new LinkedList<Token>();
  }

  protected Token last(int offset) {
    return mConsumed.get(offset - 1);
  }

  protected boolean match(TokenType... types) {
    // make sure all of the types match before we start consuming
    for (int i = 0; i < types.length; i++) {
      if (!lookAhead(i).getType().equals(types[i]))
        return false;
    }

    // consume the matched tokens
    for (int i = 0; i < types.length; i++) {
      mConsumed.add(0, mRead.remove(0));
    }
    
    return true;
  }

  protected void consume(TokenType... types) {
    // make sure all of the types match before we start consuming
    for (int i = 0; i < types.length; i++) {
      if (!lookAhead(i).getType().equals(types[i]))
        throw new ParseError("Expected " + types[i] + " and found " + lookAhead(i));
    }

    // consume the matched tokens
    for (int i = 0; i < types.length; i++) {
      mConsumed.add(0, mRead.remove(0));
    }
  }

  private Token lookAhead(int distance) {
    // read in as many as needed
    while (distance >= mRead.size()) {
      mRead.add(mLexer.readToken());
    }

    // get the queued token
    return mRead.get(distance);
  }

  private final Lexer mLexer;

  private final List<Token> mRead;
  private final List<Token> mConsumed;
}
