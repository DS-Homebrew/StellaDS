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
// Copyright (c) 1995-2003 by Bradford W. Mott
//
// See the file "license" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//
// $Id: Cart.hxx,v 1.2 2003/02/17 04:59:54 bwmott Exp $
//============================================================================

#ifndef CARTRIDGE_HXX
#define CARTRIDGE_HXX

class Cartridge;
class System;

#include "bspf.hxx"
#include "Device.hxx"

/**
  A cartridge is a device which contains the machine code for a 
  game and handles any bankswitching performed by the cartridge.
 
  @author  Bradford W. Mott
  @version $Id: Cart.hxx,v 1.2 2003/02/17 04:59:54 bwmott Exp $
*/
class Cartridge : public Device
{
  public:
    /**
      Create a new cartridge object allocated on the heap.  The
      type of cartridge created depends on the properties object.

      @param image A pointer to the ROM image
      @param size The size of the ROM image 
      @param properties The properties associated with the game
      @return Pointer to the new cartridge object allocated on the heap
    */
    static Cartridge* create(const uInt8* image, uInt32 size);

  public:
    /**
      Create a new cartridge
    */
    Cartridge();
 
    /**
      Destructor
    */
    virtual ~Cartridge();

  private:
    /**
      Try to auto-detect the bankswitching type of the cartridge

      @param image A pointer to the ROM image
      @param size The size of the ROM image 
      @return The "best guess" for the cartridge type
    */
    static string autodetectType(const uInt8* image, uInt32 size);

    /**
      Utility method used by isProbably3F and isProbably3E
    */
    static int searchForBytes(const uInt8* image, uInt32 size, uInt8 byte1, uInt8 byte2);

    /**
      Returns true if the image is probably a SuperChip (256 bytes RAM)
    */
    static bool isProbablySC(const uInt8* image, uInt32 size);

    /**
      Returns true if the image is probably a 3F bankswitching cartridge
    */
    static bool isProbably3F(const uInt8* image, uInt32 size);

    /**
      Returns true if the image is probably a 3E bankswitching cartridge
    */
    static bool isProbably3E(const uInt8* image, uInt32 size);

    /**
      Returns true if the image is probably a E0 bankswitching cartridge
    */
    static bool isProbablyE0(const uInt8* image, uInt32 size);

    /**
      Returns true if the image is probably a E7 bankswitching cartridge
    */
    static bool isProbablyE7(const uInt8* image, uInt32 size);

  private:
    // Copy constructor isn't supported by cartridges so make it private
    Cartridge(const Cartridge&);

    // Assignment operator isn't supported by cartridges so make it private
    Cartridge& operator = (const Cartridge&);
};
#endif

