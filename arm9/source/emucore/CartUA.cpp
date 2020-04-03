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
// Copyright (c) 1995-2004 by Bradford W. Mott
//
// See the file "license" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//
// $Id: CartUA.cxx,v 1.2 2004/06/25 13:32:16 stephena Exp $
//============================================================================

#include <cassert>
#include <iostream>
#include "CartUA.hxx"
#include "System.hxx"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
CartridgeUA::CartridgeUA(const uInt8* image)
{
  // Copy the ROM image into my buffer
  for(uInt32 addr = 0; addr < 8192; ++addr)
  {
    myImage[addr] = image[addr];
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
CartridgeUA::~CartridgeUA()
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const char* CartridgeUA::name() const
{
  return "CartridgeUA";
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeUA::reset()
{
  // Upon reset we switch to bank 0
  bank(0);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeUA::install(System& system)
{
  mySystem = &system;
  uInt16 shift = mySystem->pageShift();
  uInt16 mask = mySystem->pageMask();

  // Make sure the system we're being installed in has a page size that'll work
  assert((0x1000 & mask) == 0);

  // Get the page accessing methods for the hot spots since they overlap
  // areas within the TIA we'll need to forward requests to the TIA
  myHotSpotPageAccess = mySystem->getPageAccess(0x0220 >> shift);

  // Set the page accessing methods for the hot spots
  System::PageAccess access;
  access.directPeekBase = 0;
  access.directPokeBase = 0;
  access.device = this;
  mySystem->setPageAccess(0x0220 >> shift, access);
  mySystem->setPageAccess(0x0240 >> shift, access);

  // Install pages for bank 0
  bank(0);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt8 CartridgeUA::peek(uInt16 address)
{
  address = address & 0x1FFF;

  // Switch banks if necessary
  switch(address)
  {
    case 0x0220:
      // Set the current bank to the lower 4k bank
      bank(0);
      break;

    case 0x0240:
      // Set the current bank to the upper 4k bank
      bank(1);
      break;

    default:
      break;
  }

  if(!(address & 0x1000))
  {
    return myHotSpotPageAccess.device->peek(address);
  }
  else
  {
    return 0;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeUA::poke(uInt16 address, uInt8 value)
{
  address = address & 0x1FFF;

  // Switch banks if necessary
  switch(address)
  {
    case 0x0220:
      // Set the current bank to the lower 4k bank
      bank(0);
      break;

    case 0x0240:
      // Set the current bank to the upper 4k bank
      bank(1);
      break;

    default:
      break;
  }

  if(!(address & 0x1000))
  {
    myHotSpotPageAccess.device->poke(address, value);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeUA::bank(uInt16 bank)
{ 
  // Remember what bank we're in
  myCurrentBank = bank;
  uInt16 offset = myCurrentBank * 4096;
  uInt16 shift = mySystem->pageShift();
//  uInt16 mask = mySystem->pageMask();

  // Setup the page access methods for the current bank
  System::PageAccess access;
  access.device = this;
  access.directPokeBase = 0;

  // Map ROM image into the system
  for(uInt32 address = 0x1000; address < 0x2000; address += (1 << shift))
  {
    access.directPeekBase = &myImage[offset + (address & 0x0FFF)];
    mySystem->setPageAccess(address >> shift, access);
  }
}
