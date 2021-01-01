/* uChipSAMD21unbrick Copyright (c) 2019 Nicola Wrachien <info@itaca-innovation.com>
 * All rights reserved.
 * 
 * This sketch/program is based on Adafruit's Arduino DAP library, which, in turn, is based on 
 * Alex Taradov Free-DAP code. Just small modifications have been made to the code.
 * The following is the copyright notice for the Adafruit's DAP library. This sketch/program inherits
 * the same license, see below:
 * 
 * Copyright (c) 2013-2017, Alex Taradov <alex@taradov.com>, Adafruit <info@adafruit.com>,
 * 
 * All rights reserved.
 *
 * This is mostly a re-mix of Alex Taradovs excellent Free-DAP code - we just put both halves into one library and wrapped it up in Arduino compatibility
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "Adafruit_DAP.h"
#include "bootloaderBIN.h"
#define WAIT_FOR_SERIAL_MONITOR
#define BOOTLOADERSIZE 8192 // we check only the first 8192 bytes, to speed up
#define SWDIO 10 // uchip pin 10
#define SWCLK 11 // uchip pin 11
#define SWRST 3  // actually not used!
#define PWR 12  // pin connected to the PNP/pMOSFET that enables/disables the board (active low)
// note: the following values are optimized for a uChip target board. Change them as per your application!
#define POWER_DOWN_DELAY_TIME 500 // time in ms to wait after the board is powered down (to discharge internal capacitors)
#define POWER_ON_WAIT_TIME  100  // time in ms to wait after the board is powered up
//
#define checkForAbort() do{if (abortRequired) return 0xFF;}while(0)   // macro to prematurely end the DAP operation
#define BUFSIZE 256       //don't change!
char tmpBuffer[512];      // for temporary string output.
boolean abortRequired = false;  // global variable to prematurely abort the process
Adafruit_DAP_SAM dap;     // DAP for SAM devices
#define OPERATION_PROGRAM 1
#define OPERATION_PROTECT_BOOTLOADER 2
#define OPERATION_UNPROTECT_BOOTLOADER 3
#define OPERATION_ERASE 4
// list of operations to perform
const uint8_t operations[] = {OPERATION_UNPROTECT_BOOTLOADER, OPERATION_ERASE, OPERATION_PROGRAM, OPERATION_PROTECT_BOOTLOADER};
// Function called when there's an SWD error
void error(const char *text) 
{
  SerialUSB.println(text);
  abortRequired = true;
}
uint8_t dapOperation(const uint8_t * bin, uint16_t size, uint32_t address, uint8_t operation)
{
  // turn off the board
  pinMode(SWCLK, INPUT);    // let's put SWCLK as input. If it was output (and held high), then issues might arise when powering down the target board.
  digitalWrite(PWR, HIGH);  // turn off the target board.
  // wait 0.5s so that the board capacitors are discharged
  delay(POWER_DOWN_DELAY_TIME);
  digitalWrite(SWCLK, LOW);     // set the SWCLK level low so that the SAMD21 boots in debug mode
  pinMode(SWCLK, OUTPUT);
  // turn on the board
  digitalWrite(PWR, LOW);
  // wait till the board is "ready"
  delay(POWER_ON_WAIT_TIME);
  // now begin!   
  dap.begin(SWCLK, SWDIO, SWRST, &error);
  SerialUSB.print("Connecting...");  
  dap.dap_disconnect();
  dap.dap_connect();
  dap.dap_transfer_configure(0, 128, 128);
  dap.dap_swd_configure(0);
  if (! dap.dap_reset_link())                      
  {
    error(dap.error_message);
    return 1;
  }
  if (! dap.dap_swj_clock(0))               
  {
    error(dap.error_message);
    return 2;
  }
  dap.dap_target_prepare();
  uint32_t dsu_did;
  if (! dap.select(&dsu_did)) 
  {
    SerialUSB.print("Unknown device found 0x"); 
    SerialUSB.print(dsu_did, HEX);
    error("Unknown device found");
    return 3;
  }
  for (device_t *device = dap.devices; device->dsu_did > 0; device++) 
  {
    if (device->dsu_did == dsu_did) 
    {
      SerialUSB.print("Found Target: ");
      SerialUSB.print(device->name);
      SerialUSB.print("\tFlash size: ");
      SerialUSB.print(device->flash_size);
      SerialUSB.print("\tFlash pages: ");
      SerialUSB.println(device->n_pages);
    }
  }
  uint8_t flashRow[BUFSIZE];        
  SerialUSB.println(" done.");
  switch (operation)
  {
    case OPERATION_UNPROTECT_BOOTLOADER:
      SerialUSB.print("Unprotecting bootloader (BOOTPROT = 7)... ");
      dap.fuseRead(); //MUST READ FUSES BEFORE SETTING OR WRITING ANY FUSE
      checkForAbort();
      dap._USER_ROW.BOOTPROT = 7;   // bootloader 0kB
      dap._USER_ROW.WDT_Always_On = 0;
      dap._USER_ROW.WDT_Enable = 0; // remove watchdog...
      dap.fuseWrite();
      checkForAbort();
      SerialUSB.print("Written!\r\n");     
      break; 
    case OPERATION_ERASE:
      {
        SerialUSB.print("Erasing... ");
        dap.erase();
        checkForAbort();
        SerialUSB.println(" done.");
        SerialUSB.print("Blank check of the bootloader section... ");
        uint32_t addr;
        while(addr < BOOTLOADERSIZE)
        {
          dap.readBlock(addr,flashRow);
          checkForAbort();
          for (int i = 0; i < BUFSIZE; i++)
          {
            if (flashRow[i] != 0xFF)
            {
                dap.dap_set_clock(50);
                dap.deselect();
                dap.dap_disconnect(); 
                snprintf(tmpBuffer,sizeof(tmpBuffer),"error at address 0x%08x, got 0x%02x\r\n", addr + i, flashRow[i] );
                error(tmpBuffer);
                return 1;
            }
          }
          addr += BUFSIZE;
          SerialUSB.print(".");
        }
        SerialUSB.print("done, no errors!\r\n");             
      }
      break; 
    case OPERATION_PROGRAM:
      {
        SerialUSB.print("Programming... ");
        unsigned long t = millis();
        uint32_t addr = dap.program_start(address);
      
        while(addr < (size + address))
        {
          // let us fill with 0xFF to avoid issues if the program is not a multiple of a flashrow
          memset(flashRow, 0xFF, sizeof(flashRow));
          memcpy(flashRow, bin + addr - address, min( BUFSIZE, size + address -  addr) );
          dap.programBlock(addr, flashRow );
          addr += BUFSIZE;
          checkForAbort();
        }
        SerialUSB.print(millis() - t);
        SerialUSB.println("ms done!");
        // verify
        t = millis();
        SerialUSB.print("Verifying... ");
        addr = address;
        while(addr < (size + address))
        {    
          dap.readBlock(addr,flashRow);
          checkForAbort();
          for (int i = 0; i < min( BUFSIZE, size + address -  addr); i++)
          {
              if (flashRow[i] != bin[addr - address + i])
              {
                  dap.dap_set_clock(50);
                  dap.deselect();
                  dap.dap_disconnect(); 
                  snprintf(tmpBuffer,sizeof(tmpBuffer),"error at address 0x%08x, got 0x%02x, expected 0x%02x\r\n", addr + i, flashRow[i], bin[addr - address + i]);
                  error(tmpBuffer);
                  return 2;
              }      
          }
          addr += BUFSIZE;
        }
        SerialUSB.print(millis() - t);
        SerialUSB.println(" ms done!");    
      }
      break;
    case OPERATION_PROTECT_BOOTLOADER:
      SerialUSB.print("Protecting bootloader (BOOTPROT =2) ... ");
      dap.fuseRead(); //MUST READ FUSES BEFORE SETTING OR WRITING ANY FUSE
      checkForAbort();
      dap._USER_ROW.BOOTPROT = 2;   // bootloader 8kB
      dap._USER_ROW.WDT_Always_On = 0;
      dap._USER_ROW.WDT_Enable = 0; // remove watchdog.
      dap.fuseWrite();
      SerialUSB.print("Written!\r\n");
      break;
    default:
      SerialUSB.print("Unknown command aborting!\r\n");
      return 0xff;
  }
  dap.dap_set_clock(50);
  dap.deselect();
  dap.dap_disconnect();   
  return 0;
}

void setup() 
{
  pinMode(SWCLK, OUTPUT);
  digitalWrite(SWCLK, LOW);
  pinMode(PWR, OUTPUT);
  digitalWrite(PWR, HIGH);
  pinMode(LED_BUILTIN, OUTPUT);  
  SerialUSB.begin(115200);
  #ifdef WAIT_FOR_SERIAL_MONITOR
    while(!SerialUSB);
  #endif
  SerialUSB.print("Program Start\r\n");
}


void loop() 
{
  static boolean done = false;
  static uint32_t lastTime = 0;
  uint32_t time = millis(); 
  if (!done)
  {        
    int res; 
    // the various operations are performed separately. In fact, at least before the ERASE operation, the board might be still bricked (e.g. if you coded an application that disables the SWD pins)
    for (int i = 0; i < sizeof(operations) && !abortRequired; i++)
    {
      res = dapOperation(binfile, sizeof(binfile), 0, operations[i]);
      if (res)
      {
        abortRequired = true;
        break;
      }
    }
    if (!abortRequired)
    {
      SerialUSB.println("Target successfully unbricked!");
      // turn off and on the target, to show that it is alive!
      digitalWrite(PWR, HIGH);
      delay(POWER_DOWN_DELAY_TIME);         
      digitalWrite(PWR, LOW);
    }
    else
    {
      SerialUSB.println("Something went wrong! The target is likely still bricked :(");      
    }
  } 
  //blink led on the host to show we're done
  done = true;
  if (done && !abortRequired)
  {
    if (time - lastTime > 200) 
    {
      lastTime = time;
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
  }
  else if (abortRequired)   // if something went wrong
  {
     digitalWrite(LED_BUILTIN, HIGH); // fixed LED: error  
  }
}
