package com.stuffwithstuff.magpie;

/**
 * This defines identifiers that are defined or used within Magpie but also
 * referenced directly by the Java code.
 */
public final class Identifiers {
  public static final String TYPE = "type";
  public static final String EQUALS = "==";
  public static final String NEW_TYPE = "newType";
  public static final String CALL = "call";
  public static final String OR = "|";
  public static final String GET_METHOD_TYPE = "getMethodType";
  public static final String CAN_ASSIGN_FROM = "canAssignFrom";
  public static final String TO_STRING = "toString";
  public static final String NEW = "new";
  public static final String IS_TRUE = "true?";
  public static final String ITERATE = "iterate";
  public static final String NEXT = "next";
  public static final String CURRENT = "current";
  public static final String UNSAFE_REMOVE_NOTHING = "unsafeRemoveNothing";
  
  public static String makeSetter(String baseName) {
    return baseName + "=";
  }
  
  private Identifiers() {}
}
