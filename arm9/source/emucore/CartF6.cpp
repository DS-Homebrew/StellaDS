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
// Copyright (c) 1995-2005 by Bradford W. Mott
//
// See the file "license" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//
// $Id: CartF6.cxx,v 1.4 2005/02/13 19:17:02 stephena Exp $
//============================================================================

#include <assert.h>
#include "CartF6.hxx"
#include "System.hxx"
#include <iostream>

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
CartridgeF6::CartridgeF6(const uInt8* image)
{
  // Copy the ROM image into my buffer
  for(uInt32 addr = 0; addr < 16384; ++addr)
  {
    myImage[addr] = image[addr];
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
CartridgeF6::~CartridgeF6()
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const char* CartridgeF6::name() const
{
  return "CartridgeF6";
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeF6::reset()
{
  // Upon reset we switch to bank 0
  bank(0);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeF6::install(System& system)
{
  mySystem = &system;
  uInt16 shift = mySystem->pageShift();
  uInt16 mask = mySystem->pageMask();

  // Make sure the system we're being installed in has a page size that'll work
  assert((0x1000 & mask) == 0);

  // Set the page accessing methods for the hot spots
  System::PageAccess access;
  for(uInt32 i = (0x1FF6 & ~mask); i < 0x2000; i += (1 << shift))
  {
    access.directPeekBase = 0;
    access.directPokeBase = 0;
    access.device = this;
    mySystem->setPageAccess(i >> shift, access);
  }

  // Upon install we'll setup bank 0
  bank(0);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt8 CartridgeF6::peek(uInt16 address)
{
  address = address & 0x0FFF;

  // Switch banks if necessary
  switch(address)
  {
    case 0x0FF6:
      // Set the current bank to the first 4k bank
      bank(0);
      break;

    case 0x0FF7:
      // Set the current bank to the second 4k bank
      bank(1);
      break;

    case 0x0FF8:
      // Set the current bank to the third 4k bank
      bank(2);
      break;

    case 0x0FF9:
      // Set the current bank to the forth 4k bank
      bank(3);
      break;

    default:
      break;
  }

  return myImage[myCurrentBank * 4096 + address];
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeF6::poke(uInt16 address, uInt8)
{
  address = address & 0x0FFF;

  // Switch banks if necessary
  switch(address)
  {
    case 0x0FF6:
      // Set the current bank to the first 4k bank
      bank(0);
      break;

    case 0x0FF7:
      // Set the current bank to the second 4k bank
      bank(1);
      break;

    case 0x0FF8:
      // Set the current bank to the third 4k bank
      bank(2);
      break;

    case 0x0FF9:
      // Set the current bank to the forth 4k bank
      bank(3);
      break;

    default:
      break;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeF6::bank(uInt16 bank)
{ 
  // Remember what bank we're in
  myCurrentBank = bank;
  uInt16 offset = myCurrentBank * 4096;
  uInt16 shift = mySystem->pageShift();
  uInt16 mask = mySystem->pageMask();

  // Setup the page access methods for the current bank
  System::PageAccess access;
  access.device = this;
  access.directPokeBase = 0;

  // Map ROM image into the system
  for(uInt32 address = 0x1000; address < (0x1FF6U & ~mask);
      address += (1 << shift))
  {
    access.directPeekBase = &myImage[offset + (address & 0x0FFF)];
    mySystem->setPageAccess(address >> shift, access);
  }
}

