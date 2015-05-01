/*************************************************************
*       Siemens Re-chargeable Emergency Notifier Wristband (SIREN)
*       EE459 Spring 2011 Team 5A(monitor): Christopher Cortes, Alexander Nobles, Steven Spinn
*************************************************************/

#include <hidef.h> /* for EnableInterrupts macro */
#include <string.h>
#include "derivative.h" /* include peripheral declarations */

/*
  The following puts the dummy interrupt service routine at
  location MY_ISR_ROM which is defined in the PRM file as
  the start of the FLASH ROM
*/

#pragma CODE_SEG MY_ISR_ROM

#pragma TRAP_PROC
void dummyISR(void)
{
}

/*
  This pragma sets the code storage back to default area of ROM as defined in
  the PRM file.
*/

#pragma CODE_SEG DEFAULT

typedef unsigned char uchar;
typedef unsigned int uint;

#define XCVR_RxANT    PTD_PTD4   
#define XCVR_TxANT    PTD_PTD3    
#define XCVR_IRQ      PTD_PTD2   
#define XCVR_CSN      PTD_PTD0 
#define XCVR_SCK      PTD_PTD1   
#define XCVR_SDI      PTD_PTD6 
#define XCVR_SDO      PTD_PTD7

//Global Variables
uchar locate[17] =     {0x29, 0x12, 0x34, 0x56, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x78};
uchar doneLocate[17] = {0x28, 0x12, 0x34, 0x56, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x78};
uchar sendSyncID[17] = {0x26, 0x12, 0x34, 0x56, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x78};

uchar RF_RXBUF[35];
uchar syncList[6][3];
unsigned char one = 1;
void initialize(void);
void strout(int, unsigned char *);
void iniout(unsigned char);
void cmdout(unsigned char);
void datout(unsigned char);
void nibout(unsigned char);
void busywt(void);
void del1m(int);
void del40u(int);
unsigned char checkKeypadInput(void);
void lcd_write(int, unsigned char*);
void clearLCD(void);
void setPassword(void);
unsigned char enterPwd(void);
void waitForDelay(void);
void waitForEnter(void);
void syncWristbands(void);
void locateWristbands(void);
void armSystem(void);
void disarmSystem(void);
void checkAlarm(void);
void write0(void);
void write1(void);
void writeByte(char address, char data);
char readByte(char address);
void init_RFM22(void);
void to_tx_mode(char[], char);
void to_rx_mode(void);
void rx_reset(void);
void send_read_address(uchar addr);
uchar send_command(void);

volatile struct {
  unsigned char Keypad:4;
} MyPTA @0x0000;

#define Keypad MyPTA.Keypad
#define KEYPAD_VALID !PTA_PTA4
#define LCD1 0
#define LCD2 0x40
#define ALARM_BUZZER PTA_PTA5

/*
  This pragma sets the area for allocating variables to the zero page
  of RAM (0x00 to 0xff) for more efficient access.
*/

#pragma DATA_SEG __SHORT_SEG MY_ZEROPAGE

unsigned char delay0;
unsigned char delay1;
unsigned char delay2;


unsigned char pwd[4];
unsigned char armed;
unsigned char state; //State 0 -> Initial state - set password
                     //State 1 -> Menu - Arm/Disarm
                     //State 2 -> Menu - Change Pwd
					 //State 3 -> Menu - Sync Wristband
					 //State 4 -> Menu - Locate Wristband
					 //State 5 -> Enter Pwd - *Not used*
					 //State 6 -> Change Pwd - *Not used*
					 //State 7 -> Status
					 
					 //Current Keypad Configuration
					 // * -> Left
					 // # -> Right
					 // A -> Menu
					 // B -> CLR
					 // C -> Bksp
					 // D -> Enter
					 
unsigned char alarmActive;
					 
                     
                     

/*
  This pragma sets the area for allocating variables back to the default area
  of RAM as defined in the PRM file.
*/

#pragma DATA_SEG DEFAULT

/*
    Declare these strings as "const" so they are allocated in ROM
*/




const unsigned char statusHdr[] = "PROTECTION STATUS:";
const unsigned char statusEn[] = "<< ENABLED >>";
const unsigned char statusDis[] = "<< DISABLED >>";
const unsigned char menuHdr[] = "Main Menu";
const unsigned char menuDisable[] = "> Disable Alarm";
const unsigned char menuEnable[] = "> Enable Alarm";
const unsigned char menuSetPwd[] = "> Set Password";
const unsigned char menuChangePwd[] = "> Change Password";
const unsigned char menuLocate[] = "> Locate Wristband";
const unsigned char menuSync[] = "> Sync Wristband";

const unsigned char setPwdHdr[] = "Set 4 Digit Password";
const unsigned char reenterPwdHdr[] = "Re-enter to confirm:";
const unsigned char enterPwdHdr[] = "Enter Password";


const unsigned char locateHdr[] =  "Locating Wristbands";
const unsigned char locateStop[] = "> Stop";

const unsigned char syncHdr[] =  "Press Sync button on";
const unsigned char syncInstr[] = "wristband to sync.";

const unsigned char pwdChanged[] = "Password changed";
const unsigned char pwdChanged2[] = "successfully.";
const unsigned char pwdSet[] = "Password set";
const unsigned char pwdMismatch[] = "Password mismatch.";
const unsigned char tryAgain[] = "Try again.";

const unsigned char incorrectPwd[] = "Incorrect password";
const unsigned char pwdPrompt[] = ">";

const unsigned char systemArmed[] =  "System Armed";
const unsigned char systemDisarmed[] =  "System Disarmed";

const unsigned char syncSuccessHdr[] = "Wristband Synced:";
const unsigned char syncFailureHdr[] = "Sync Failure";



void armSystem()
{
	armed = 1;
	lcd_write(LCD1, (unsigned char *)systemArmed);
	waitForDelay();
}

void disarmSystem()
{
	ALARM_BUZZER = 1;
	alarmActive = 0;
	armed = 0;
	lcd_write(LCD1, (unsigned char *)systemDisarmed);
	waitForDelay();
}

void lcd_write(int addr, unsigned char* c) {
  clearLCD();
  strout(addr, c);
}

void syncWristbands()
{
  unsigned char key;
  unsigned char i, j;
  unsigned char success = 0;
  unsigned char bandID;
	lcd_write(LCD1, (unsigned char *)syncHdr);
	strout(LCD2, (unsigned char *)syncInstr);
	//WIRELESS CODE GOES HERE -> WAIT TO RECEIVE SYNC INFO -> If CLR pressed, cancel
	while(one) {
	  if(KEYPAD_VALID) {
	    key = checkKeypadInput();
	    if(key == 'B') {
	      rx_reset();
	      lcd_write(LCD1, "Sync cancelled.");
  		  waitForDelay();
	      return;
	    }
	  }
	  if(XCVR_IRQ == 0)
  	{
  		send_read_address(0x7F);
  		for (i=0 ; i<17 ; i++)
  			RF_RXBUF[i] = send_command();
  		
  		if(RF_RXBUF[0] == 0x27) 
  		{
  		  lcd_write(LCD1, "Sync request received.");
	      strout(LCD2, "Syncing...");
	      for(i = 0; i < 6; i++) 
  		  {
  		    if(syncList[i][0] == RF_RXBUF[1] && syncList[i][1] == RF_RXBUF[2] && syncList[i][3] == RF_RXBUF[3]) 
  		    {
  		      lcd_write(LCD1, "Wristband already");
	          strout(LCD2, "synced.");
  		      return; 
  		    }
  		  }
  		  for(i = 0; i < 6; i++) 
  		  {
  		    if(syncList[i][0] == 0x00) 
  		    {
  		      bandID = i;
  		      for(j = 0; j < 3; j++) 
  		      {
  		        syncList[i][j] = RF_RXBUF[j+1];
  		      }
  		      success = 1;
  		      break; 
  		    }
  		  }
  		  
  		  //add next 3 bytes to sync list
  		  rx_reset();
  		  if(!success) {
  		    lcd_write(LCD1, (unsigned char*)syncFailureHdr);
  		    waitForDelay();
	        //strout(LCD2, <<ID NUM>>);
	        return;
  		  }
  		  break;
  		}  
  	} 
	}
	to_tx_mode(sendSyncID, 17);
	to_rx_mode();
	while(one) {
	  if(KEYPAD_VALID) {
	    key = checkKeypadInput();
	    if(key == 'B') {
	      syncList[bandID][0] = 0x00;
	      rx_reset();
	      lcd_write(LCD1, "Sync cancelled.");
  		  waitForDelay();
	      return;
	    }
	  }
	  if(XCVR_IRQ == 0)
  	{
  		send_read_address(0x7F);
  		for (i=0 ; i<17 ; i++)
  			RF_RXBUF[i] = send_command();
  		
  		if(RF_RXBUF[0] == 0x25 && syncList[bandID][0] == RF_RXBUF[1] && syncList[bandID][1] == RF_RXBUF[2] && syncList[bandID][3] == RF_RXBUF[3]) 
  		{
  		  lcd_write(LCD1, (unsigned char*)syncSuccessHdr);
	      strout(LCD2, (unsigned char*)syncList[bandID]);
  		  rx_reset();
  		  break;
  		}  
  	} 
	}
	
	

	//temporary:
	waitForDelay();
	
}

void locateWristbands()
{
	lcd_write(LCD1, (unsigned char *)locateHdr);
	strout(LCD2, (unsigned char *)locateStop);
	//WIRELESS CODE GOES HERE -> Send beep command
	
	to_tx_mode(locate, 17);
	
	waitForEnter();
	to_tx_mode(doneLocate, 17);
	//WIRELESS CODE -> Send stop beep command
}

void setPassword() {
  //unsigned char display[2] = "\00";
  unsigned char data;
  unsigned char i;
  int lcdAddr = 0x41;
  unsigned char confirm[4];
  unsigned char newpwd[4];
  unsigned char valid = 0;
  unsigned char done = 0;
  int j;
  
  
  while(!valid)
  {
    lcd_write(LCD1, (unsigned char *)setPwdHdr);
    strout(LCD2, (unsigned char *)pwdPrompt);
    i = 0x04;
    done = 0;
    lcdAddr = 0x41;
	  while(!done) { //Enter 4 digit password.
  		if(KEYPAD_VALID) 
  		{
  		  data = checkKeypadInput();
  		  if(data == 'B' && state != 0) {
  		    return; 
  		  }
  		  if(data == 'C' && i < 4) //backspace pressed
  		  {
  			  //display[0] = ' ';
  			  lcdAddr--;
  			  strout(lcdAddr, " ");
  			  i++;
  		  }
  		  else if(i > 0 && data != '#' && data != '*' && data != 'A' && data != 'B' && data != 'C' && data != 'D')
  		  {
  			  newpwd[4-i] = data;
  			  //display[0] = data;
  			  strout(lcdAddr, "*");
  			  lcdAddr++;
  			  i--;
  		  }
  		  else if(data == 'D' && i == 0) //Must press enter key to continue
  		  {
  			  done = 1;
  		  } 
  		}
	  }
	  
    lcd_write(LCD1, (unsigned char *)reenterPwdHdr);
    strout(LCD2, (unsigned char *)pwdPrompt);
    lcdAddr = 0x41;
    i = 0x04;
    done = 0;
  	while(!done) { //Re-enter password for confirmation
    		if(KEYPAD_VALID) {
    		  data = checkKeypadInput();
    		  if(data == 'B' && state != 0) {
  		       return; 
  		    }
    		  if(data == 'C' && i < 4) //backspace pressed
    		  {
      			//display[0] = ' ';
      			lcdAddr--;
      			strout(lcdAddr, " ");
      			i++;
    		  }
    		  else if(i > 0 && data != '#' && data != '*' && data != 'A' && data != 'B' && data != 'C' && data != 'D')
    		  {
      			confirm[4-i] = data;
      			//display[0] = data;
      			strout(lcdAddr, "*");
      			lcdAddr++;
      			i--;
    		  }
    		  else if(data == 'D' && i == 0) //Must press enter key to continue
    		  {
    			  done = 1;
    		  }
    		}
    }
    valid = 1;
    for(j = 0; j < 4; j++) //check two passwords to make sure they match
    {
    	if(newpwd[j] != confirm[j])
    	{
    		valid = 0;
    		lcd_write(LCD1, (unsigned char *)pwdMismatch);
    		strout(LCD2, (unsigned char *)tryAgain);
    		waitForDelay();
    		break;
    	}
    }
	  
  }
  for(j = 0; j < 4; j++) {
    pwd[j] = newpwd[j]; 
  }
  lcd_write(LCD1, (unsigned char *)pwdSet);
  strout(LCD2, (unsigned char *)pwdChanged2);
  waitForDelay();  
}

void waitForDelay()
{
	unsigned char data;
	int counter = 0;
	while(counter < 3000) //Wait for 3 sec or until user presses Enter
	{
		if(KEYPAD_VALID){
			data = checkKeypadInput();
			if(data == 'D')
			{
				return;
			}
		}
		del1m(1);		
		counter++;
	}
}

void waitForEnter()
{
	unsigned char data;
	while(one) //Wait for user to press Enter
	{
		if(KEYPAD_VALID){
			data = checkKeypadInput();
			if(data == 'D')
			{
				return;
			}
		}
	}
}

unsigned char enterPwd()
{
	unsigned char data;
	unsigned char i = 0x04;
	int lcdAddr = 0x41;
	unsigned char temppwd[4];
	unsigned char valid = 0;
	unsigned char done = 0;
	int j;
	lcd_write(LCD1, (unsigned char *)enterPwdHdr);
	strout(LCD2, (unsigned char *)pwdPrompt);
	while(!done) { //Enter 4 digit password.
		if(KEYPAD_VALID) {
		  data = checkKeypadInput();
		  if(data == 'B') //Pressed clear
		  {
			return 0;
		  }
		  if(data == 'C' && i < 4) //backspace pressed
		  {
			lcdAddr--;
			strout(lcdAddr, " ");
			i++;
		  }
		  else if(i > 0 && data != '#' && data != '*' && data != 'A' && data != 'B' && data != 'C' && data != 'D')
		  {
			temppwd[4-i] = data;
			strout(lcdAddr, "*");
			lcdAddr++;
			i--;
		  }
		  else if(data == 'D' && i == 0) //Must press enter key to continue
		  {
			done = 1;
		  }
		}
	}
	valid = 1;
	for(j = 0; j < 4; j++) //check two passwords to make sure they match
	{
		if(pwd[j] != temppwd[j])
		{
			valid = 0;
			break;
		}
	}
	if(!valid)
	{
		lcd_write(LCD1, (unsigned char *)incorrectPwd);
		strout(LCD2, (unsigned char *)tryAgain);
		waitForDelay();
		return 0;
	}	
	return 1;
}

unsigned char checkKeypadInput() {
	unsigned char r;
    switch(Keypad) {
      case 0b0000:
         r = '1';
         break;
      case 0b0001:
         r = '2';
         break;
      case 0b0010:
         r = '3';
         break;
      case 0b0011:
         r = 'A';
         break;
      case 0b0100:
         r = '4';
         break;
      case 0b0101:
         r = '5';
         break;
      case 0b0110:
         r = '6';
         break;
      case 0b0111:          
         r = 'B';
         break;
      case 0b1000:
         r = '7';
         break;
      case 0b1001:
         r = '8';
         break;
      case 0b1010:
         r = '9';
         break;
      case 0b1011:
         r = 'C';
         break;
      case 0b1100:
         r = '*';
         break;
      case 0b1101:
         r = '0';
         break;
      case 0b1110:
         r = '#';
         break;
      case 0b1111:
         r = 'D';
         break;
    }
	del1m(70);
	return r;
}


void clearLCD() {
    cmdout(0x01);
}

/*
  strout - Print the contents of the character string "s" starting at LCD
  RAM location "x".  The string must be terminated by a zero byte.
*/
void strout(int x, unsigned char *s)
{
    unsigned char ch;

    cmdout(x | 0x80);   // Make A contain a Set Display Address command


    while ((ch = *s++) != (unsigned char) NULL) {
        datout(ch);     // Output the next character
    }
}

/*
  datout - Output a byte to the LCD display data register (the display)
  and wait for the busy flag to reset.
*/
void datout(unsigned char x)
{
    PTB_PTB4 = 1;            // RS=1
    PTB_PTB5 = 0;            // Set R/W=0, 
    PTB_PTB6 = 0;            // E=0
    nibout(x);          // Put data bits 4-7 in PTB
    nibout(x << 4);     // Put data bits 0-3 in PTB
    busywt();           // Wait for BUSY flag to reset
}

/*
  cmdout - Output a byte to the LCD display instruction register and
  wait for the busy flag to reset.
*/
void cmdout(unsigned char x)
{
    PTB_PTB4 = 0;            // RS=0
    PTB_PTB5 = 0;            // Set R/W=0, 
    PTB_PTB6 = 0;            // E=0
    nibout(x);          // Output command bits 4-7
    nibout(x << 4);     // Output command bits 0-3
    busywt();           // Wait for BUSY flag to reset
}

/*
  nibout - Puts bits 4-7 from x into the four bits of Port B that we're
  using to talk to the LCD.  The other bits of Port B are unchanged.
  Toggle the E control line low-high-low.  
*/
void nibout(unsigned char x)
{               

    PTB = ((x >> 4) & 0x0f) | (PTB & 0xf0);   // Use bits 0-3

    PTB_PTB6 = 1;       // Set E to 1
    PTB_PTB6 = 0;       // Set E to 0
}

/*
  initialize - Do various things to force a initialization of the LCD
  display by instructions, and then set up the display parameters and
  turn the display on.
*/
void initialize()
{
    del1m(15);          // Delay at least 15ms

    iniout(0x30);       // Send a 0x30
    del1m(4);           // Delay at least 4msec

    iniout(0x30);       // Send a 0x30
    del40u(3);          // Delay at least 100usec

    iniout(0x30);       // Send a 0x30
    del40u(3);          // Delay at least 100usec

    iniout(0x20);       // Function Set: 4-bit interface
    busywt();           // Wait for BUSY flag to reset

    cmdout(0x28);       // Function Set: 4-bit interface, 2 lines

    cmdout(0x0C);       // Display and cursor on
}

/*
  iniout - Output a byte to the LCD control register.  Same as the "cmdout"
  function but it doesn't wait for the BUSY flag since that flag isn't
  working during initialization.
*/
void iniout(unsigned char x)
{
    PTB_PTB4 = 0;            // RS=0
    PTB_PTB5 = 0;            // Set R/W=0, 
    PTB_PTB6 = 0;            // E=0
    nibout(x);
}

/*
  busywt - Wait for the BUSY flag to reset
*/
void busywt()
{
    unsigned char bf;

    DDRB = 0x70;           // Set Data lines for input
    PTB_PTB4 = 0;            // RS=0
    PTB_PTB5 = 1;            // Set R/W=1, 
    PTB_PTB6 = 0;            // E=0
    do {
        PTB_PTB6 = 1;   // Set E=1

        bf = PTB & 0x08;  // Read status register bits 0-3

        PTB_PTB6 = 0;   // Set E=0
        PTB_PTB6 = 1;   // Set E=1, ignore bits 0-3
        PTB_PTB6 = 0;   // Set E=0
    } while (bf != 0);  // If Busy (PTB7=1), loop

    DDRB = 0xff;        // Set PTB for output
}

/*
  Note: these delay routines only work if the the delay0 and delay1
  variables are located in the RAM zero page
*/

/*
  del40u - Delay about 40usec times the "d" argument
*/
void del40u(int d)
{
    while (d--) {
        asm {
            ; The following code delays about 40 microseconds by looping.
            ; Total time is 4 + 13 * (4 + 3) = 95 cycles
            ; A 9.8304MHz external clock gives an internal CPU clock of
            ; 2.4576MHz (407ns/cycle).  95 x .407 usec = 38.7 usec.
            ; The JSR and RTS instructions will add a bit more.
                    mov     #13,delay0  ; 4 cycles
            u1:     dec     delay0      ; 4 cycles
                    bne     u1          ; 3 cycles
        }
    }
}

/*
  del1m - Delay about 1msec times the "d" argument.
*/
void del1m(int d)
{
    while (d--) {
        asm {
            ; The following code delays 1 millisecond by looping.
            ; Total time is 4 + 5 * (4 + 68 * (4 + 3) + 4 + 3) = 2439 cycles
            ; A 9.8304MHz external clock gives an internal CPU clock of
            ; 2.4576MHz (407ns/cycle).  Delay is then .993 milliseconds.
                    mov     #5,delay1   ; 4 cycles
            m1:     mov     #68,delay0  ; 4 cycles
            m0:     dec     delay0      ; 4 cycles
                    bne     m0          ; 3 cycles
                    dec     delay1      ; 4 cycles
                    bne     m1          ; 3 cycles
        }
    }
}

void write0() {
  XCVR_SCK = 0;
  XCVR_SDI = 0;
  XCVR_SCK = 1;
}

void write1() {
  XCVR_SCK = 0;
  XCVR_SDI = 1;
  XCVR_SCK = 1;
}

void writeByte(char address, char data)
{
  char cnt;
  address |= 0x80;
  XCVR_SCK = 0;
  XCVR_CSN = 0;
  for(cnt=0 ; cnt<8 ; cnt++) {
    if(address & 0x80)
      write1();
    else
      write0();
    address <<= 1;
  }
  for(cnt=0 ; cnt<8 ; cnt++) {
    if(data & 0x80)
      write1();
    else
      write0();
    data <<= 1;
  }
  XCVR_SCK = 0;
  XCVR_CSN = 1;
}

char readByte(char address)
{
  char cnt, read_data;
  address &= 0x7F;  
  XCVR_SCK = 0;
  XCVR_CSN = 0;
  for(cnt=0 ; cnt<8 ; cnt++) {
    if(address & 0x80)
      write1();
    else
      write0();
    address <<= 1;
  }
  read_data = 0x00;
  for(cnt=0 ; cnt<8 ; cnt++) {
    read_data <<= 1;
    XCVR_SCK = 0;
    if(XCVR_SDO == 1)
      read_data |= 0x01;
    XCVR_SCK = 1;
  }
  XCVR_SCK = 0;
  XCVR_CSN = 1;
  return read_data;
}

// Initialize the RFM22 for transmitting
void init_RFM22(void)
{
	writeByte(0x06, 0x00);	  // Disable all interrupts
	writeByte(0x07, 0x01);		// Set READY mode
	writeByte(0x09, 0x7F);		// Cap = 12.5pF
	writeByte(0x0A, 0x2B);		// Clk output is 4MHz, on GPIO2
	
	writeByte(0x0B, 0xF4);		// GPIO0 is for RX data output
	writeByte(0x0C, 0xEF);		// GPIO1 is TX/RX data CLK output
	writeByte(0x0D, 0x00);		// GPIO2 for MCLK output
	writeByte(0x0E, 0x00);		// GPIO port use default value
	
	writeByte(0x0F, 0x70);		// NO ADC used
	writeByte(0x10, 0x00);		// no ADC used
	writeByte(0x12, 0x00);		// No temp sensor used
	writeByte(0x13, 0x00);		// no temp sensor used
	
	writeByte(0x70, 0x20);		// No manchester code, no data whiting, data rate < 30Kbps
	
	writeByte(0x1C, 0x1D);		// IF filter bandwidth
	writeByte(0x1D, 0x40);		// AFC Loop
	//writeByte(0x1E, 0x0A);	// AFC timing
	
	writeByte(0x20, 0xA1);		// clock recovery
	writeByte(0x21, 0x20);		// clock recovery
	writeByte(0x22, 0x4E);		// clock recovery
	writeByte(0x23, 0xA5);		// clock recovery
	writeByte(0x24, 0x00);		// clock recovery timing
	writeByte(0x25, 0x0A);		// clock recovery timing
	
	//writeByte(0x2A, 0x18);
	writeByte(0x2C, 0x00);
	writeByte(0x2D, 0x00);
	writeByte(0x2E, 0x00);
	
	writeByte(0x6E, 0x27);		// TX data rate 1
	writeByte(0x6F, 0x52);		// TX data rate 0
	
	writeByte(0x30, 0x8C);		// Data access control
	
	writeByte(0x32, 0xFF);		// Header control
	
	writeByte(0x33, 0x42);		// Header 3, 2, 1, 0 used for head length, fixed packet length, synchronize word length 3, 2,
	
	writeByte(0x34, 64);		// 64 nibble = 32 byte preamble
	writeByte(0x35, 0x20);		// 0x35 need to detect 20bit preamble
	writeByte(0x36, 0x2D);		// synchronize word
	writeByte(0x37, 0xD4);
	writeByte(0x38, 0x00);
	writeByte(0x39, 0x00);
	writeByte(0x3A, 's');		// set tx header 3
	writeByte(0x3B, 'o');		// set tx header 2
	writeByte(0x3C, 'n');		// set tx header 1
	writeByte(0x3D, 'g');		// set tx header 0
	writeByte(0x3E, 17);		// set packet length to 17 bytes
	
	writeByte(0x3F, 's');		// set rx header
	writeByte(0x40, 'o');
	writeByte(0x41, 'n');
	writeByte(0x42, 'g');
	writeByte(0x43, 0xFF);		// check all bits
	writeByte(0x44, 0xFF);		// Check all bits
	writeByte(0x45, 0xFF);		// check all bits
	writeByte(0x46, 0xFF);		// Check all bits
	
	writeByte(0x56, 0x01);
	
	writeByte(0x6D, 0x07);		// Tx power to max
	
	writeByte(0x79, 0x00);		// no frequency hopping
	writeByte(0x7A, 0x00);		// no frequency hopping
	
	writeByte(0x71, 0x22);		// GFSK, fd[8]=0, no invert for TX/RX data, FIFO mode, txclk-->gpio
	
	writeByte(0x72, 0x48);		// Frequency deviation setting to 45K=72*625
	
	writeByte(0x73, 0x00);		// No frequency offset
	writeByte(0x74, 0x00);		// No frequency offset
	
	writeByte(0x75, 0x53);		// frequency set to 434MHz
	writeByte(0x76, 0x64);		// frequency set to 434MHz
	writeByte(0x77, 0x00);		// frequency set to 434Mhz
	
	writeByte(0x5A, 0x7F);
	writeByte(0x59, 0x40);
	writeByte(0x58, 0x80);
	
	writeByte(0x6A, 0x0B);
	writeByte(0x68, 0x04);
	writeByte(0x1F, 0x03);
}

void to_tx_mode(unsigned char buf[], char size)
{
	uchar i;
	writeByte(0x07, 0x01);	// To ready mode
	del1m(50);	
	XCVR_RxANT = 0;
	XCVR_TxANT = 1;
	
	writeByte(0x08, 0x03);	// FIFO reset
	writeByte(0x08, 0x00);	// Clear FIFO
	writeByte(0x34, 64);	// preamble = 64nibble
	writeByte(0x3E, size);	// packet length = 17bytes
	for (i=0; i<size; i++)
	{
		writeByte(0x7F, buf[i]);	// send payload to the FIFO
	}

	writeByte(0x05, 0x04);	// enable packet sent interrupt
	i = readByte(0x03);		// Read Interrupt status1 register
	i = readByte(0x04);
	
	writeByte(0x07, 9);	// Start TX
	
	while(XCVR_IRQ);
	writeByte(0x07, 0x01);	// to ready mode
	
	XCVR_RxANT = 0;
	XCVR_TxANT = 0;
}

void rx_reset(void) { 
  int i;
  writeByte(0x07, 0x01);
	i = readByte(0x03);		//read Interrupt Status1 reg
	i = readByte(0x04);
	writeByte(0x7E, 0x17);
	writeByte(0x08, 0x03);	//fifo reset
	writeByte(0x08, 0x00);
	writeByte(0x07, 0x05);
	writeByte(0x05, 0x02);
}

void to_rx_mode(void) {
	writeByte(0x07, 0x01);	//to ready mode
	XCVR_RxANT = 1;
	XCVR_TxANT = 0;
	rx_reset();
}

uchar send_command(void) { 
  uchar Result, i; 
  
  XCVR_SCK = 0; 
  Result = 0; 
  for(i=0 ; i<8 ; i++) {                     
    Result = Result<<1; 
    XCVR_SCK = 1; 
    //NOP(); 
    if(XCVR_SDO) 
      Result |= 1; 
      
    XCVR_SCK = 0; 
    //NOP(); 
 }
 return(Result); 
}

void send_read_address(uchar addr) 
{ 
  uchar cnt = 8;
  addr &= 0x7f; // spi read funciton 
   
  XCVR_CSN = 1;
  XCVR_SCK = 0; 
  XCVR_CSN = 0;
  while(cnt--)  // cycle to 8 times 
  { 
     if(addr & 0x80)// send one bit command 
        write1(); 
     else 
        write0();    
     addr = addr << 1; 
  } 
} 

void main(void) {
    unsigned char c[2] = "\00";
	  unsigned char key;
	  unsigned char pwdCheck = 0;
	  unsigned char lcdUpdate = 1;
	  int timer = 0;
	  
	  uchar i, chksum;

    // EnableInterrupts; /* enable interrupts */
    /* include your code here */

    CONFIG1_COPD = 1;           // disable COP reset


    DDRB = 0xff;                // Set PTB bits 0-7 for output
    DDRA_DDRA5 = 1;
    
    DDRD_DDRD0 = 1; //Set bits for RFM22B
    DDRD_DDRD1 = 1;
    DDRD_DDRD2 = 0;
    DDRD_DDRD3 = 1;
    DDRD_DDRD4 = 1;
    DDRD_DDRD6 = 1;
    DDRD_DDRD7 = 0;
    
    init_RFM22();
    
    initialize();               // Initialize the LCD display
    
    //Set cursor back to home
    
    PTB_PTB4 = 0;            // RS=1
    PTB_PTB5 = 0;            // Set R/W=0, 
    PTB_PTB6 = 0;            // E=0
    cmdout(0x02);   
    
    
    state = 0;
    armed = 0;
	  alarmActive = 0;
	  ALARM_BUZZER = 1;  //set initial state variables
    setPassword();
    state = 1;
    
    lcd_write(LCD1, "        SIEMENS         ");    //Show splash screen
    strout(LCD2, "         SIREN          ");
    del1m(5000);
    
    to_rx_mode(); //start Rx
    
    
    while (one) {               // Loop forever
    if(lcdUpdate) {
      lcdUpdate = 0;
  		if(state == 1)
  		{
  			if(armed)
  			{
  				lcd_write(LCD1, (unsigned char *)menuHdr);
  				strout(LCD2, (unsigned char *)menuDisable);
  			}
  			else
  			{
  				lcd_write(LCD1, (unsigned char *)menuHdr);
  				strout(LCD2, (unsigned char *)menuEnable);
  			}
  		} 
  		else if(state == 2)
  		{
  			lcd_write(LCD1, (unsigned char *)menuHdr);
  			strout(LCD2, (unsigned char *)menuChangePwd);
  		} 
  		else if(state == 3)
  		{
  			lcd_write(LCD1, (unsigned char *)menuHdr);
  			strout(LCD2, (unsigned char *)menuSync);
  		} 
  		else if(state == 4)
  		{
  			lcd_write(LCD1, (unsigned char *)menuHdr);
  			strout(LCD2, (unsigned char *)menuLocate);
  		} 
  		else if(state == 7)
  		{
  			if(armed)
  			{
  				lcd_write(LCD1, (unsigned char *)statusHdr);
  				strout(LCD2, (unsigned char *)statusEn);
  			}
  			else
  			{
  				lcd_write(LCD1, (unsigned char *)statusHdr);
  				strout(LCD2, (unsigned char *)statusDis);
  			}
  		}
    }
	
		
    if(KEYPAD_VALID) 
    {
    
      lcdUpdate = 1;
      timer = 0;
  		key = checkKeypadInput();
  		if(key == '#')
  		{
  			if(state == 1)// && !armed)
  				state = 2;
  			else if(state == 2)
  				state = 3;
  			else if(state == 3)
  				state = 4;
  			else if(state == 4)
  				state = 1;
  		}
  		else if(key == '*')
  		{
  			if(state == 1)// && !armed)
  				state = 4;
  			else if(state == 2)
  				state = 1;
  			else if(state == 3)
  				state = 2;
  			else if(state == 4)
  				state = 3;
  		}
  		else if(key == 'D')
  		{
  			if(state == 1)
  			{
  				if(armed)
  				{
  					pwdCheck = enterPwd();
  					if(pwdCheck)
  					{
  						disarmSystem();
  						state = 1;
  					}
  				}
  				else
  				{
  					armSystem();
  					state = 7;
  				}
  			}
  			else if(state == 2)
  			{
  				pwdCheck = enterPwd();
  				if(pwdCheck)
  				{
  					setPassword();
  				}
  			}	
  			else if(state == 3)
  			{
  				syncWristbands();
  			}
  			else if(state == 4)
  			{
  				locateWristbands();
  			} 
  			else if(state == 7) 
  			{  
  				state = 1;
  			}
  		}
  		else if(key == 'B')
  		{
  			if(state == 1 || state == 2 || state == 3 || state == 4)
  			{	
  				state = 7;
  			}
  		}
  		else if(key == 'A')
  		{
  			if(state == 7)
  				state = 1;
  		}
    }
  	if(armed) //check for alarm signal
  	{
  		 
    	if(XCVR_IRQ == 0)
    	{
    		send_read_address(0x7F);
    		for (i=0 ; i<17 ; i++)
    			RF_RXBUF[i] = send_command();
    		
    		if(RF_RXBUF[0] == 0x30) {
    		  for(i = 0; i < 6; i++) {
		        if(RF_RXBUF[1] == syncList[i][0] && RF_RXBUF[2] == syncList[i][1] && RF_RXBUF[3] == syncList[i][2]) {
    		
    		      alarmActive = 1;
    			    ALARM_BUZZER = 0;
    			    lcd_write(LCD1, "ALERT! YOUR KID");
    			    strout(LCD2, "IS DROWNING!");
		        }
    		  }
    		}
    		
    		chksum = 0;
    		for(i=0 ; i<17 ; i++)
    			chksum += RF_RXBUF[i];
    		if(( chksum == RF_RXBUF[16] )&&( RF_RXBUF[0] == 0x30 )) 
          writeByte(0x07, 0x01);	//to ready mode
        else 
          rx_reset();
    	}    	
    	
    	if(alarmActive) {
    	   ALARM_BUZZER = 0;
    	   if(state == 7) {    
      		 strout(LCD1, "ALERT! YOUR KID         ");
    	  	 strout(LCD2, "IS DROWNING!            ");
    	   }
    	}
  	} 
  	else {
  	  if(XCVR_IRQ == 0) {
  	     rx_reset();
  	  }
  	}
  	
  	
  	del1m(1);
    if(timer > 20000) {
       lcdUpdate = 1;
       state = 7;
       timer = 0;
    }
    timer++;
   }
   
}

