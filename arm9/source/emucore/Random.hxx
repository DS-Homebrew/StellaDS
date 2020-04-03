//============================================================================
//
//   SSSS    tt          lll  lll
//  SS  SS   tt           ll   ll
//  SS     tttttt  eeee   ll   ll   aaaa
//   SSSS    tt   ee  ee  ll   ll      aa
//      SS   tt   eeeeee  ll   ll   aaaaa  --  "An Atari 2600 VCS Emulator"
//  SS  SS   tt   ee      ll   ll  aa  aa
//   SSSS     ttt  eeeee llll llll  aaaaa
//
// Copyright (c) 1995-1998 by Bradford W. Mott
//
// See the file "license" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//
// $Id: Random.hxx,v 1.1.1.1 2001/12/27 19:54:23 bwmott Exp $
//============================================================================

#ifndef RANDOM_HXX
#define RANDOM_HXX

#include "bspf.hxx"

/**
  This is a quick-and-dirty random number generator.  It is based on 
  information in Chapter 7 of "Numerical Recipes in C".  It's a simple 
  linear congruential generator.

  @author  Bradford W. Mott
  @version $Id: Random.hxx,v 1.1.1.1 2001/12/27 19:54:23 bwmott Exp $
*/
class Random
{
  public:
    /**
      Class method which allows you to set the seed that'll be used
      for created new instances of this class

      @param value The value to seed the random number generator with
    */
    static void seed(uInt32 value);

  public:
    /**
      Create a new random number generator
    */
    Random();
    
  public:
    /**
      Answer the next random number from the random number generator

      @return A random number
    */
    uInt32 next();

  private:
    // Indicates the next random number
    uInt32 myValue;

  private:
    // Seed to use for creating new random number generators
    static uInt32 ourSeed;

    // Indicates if seed has been set or not
    static bool ourSeeded;
};
#endif

