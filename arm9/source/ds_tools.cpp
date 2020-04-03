#include <nds.h>
#include <nds/fifomessages.h>

#include<stdio.h>

#include <fat.h>
#include <dirent.h>
#include <unistd.h>

#include "ds_tools.h"

#include "bgBottom.h"
#include "bgTop.h"
#include "bgFileSel.h"

#include "clickNoQuit_wav.h"
#include "clickQuit_wav.h"

#include "Console.hxx"
#include "Mediasrc.hxx"
#include "Sound.hxx"
#include "SoundSDL.hxx"
#include "Event.hxx"
#include "StellaEvent.hxx"
#include "EventHandler.hxx"


#define A26_VID_WIDTH  160  
#define A26_VID_HEIGHT 210
#define A26_VID_XOFS   0
#define A26_VID_YOFS   0

FICA2600 vcsromlist[1024];  
unsigned int countvcs=0, ucFicAct=0;

Console* theConsole = (Console*) NULL;
Sound* theSDLSnd = (Sound*) NULL;
uInt8* filebuffer = 0;

int bg0, bg0b,bg1b;
unsigned int etatEmu;
bool fpsDisplay = false;
  
#define SOUND_SIZE (2048)
static uInt8 sound_buffer[SOUND_SIZE];
uInt8* psound_buffer;

// --------------------------------------------------------------------------------------
// Color fading effect
void FadeToColor(unsigned char ucSens, unsigned short ucBG, unsigned char ucScr, unsigned char valEnd, unsigned char uWait) {
  unsigned short ucFade;
  unsigned char ucBcl;

  // Fade-out vers le noir
  if (ucScr & 0x01) REG_BLDCNT=ucBG;
  if (ucScr & 0x02) REG_BLDCNT_SUB=ucBG;
  if (ucSens == 1) {
    for(ucFade=0;ucFade<valEnd;ucFade++) {
      if (ucScr & 0x01) REG_BLDY=ucFade;
      if (ucScr & 0x02) REG_BLDY_SUB=ucFade;
      for (ucBcl=0;ucBcl<uWait;ucBcl++) {
        swiWaitForVBlank();
      }
    }
  }
  else {
    for(ucFade=16;ucFade>valEnd;ucFade--) {
      if (ucScr & 0x01) REG_BLDY=ucFade;
      if (ucScr & 0x02) REG_BLDY_SUB=ucFade;
      for (ucBcl=0;ucBcl<uWait;ucBcl++) {
        swiWaitForVBlank();
      }
    }
  }
}

// --------------------------------------------------------------------------------------
volatile u32 emuFps;
volatile u32 emuActFrames;
volatile u16 g_framePending = 0;

void vblankIntr() {
	if (g_framePending == 2) {
		g_framePending = 0;
		emuActFrames++;
	}

	//antialias tile layer
	static u16 sTime = 0;

	sTime++;
	if(sTime >= 60) {
		sTime = 0;
		emuFps = emuActFrames;
		emuActFrames = 0;
	}
}

static void vcountIntr() {
	if (g_framePending == 1 && REG_VCOUNT < 192) {
		g_framePending = 2;
	}
}

void dsInitScreenMain(void) {
  // Init vbl and hbl func
	SetYtrigger(190); //trigger 2 lines before vsync
	irqSet(IRQ_VBLANK, vblankIntr);
	irqSet(IRQ_VCOUNT, vcountIntr);
  irqEnable(IRQ_VBLANK | IRQ_VCOUNT);
}

void dsInitTimer(void) {

  TIMER0_DATA=0;
	TIMER0_CR=TIMER_ENABLE|TIMER_DIV_1024; 
}

void dsInitPalette(void) {
  // Init DS Specifica palette
  const uInt32* gamePalette = theConsole->myMediaSource->palette();
  for(uInt32 i = 0; i < 256; i++)	{
		uInt8 r, g, b;
 
		r = (uInt8) ((gamePalette[i] & 0x00ff0000) >> 19);
		g = (uInt8) ((gamePalette[i] & 0x0000ff00) >> 11);
		b = (uInt8) ((gamePalette[i] & 0x000000ff) >> 3);
 
		BG_PALETTE[i]=RGB15(r,g,b);
	}
}

void dsShowScreenEmu(void) {
	videoSetMode(MODE_5_2D);
  vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
  bg0 = bgInit(3, BgType_Bmp8, BgSize_B8_256x256, 0,0);

  REG_BG3PA = ((A26_VID_WIDTH / 256) << 8) | (A26_VID_WIDTH % 256) ; 
  REG_BG3PB = 0; REG_BG3PC = 0;
	REG_BG3PD = ((A26_VID_HEIGHT / 192) << 8) | ((A26_VID_HEIGHT % 192) ) ;  
  REG_BG3X = A26_VID_XOFS<<8;
  REG_BG3Y = A26_VID_YOFS<<8;
}

void dsShowScreenMain(void) {
  // Init BG mode for 16 bits colors
  videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE );
  videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE);
  vramSetBankA(VRAM_A_MAIN_BG); vramSetBankC(VRAM_C_SUB_BG);
  bg0 = bgInit(0, BgType_Text8bpp, BgSize_T_256x256, 31,0);
  bg0b = bgInitSub(0, BgType_Text8bpp, BgSize_T_256x256, 31,0);
  bg1b = bgInitSub(1, BgType_Text8bpp, BgSize_T_256x256, 30,0);
  bgSetPriority(bg0b,1);bgSetPriority(bg1b,0);

  decompress(bgTopTiles, bgGetGfxPtr(bg0), LZ77Vram);
  decompress(bgTopMap, (void*) bgGetMapPtr(bg0), LZ77Vram);
  dmaCopy((void *) bgTopPal,(u16*) BG_PALETTE,256*2);

  decompress(bgBottomTiles, bgGetGfxPtr(bg0b), LZ77Vram);
  decompress(bgBottomMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
  dmaCopy((void *) bgBottomPal,(u16*) BG_PALETTE_SUB,256*2);
  unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);

  REG_BLDCNT=0; REG_BLDCNT_SUB=0; REG_BLDY=0; REG_BLDY_SUB=0;
      
  swiWaitForVBlank();
}

void dsFreeEmu(void) {
  // Stop timer of sound
  TIMER2_CR=0; irqDisable(IRQ_TIMER2); 

  // Free buffer if needed 
	if (filebuffer != 0)
		free(filebuffer);
  if (theConsole) 
    delete theConsole;
  if (theSDLSnd) 
    delete theSDLSnd;
}

void VsoundHandler(void) {
  psound_buffer++;
  if (psound_buffer>=&sound_buffer[SOUND_SIZE]) psound_buffer=sound_buffer;
  theSDLSnd->callback(psound_buffer, 1);
}

void dsLoadGame(char *filename) {
  unsigned int buffer_size=0;
  
  // Free buffer if needed
  TIMER2_CR=0; irqDisable(IRQ_TIMER2); 
	if (filebuffer != 0)
		free(filebuffer);

  if (theConsole) 
    delete theConsole;
  if (theSDLSnd) 
    delete theSDLSnd;
  
  theSDLSnd = new SoundSDL(512);
  theSDLSnd->setVolume(100);

  // Load the file
  FILE *romfile = fopen(filename, "r");
  fseek(romfile, 0, SEEK_END);
  buffer_size = ftell(romfile);
  rewind(romfile);
  filebuffer = (unsigned char *) malloc(buffer_size);
  fread(filebuffer, buffer_size, 1, romfile);
  fclose(romfile);

  // Init the emulation
  theConsole = new Console((const uInt8*) filebuffer, buffer_size, "noname", *theSDLSnd);
 	dsInitPalette();

  psound_buffer=sound_buffer;
  TIMER2_DATA = TIMER_FREQ(22050);                        
	TIMER2_CR = TIMER_DIV_1 | TIMER_IRQ_REQ | TIMER_ENABLE;	     
	irqSet(IRQ_TIMER2, VsoundHandler);                           
	irqEnable(IRQ_TIMER2);  
}

unsigned int dsReadPad(void) {
	unsigned int keys_pressed, ret_keys_pressed;

	do {
		keys_pressed = keysCurrent();
	} while ((keys_pressed & (KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_L | KEY_R))==0);
	ret_keys_pressed = keys_pressed;

	do {
		keys_pressed = keysCurrent();
	} while ((keys_pressed & (KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_L | KEY_R))!=0);

	return ret_keys_pressed;
}

bool dsWaitOnQuit(void) {
  bool bRet=false, bDone=false;
  unsigned int keys_pressed;
  unsigned int posdeb=0;
  char szName[32];
  
  decompress(bgFileSelTiles, bgGetGfxPtr(bg0b), LZ77Vram);
  decompress(bgFileSelMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
  dmaCopy((void *) bgFileSelPal,(u16*) BG_PALETTE_SUB,256*2);
  unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
  
  strcpy(szName,"Quit StellaDS ?");
  dsPrintValue(16-strlen(szName)/2,2,0,szName);
  sprintf(szName,"%s","A TO CONFIRM, B TO GO BACK");
  dsPrintValue(16-strlen(szName)/2,23,0,szName);
      
  while(!bDone) {
    strcpy(szName,"          YES          ");
    dsPrintValue(5,10+0,(posdeb == 0 ? 1 :  0),szName);
    strcpy(szName,"          NO           ");
    dsPrintValue(5,14+1,(posdeb == 2 ? 1 :  0),szName);
    swiWaitForVBlank();
  
    // Check pad
    keys_pressed=dsReadPad();
    if (keys_pressed & KEY_UP) {
      if (posdeb) posdeb-=2;
    }
    if (keys_pressed & KEY_DOWN) {
      if (posdeb<1) posdeb+=2;
    }
    if (keys_pressed & KEY_A) {
      bRet = (posdeb ? false : true);
      bDone = true;
    }
    if (keys_pressed & KEY_B) {
      bDone = true;
    }
  }

  decompress(bgBottomTiles, bgGetGfxPtr(bg0b), LZ77Vram);
  decompress(bgBottomMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
  dmaCopy((void *) bgBottomPal,(u16*) BG_PALETTE_SUB,256*2);
  dmaVal = *(bgGetMapPtr(bg1b) +31*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);  

  return bRet;
}

void dsDisplayFiles(unsigned int NoDebGame,u32 ucSel) {
  unsigned int ucBcl,ucGame;
  u8 maxLen;
  char szName[256];
  
  // Display all games if possible
  unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) (bgGetMapPtr(bg1b)),32*24*2);
  sprintf(szName,"%04d/%04d GAMES",1+ucSel+NoDebGame,countvcs);
  dsPrintValue(16-strlen(szName)/2,2,0,szName);
  dsPrintValue(31,5,0,(char *) (NoDebGame>0 ? "<" : " "));
  dsPrintValue(31,22,0,(char *) (NoDebGame+14<countvcs ? ">" : " "));
  sprintf(szName,"%s","A TO SELECT A GAME, B TO GO BACK");
  dsPrintValue(16-strlen(szName)/2,23,0,szName);
  for (ucBcl=0;ucBcl<17; ucBcl++) {
    ucGame= ucBcl+NoDebGame;
    if (ucGame < countvcs) {
      maxLen=strlen(vcsromlist[ucGame].filename);
      strcpy(szName,vcsromlist[ucGame].filename);
      if (maxLen>29) szName[29]='\0';
      if (vcsromlist[ucGame].directory) {
        sprintf(szName,"[%s]",vcsromlist[ucGame].filename);
        sprintf(szName,"%-29s",szName);
        dsPrintValue(0,5+ucBcl,(ucSel == ucBcl ? 1 :  0),szName);
      }
      else {
        sprintf(szName,"%-29s",strupr(szName));
        dsPrintValue(1,5+ucBcl,(ucSel == ucBcl ? 1 : 0),szName);
      }
    }
  }
}

void dsDisplayButton(unsigned char button) {
  unsigned short *ptrBg1 = bgGetMapPtr(bg0b) +32*26;
  unsigned short *ptrBg0 = bgGetMapPtr(bg0b);
  unsigned int i;
  
  switch (button) {
    case 0: // ON/OFF
      for (i=0;i<4;i++) {
        *(ptrBg0+(4+i)*32+5) = *(ptrBg1+(0+i)*32+0);
        *(ptrBg0+(4+i)*32+6) = *(ptrBg1+(0+i)*32+1);
      } 
      break;
    case 1: // ON/OFF
      for (i=0;i<4;i++) {
        *(ptrBg0+(4+i)*32+5) = *(ptrBg1+(0+i)*32+2);
        *(ptrBg0+(4+i)*32+6) = *(ptrBg1+(0+i)*32+3);
      } 
      break;
    case 2: // BW/Color
      for (i=0;i<4;i++) {
        *(ptrBg0+(4+i)*32+9) = *(ptrBg1+(0+i)*32+4);
        *(ptrBg0+(4+i)*32+10) = *(ptrBg1+(0+i)*32+5);
      } 
      break;
    case 3: // BW/Color
      for (i=0;i<4;i++) {
        *(ptrBg0+(4+i)*32+9) = *(ptrBg1+(0+i)*32+6);
        *(ptrBg0+(4+i)*32+10) = *(ptrBg1+(0+i)*32+7);
      } 
      break;
    case 4: // Select
      for (i=0;i<4;i++) {
        *(ptrBg0+(4+i)*32+21) = *(ptrBg1+(0+i)*32+8);
        *(ptrBg0+(4+i)*32+22) = *(ptrBg1+(0+i)*32+9);
      } 
      break;
    case 5: // Select
      for (i=0;i<4;i++) {
        *(ptrBg0+(4+i)*32+21) = *(ptrBg1+(0+i)*32+10);
        *(ptrBg0+(4+i)*32+22) = *(ptrBg1+(0+i)*32+11);
      } 
      break;
    case 6: // Reset
      for (i=0;i<4;i++) {
        *(ptrBg0+(4+i)*32+27) = *(ptrBg1+(0+i)*32+12);
        *(ptrBg0+(4+i)*32+28) = *(ptrBg1+(0+i)*32+13);
      } 
      break;
    case 7: // Reset
      for (i=0;i<4;i++) {
        *(ptrBg0+(4+i)*32+27) = *(ptrBg1+(0+i)*32+14);
        *(ptrBg0+(4+i)*32+28) = *(ptrBg1+(0+i)*32+15);
      } 
      break;
    case 8: // PAL<> NTSC
      for (i=0;i<4;i++) {
        *(ptrBg0+(22)*32+(27+i)) = *(ptrBg1+(0)*32+(16+i));
        *(ptrBg0+(23)*32+(27+i)) = *(ptrBg1+(1)*32+(16+i));
      } 
      break;
    case 9: // PAL<> NTSC
      for (i=0;i<4;i++) {
        *(ptrBg0+(22)*32+(27+i)) = *(ptrBg1+(2)*32+(16+i));
        *(ptrBg0+(23)*32+(27+i)) = *(ptrBg1+(3)*32+(16+i));
      } 
      break;
  }
}

unsigned int dsWaitForRom(void) {
  bool bDone=false, bRet=false;
  u32 ucHaut=0x00, ucBas=0x00,ucSHaut=0x00, ucSBas=0x00,romSelected= 0, firstRomDisplay=0,nbRomPerPage, uNbRSPage, uLenFic=0,ucFlip=0, ucFlop=0;
  char szName[64];

  decompress(bgFileSelTiles, bgGetGfxPtr(bg0b), LZ77Vram);
  decompress(bgFileSelMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
  dmaCopy((void *) bgFileSelPal,(u16*) BG_PALETTE_SUB,256*2);
  unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
  
  nbRomPerPage = (countvcs>=17 ? 17 : countvcs);
  uNbRSPage = (countvcs>=5 ? 5 : countvcs);
  if (ucFicAct>countvcs-nbRomPerPage) {
    firstRomDisplay=countvcs-nbRomPerPage;
    romSelected=ucFicAct-countvcs+nbRomPerPage;
  }
  else {
    firstRomDisplay=ucFicAct;
    romSelected=0;
  }
  dsDisplayFiles(firstRomDisplay,romSelected);
  while (!bDone) {
    if (keysCurrent() & KEY_UP) {
      if (!ucHaut) {
        ucFicAct = (ucFicAct>0 ? ucFicAct-1 : countvcs-1);
        if (romSelected>uNbRSPage) { romSelected -= 1; }
        else {
          if (firstRomDisplay>0) { firstRomDisplay -= 1; }
          else {
            if (romSelected>0) { romSelected -= 1; }
            else {
              firstRomDisplay=countvcs-nbRomPerPage;
              romSelected=nbRomPerPage-1;
            }
          }
        }
        ucHaut=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else {
        ucHaut++;
        if (ucHaut>10) ucHaut=0;
      } 
    }
    else {
      ucHaut = 0;
    }  
    if (keysCurrent() & KEY_DOWN) {
      if (!ucBas) {
        ucFicAct = (ucFicAct< countvcs-1 ? ucFicAct+1 : 0);
        if (romSelected<uNbRSPage-1) { romSelected += 1; }
        else {
          if (firstRomDisplay<countvcs-nbRomPerPage) { firstRomDisplay += 1; }
          else {
            if (romSelected<nbRomPerPage-1) { romSelected += 1; }
            else {
              firstRomDisplay=0;
              romSelected=0;
            }
          }
        }
        ucBas=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else {
        ucBas++;
        if (ucBas>10) ucBas=0;
      } 
    }
    else {
      ucBas = 0;
    }  
    if (keysCurrent() & KEY_R) {
      if (!ucSBas) {
        ucFicAct = (ucFicAct< countvcs-nbRomPerPage ? ucFicAct+nbRomPerPage : countvcs-nbRomPerPage);
        if (firstRomDisplay<countvcs-nbRomPerPage) { firstRomDisplay += nbRomPerPage; }
        else { firstRomDisplay = countvcs-nbRomPerPage; }
        if (ucFicAct == countvcs-nbRomPerPage) romSelected = 0;
        ucSBas=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else {
        ucSBas++;
        if (ucSBas>10) ucSBas=0;
      } 
    }
    else {
      ucSBas = 0;
    }  
    if (keysCurrent() & KEY_L) {
      if (!ucSHaut) {
        ucFicAct = (ucFicAct> nbRomPerPage ? ucFicAct-nbRomPerPage : 0);
        if (firstRomDisplay>nbRomPerPage) { firstRomDisplay -= nbRomPerPage; }
        else { firstRomDisplay = 0; }
        if (ucFicAct == 0) romSelected = 0;
        ucSHaut=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else {
        ucSHaut++;
        if (ucSHaut>10) ucSHaut=0;
      } 
    }
    else {
      ucSHaut = 0;
    }  
		if ( keysCurrent() & KEY_B ) {
      bDone=true;
      while (keysCurrent() & KEY_B);
    }

    if (keysCurrent() & KEY_A) {
      if (!vcsromlist[ucFicAct].directory) {
        bRet=true;
        bDone=true;
      }
      else {
        chdir(vcsromlist[ucFicAct].filename);
        vcsFindFiles();
        ucFicAct = 0;
        nbRomPerPage = (countvcs>=16 ? 16 : countvcs);
        uNbRSPage = (countvcs>=5 ? 5 : countvcs);
        if (ucFicAct>countvcs-nbRomPerPage) {
          firstRomDisplay=countvcs-nbRomPerPage;
          romSelected=ucFicAct-countvcs+nbRomPerPage;
        }
        else {
          firstRomDisplay=ucFicAct;
          romSelected=0;
        }
        dsDisplayFiles(firstRomDisplay,romSelected);
        while (keysCurrent() & KEY_A);
      }
    }
    // Scroll la selection courante
    if (strlen(vcsromlist[ucFicAct].filename) > 29) {
      ucFlip++;
      if (ucFlip >= 8) {
        ucFlip = 0;
        uLenFic++;
        if ((uLenFic+29)>strlen(vcsromlist[ucFicAct].filename)) {
          ucFlop++;
          if (ucFlop >= 8) {
            uLenFic=0;
            ucFlop = 0;
          }
          else
            uLenFic--;
        }
        strncpy(szName,vcsromlist[ucFicAct].filename+uLenFic,29);
        szName[29] = '\0';
        dsPrintValue(1,5+romSelected,1,szName);
      }
    }
    swiWaitForVBlank();
  }
  
  decompress(bgBottomTiles, bgGetGfxPtr(bg0b), LZ77Vram);
  decompress(bgBottomMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
  dmaCopy((void *) bgBottomPal,(u16*) BG_PALETTE_SUB,256*2);
  dmaVal = *(bgGetMapPtr(bg1b) +31*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
  
  return bRet;
}

unsigned int dsWaitOnMenu(unsigned int actState) {
  unsigned int uState=STELLADS_PLAYINIT;
  unsigned int keys_pressed;
  bool bDone=false, romSel;
  int iTx,iTy;
  
  while (!bDone) {
    // wait for stylus
    keys_pressed = keysCurrent();
    if (keys_pressed & KEY_TOUCH) {
      touchPosition touch;
      touchRead(&touch);
      iTx = touch.px;
      iTy = touch.py;
      if ((iTx>23) && (iTx<39) && (iTy>35) && (iTy<65)) { // 24,36  -> 38,64   quit
        dsDisplayButton(1);
        soundPlaySample(clickQuit_wav, SoundFormat_8Bit, clickQuit_wav_size, 22050, 127, 64, false, 0);
        bDone=dsWaitOnQuit();
        if (bDone) uState=STELLADS_QUITSTDS;
        else dsDisplayButton(0);
      }
      if ((iTx>47) && (iTx<209) && (iTy>99) && (iTy<133)) {     // 48,100 -> 208,132 cartridge slot
        bDone=true; 
        // Find files in current directory and show it 
        vcsFindFiles();
        romSel=dsWaitForRom();
        if (romSel) { uState=STELLADS_PLAYINIT; dsLoadGame(vcsromlist[ucFicAct].filename); }
        else { uState=actState; }
      }
    }
    swiWaitForVBlank();
  }
  
  return uState;
}

void dsPrintValue(int x, int y, unsigned int isSelect, char *pchStr) {
  u16 *pusEcran,*pusMap;
  u16 usCharac;
  char szTexte[128],*pTrTxt=szTexte;
  
  strcpy(szTexte,pchStr);
  strupr(szTexte);
  pusEcran=(u16*) (bgGetMapPtr(bg1b))+x+(y<<5);
  pusMap=(u16*) (bgGetMapPtr(bg0b)+(2*isSelect+24)*32);
  //keybBAS_map[29]
  while((*pTrTxt)!='\0' ) {
    usCharac=0x0000;
    if ((*pTrTxt) == '|')
      usCharac=*(pusMap);
    else if (((*pTrTxt)<' ') || ((*pTrTxt)>'_'))
      usCharac=*(pusMap);
    else if((*pTrTxt)<'@')
      usCharac=*(pusMap+(*pTrTxt)-' ');
    else
      usCharac=*(pusMap+32+(*pTrTxt)-'@');
    *pusEcran++=usCharac;
    pTrTxt++;
  }
}

//---------------------------------------------------------------------------------
void dsInstallSoundEmuFIFO(void) {
	FifoMessage msg;
  msg.SoundPlay.data = &sound_buffer;
  msg.SoundPlay.freq = 22050;
	msg.SoundPlay.volume = 127;
	msg.SoundPlay.pan = 64;
	msg.SoundPlay.loop = 1;
	msg.SoundPlay.format = ((1)<<4) | SoundFormat_8Bit;
  msg.SoundPlay.loopPoint = 0;
  msg.SoundPlay.dataSize = SOUND_SIZE >> 2;
  msg.type = EMUARM7_PLAY_SND;
  fifoSendDatamsg(FIFO_USER_01, sizeof(msg), (u8*)&msg);
}

#define WAITVBL swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank();

ITCM_CODE void dsMainLoop(void) {
  u32 CurrentTimeInMs=0,PreviousTimeInMs=0,TimeElapsed;
  char fpsbuf[32];
  unsigned int keys_pressed,keys_touch=0, console_color=1,console_palette=1, romSel;
  int iTx,iTy;
  
  while(etatEmu != STELLADS_QUITSTDS) {
    switch (etatEmu) {
      case STELLADS_MENUINIT:
        dsShowScreenMain();
        etatEmu = STELLADS_MENUSHOW;
        break;
        
      case STELLADS_MENUSHOW:
        etatEmu = dsWaitOnMenu(STELLADS_MENUSHOW);
        break;
        
      case STELLADS_PLAYINIT:
        dsShowScreenEmu();
        etatEmu = STELLADS_PLAYGAME;
        break;
        
      case STELLADS_PLAYGAME:
        // 65535 = 1 frame
        // 1 frame = 1/50 ou 1/60 (0.02 ou 0.016 
        // 656 -> 50 fps et 546 -> 60 fps
        CurrentTimeInMs=ds_GetTicks();
        TimeElapsed=(CurrentTimeInMs-PreviousTimeInMs);
        while(TimeElapsed<546) {
          CurrentTimeInMs = ds_GetTicks();
          TimeElapsed=(CurrentTimeInMs-PreviousTimeInMs);
        }
        PreviousTimeInMs=CurrentTimeInMs;
				
        // Wait for keys
		scanKeys();
        keys_pressed = keysCurrent();
       	theConsole->eventHandler().sendKeyEvent(StellaEvent::KCODE_SPACE, keys_pressed & (KEY_A));
        theConsole->eventHandler().sendKeyEvent(StellaEvent::KCODE_UP,    keys_pressed & (KEY_UP));
	      theConsole->eventHandler().sendKeyEvent(StellaEvent::KCODE_DOWN,  keys_pressed & (KEY_DOWN));
	      theConsole->eventHandler().sendKeyEvent(StellaEvent::KCODE_LEFT,  keys_pressed & (KEY_LEFT));
	      theConsole->eventHandler().sendKeyEvent(StellaEvent::KCODE_RIGHT, keys_pressed & (KEY_RIGHT));
        theConsole->eventHandler().sendKeyEvent(StellaEvent::KCODE_F1,    keys_pressed & (KEY_SELECT));
        theConsole->eventHandler().sendKeyEvent(StellaEvent::KCODE_F2, 0);
        theConsole->eventHandler().sendKeyEvent(StellaEvent::KCODE_F3, 0);
        theConsole->eventHandler().sendKeyEvent(StellaEvent::KCODE_F4, 0);

        if (fpsDisplay) { siprintf(fpsbuf,"%03d",(int)emuFps); dsPrintValue(0,0,0, fpsbuf); }
		else { siprintf(fpsbuf,"   "); dsPrintValue(0,0,0, fpsbuf); }

        if (keysDown() & KEY_START) { fpsDisplay = !fpsDisplay; }
        if (keys_pressed & KEY_TOUCH) {
          if (!keys_touch) {
            touchPosition touch;
            keys_touch=1;
            touchRead(&touch);
            iTx = touch.px;
            iTy = touch.py;
            if ((iTx>23) && (iTx<39) && (iTy>35) && (iTy<65)) { // 24,36  -> 38,64   quit
              dsDisplayButton(1);
              soundPlaySample(clickQuit_wav, SoundFormat_8Bit, clickQuit_wav_size, 22050, 127, 64, false, 0);
              if (dsWaitOnQuit()) etatEmu=STELLADS_QUITSTDS;
              else dsDisplayButton(0);
            }
            else if ((iTx>71) && (iTx<87) && (iTy>35) && (iTy<65)) { // 72,36  ->86,64   tv type
              soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
              console_color=1-console_color;
              theConsole->eventHandler().sendKeyEvent(console_color ? StellaEvent::KCODE_F3 : StellaEvent::KCODE_F4, 1);
              dsDisplayButton(3-console_color);
            }
            else if ((iTx>170) && (iTx<186) && (iTy>35) && (iTy<65)) { // 171,36  ->185,64   game select
              dsDisplayButton(5);
              soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
              theConsole->eventHandler().sendKeyEvent(StellaEvent::KCODE_F1, 1);
              WAITVBL; dsDisplayButton(4);
            }
            else if ((iTx>218) && (iTx<234) && (iTy>35) && (iTy<65)) { // 219,36  ->233,64   game reset
              dsDisplayButton(7);
              soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
              theConsole->eventHandler().sendKeyEvent(StellaEvent::KCODE_F2, 1);
              WAITVBL; dsDisplayButton(6);
            }
            else if ((iTx>47) && (iTx<209) && (iTy>99) && (iTy<133)) {     // 48,100 -> 208,132 cartridge slot
              // Find files in current directory and show it 
              irqDisable(IRQ_TIMER2); fifoSendValue32(FIFO_USER_01,(1<<16) | (0) | SOUND_SET_VOLUME);
              vcsFindFiles();
              romSel=dsWaitForRom();
              if (romSel) { etatEmu=STELLADS_PLAYINIT; dsLoadGame(vcsromlist[ucFicAct].filename); }
              else { irqEnable(IRQ_TIMER2); }
              fifoSendValue32(FIFO_USER_01,(1<<16) | (127) | SOUND_SET_VOLUME);
            }
            else if ((iTx>215) && (iTx<249) && (iTy>175) && (iTy<192)) {     // 48,100 -> 208,132 palette PAL <> NTSC
              // Toggle palette
              soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
              console_palette=1-console_palette;
              theConsole->togglePalette();
              dsInitPalette();
              dsDisplayButton(9-console_palette);
            }
          }
        }
        else
          keys_touch=0;
  
        // Update frame
		    theConsole->update();
        g_framePending = 1;  
        break;
    }
	}
}
  
//----------------------------------------------------------------------------------
// Find files (a26 / bin) available
int a26Filescmp (const void *c1, const void *c2) {
  FICA2600 *p1 = (FICA2600 *) c1;
  FICA2600 *p2 = (FICA2600 *) c2;
  
  return strcmp (p1->filename, p2->filename);
}

void vcsFindFiles(void) {
	struct stat statbuf;
  DIR *pdir;
  struct dirent *pent;
  char filenametmp[255];
  
  countvcs = 0;
  
  pdir = opendir(".");

  if (pdir) {

    while (((pent=readdir(pdir))!=NULL)) {
      stat(pent->d_name,&statbuf);

      strcpy(filenametmp,pent->d_name);
      if(S_ISDIR(statbuf.st_mode)) {
        if (!( (filenametmp[0] == '.') && (strlen(filenametmp) == 1))) {
          vcsromlist[countvcs].directory = true;
          strcpy(vcsromlist[countvcs].filename,filenametmp);
          countvcs++;
        }
      }
      else {
        if (strlen(filenametmp)>4) {
          if ( (strcasecmp(strrchr(filenametmp, '.'), ".a26") == 0) )  {
            vcsromlist[countvcs].directory = false;
            strcpy(vcsromlist[countvcs].filename,filenametmp);
            countvcs++;
          }
          if ( (strcasecmp(strrchr(filenametmp, '.'), ".bin") == 0) )  {
            vcsromlist[countvcs].directory = false;
            strcpy(vcsromlist[countvcs].filename,filenametmp);
            countvcs++;
          }
        }
      }
    }
    closedir(pdir);
  }
  if (countvcs)
    qsort (vcsromlist, countvcs, sizeof (FICA2600), a26Filescmp);
}
