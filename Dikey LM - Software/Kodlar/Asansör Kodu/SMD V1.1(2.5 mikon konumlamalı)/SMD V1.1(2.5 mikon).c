#INCLUDE <16f1826.h> 

#FUSES INTRC_IO                                                      // High Speed Oscilator (>4 Mhz) crystal
#FUSES NOWDT                                                         // Watch Dog Timer disabled
#FUSES PUT                                                           // Power Up Timer enabled
#FUSES MCLR                                                          // Master Clear pin enabled
#FUSES BROWNOUT                                                      // Brownout Reset enabled
#FUSES BORV25                                                        // Brownout Reset at 2.5V
#FUSES NOLVP                                                         // Low Voltage Programming disabled
#FUSES CPD                                                           // Data EEPROM code protected
#FUSES PROTECT                                                       // Code protected from reads
#FUSES NOIESO                                                        // Internal External Switch Over Mode disabled
#FUSES NOFCMEN                                                       // Fail-safe clock monitor disabled

#USE   DELAY(clock=16000000)                                         // delay() func. adjusted for 20Mhz Primary Osc.

// Pin assignments
#DEFINE DRV_STEP     PIN_A0                  // Step output pin
#DEFINE DRV_RESET    PIN_A1                  // Reset output pin to driver (Active-high reset input initializes all internal logic and disables the Hbridge outputs. Internal pulldown.)
#DEFINE LM_UP        PIN_A2                  // Up limit switch input pin
#DEFINE LM_DOWN      PIN_A3                  // Down limit switch input pin
#DEFINE DRV_DIR      PIN_A4                  // Direction output pin

#DEFINE SPI_SDO      PIN_A6                  // SPI data output pin
#DEFINE SPI_CS       PIN_A7                  // SPI chip select output pin

#DEFINE BT_DOWN      PIN_B0                  // Down button input pin
#DEFINE SPI_SDI      PIN_B1                  // SPI data input pin

#DEFINE BT_UP        PIN_B3                  // Up button input pin
#DEFINE SPI_SCK      PIN_B4                  // SPI clock output pin

#DEFINE DRV_STALL    PIN_B6                  // Stall input pin from driver (Internal stall detect mode: logic low when motor stall detected. Pull up mevcut)
#DEFINE DRV_FAULT    PIN_B7                  // Fault input pin from driver (Logic low when in fault condition. Pull up mevcut)






//OPTION Register 
#WORD OPTION         = 0x095
//Bits of Option Register
#BIT OPTION_WPUEN    = OPTION.7


// SPI Registers
#WORD SPI_SSP1CON1   = 0x215                 // Synchronous serial port control register
#WORD SPI_SSP1STAT   = 0x214                 // Synchronous serial port status register
#WORD SPI_SSP1BUF    = 0x211                 // Synchronous serial port buffer register
#WORD SPI_PIR1       = 0x011                 // Peripheral interrupt request register
// Pin function control registers
#WORD PIN_APFCON0    = 0x11D                 // Alternate pin function control register 0
#WORD PIN_APFCON1    = 0x11E                 // Alternate pin function control register 1

// Bits of SSP1CON1 register
#BIT SPI_WRITE_FLAG     = SPI_SSP1CON1.7     // Synchronous serial port write collision detect bit
#BIT SPI_ENABLE         = SPI_SSP1CON1.5     // Synchronous serial port enable bit
#BIT SPI_CLOCK_POLARITY = SPI_SSP1CON1.4     // Synchronous serial port clock polarity select bit
#BIT SPI_MODE_3         = SPI_SSP1CON1.3     // Synchronous serial port mode select bits
#BIT SPI_MODE_2         = SPI_SSP1CON1.2   
#BIT SPI_MODE_1         = SPI_SSP1CON1.1   
#BIT SPI_MODE_0         = SPI_SSP1CON1.0  

// Bits of SPI_SSP1STAT register
#BIT SPI_INPUT_SAMPLE   = SPI_SSP1STAT.7     // Synchronous serial port data input sample bit
#BIT SPI_CLOCK_EDGE     = SPI_SSP1STAT.6     // Synchronous serial port clock edge select bit
#BIT SPI_BUFFER_STATUS  = SPI_SSP1STAT.0     // Synchronous serial port buffer full status bit

// Bits of SPI_PIR1 register
#BIT SPI_FLAG           = SPI_PIR1.3         // Synchronous serial port interrupt flag bit

// Bits of PIN_APFCON0 register
#BIT PIN_RX_SELECT      = PIN_APFCON0.7      // RX pin selection bit
#BIT PIN_SDO_SELECT     = PIN_APFCON0.6      // SDO pin selection bit 
#BIT PIN_SS_SELECT      = PIN_APFCON0.5      // SS pin selection bit 

// Bits of PIN_APFCON1 register
#BIT PIN_TX_SELECT      = PIN_APFCON1.0      // TX pin selection bit


enum  State     {OFF = 0, ON   = 1};                                 // Motor states
enum  Direction {UP  = 0, DOWN = 1};                                 // Direction of motion
enum  Motion    {ACC = 0, WALK = 1, RUN  = 2, DEC = 3, STEADY = 4};  // State of the motion

int16 const run_lim          = 800;                                  // Duration of the slow motion (per count) before acclerating to high speed  
int   const acc_lim          = 117;                                  // Number of steps before we hit max speed. acc=10000 dec=10000 
int16 const periods[acc_lim] = {2449,1015,779,656,578,523,481,447,420,397,378,361,346,333,322,311,302,293,285,277,271,264,258,253,247,243,238,234,229,226,222,
218,215,212,209,206,203,200,197,195,192,190,188,186,184,182,180,178,176,174,172,171,169,167,166,164,163,162,160,159,157,156,155,154,152,151,150,149,148,147,
146,145,144,143,142,141,140,139,138,137,136,135,134,133,132,131,130,129,128,127,126,125,124,123,122,121,120,119,118,117,116,115,114,113,112,111,110,109,108,
107,106,105,104,103,102,101,100};

int const off_time = 50;                                             // Off time of the pwm signal (should be smaller than period)
int8      motion_state = STEADY;                                     // Current state of the motion

void set_SPI()
{
   // Disable SPI to set registers
   SPI_ENABLE = 0;
   // Set idle state of the clock to low 
   SPI_CLOCK_POLARITY = 0;
   // Set SPI mode to SPI 
   SPI_MODE_3 = 0; 
   SPI_MODE_2 = 0;
   SPI_MODE_1 = 1;
   SPI_MODE_0 = 0;
   // Input data sampled at the middle of data output time
   SPI_INPUT_SAMPLE = 0;
   // Transmit occurs on transition from active to idle clock state
   SPI_CLOCK_EDGE = 1;

   // Enable SPI
   SPI_ENABLE = 1;
}

void set_variables()
{
   output_low(SPI_CS);     // Chip select is active high so keep it low to prevent out-of-sync transaction
   output_low(DRV_RESET);  // Reset is active high so keep reset pin low to activate driver 
}

void set_pins()
{
   // Set RB2 as RX pin
   //PIN_RX_SELECT = 1;
   // Set RA6 as SDO pin
   PIN_SDO_SELECT = 1;
   // Set RA5 as SS pin
   PIN_SS_SELECT = 1;
   // Set RB5 as TX pin
   //PIN_TX_SELECT = 1;
}


// Sets the motor state
void md_set_state(State value)
{
   if (value == on)
   {
   output_high(SPI_CS);
                    //FEDCBA98
   int Ctrl_1     = 0b00001100;
                    //76543210
   int Ctrl_0     = 0b00011001;// Set enable pin to given motor state
   SPI_SSP1BUF = Ctrl_1;
   while (!SPI_FLAG);
   SPI_FLAG = 0;
   SPI_SSP1BUF = Ctrl_0;
   while (!SPI_FLAG);
   SPI_FLAG = 0;
   output_low(SPI_CS);
   delay_ms(10);
   }
   else if (value == off)
   {
   output_high(SPI_CS);
                    //FEDCBA98
   int Ctrl_1     = 0b00001100;
                    //76543210
   int Ctrl_0     = 0b00011000;// Set enable pin to given motor state
   SPI_SSP1BUF = Ctrl_1;
   while (!SPI_FLAG);
   SPI_FLAG = 0;
   SPI_SSP1BUF = Ctrl_0;
   while (!SPI_FLAG);
   SPI_FLAG = 0;
   output_low(SPI_CS);
   delay_ms(10);
   }
}
// Sets the motion direction
void md_set_direction(Direction value)
{
      output_bit(DRV_DIR, !value);                                     // Set direction pin to given value
}
// Initializes motor driver
void md_init()
{
   md_set_state(OFF);                                                 // Motor off
   output_high(DRV_DIR);                                              // Direction control pin can be in any state
   output_high(DRV_STEP);                                             // Keep step input pin high (A low-to-high transition advances the motor one increment 
   
   output_high(SPI_CS);
                    //FEDCBA98
   int Ctrl_1     = 0b00001100;
                    //76543210
   int Ctrl_0     = 0b00011000;
   SPI_SSP1BUF = Ctrl_1;
   while (!SPI_FLAG);
   SPI_FLAG = 0;
   SPI_SSP1BUF = Ctrl_0;
   while (!SPI_FLAG);
   SPI_FLAG = 0;
   output_low(SPI_CS);
   delay_ms(10);

   output_high(SPI_CS);
                    //FEDCBA98
   int Torque_1   = 0b00010000;
                    //76543210
   int Torque_0   = 0b10110111;
   SPI_SSP1BUF = Torque_1;
   while (!SPI_FLAG);
   SPI_FLAG = 0;
   SPI_SSP1BUF = Torque_0;
   while (!SPI_FLAG);
   SPI_FLAG = 0;
   output_low(SPI_CS);
   delay_ms(10);
   
   output_high(SPI_CS);
                    //FEDCBA98
   int Off_1      = 0b00100000;
                    //76543210
   int Off_0      = 0b01111001;
   SPI_SSP1BUF = Off_1;
   while (!SPI_FLAG);
   SPI_FLAG = 0;
   SPI_SSP1BUF = Off_0;
   while (!SPI_FLAG);
   SPI_FLAG = 0;
   output_low(SPI_CS);
   delay_ms(10);
   
   output_high(SPI_CS);
                    //FEDCBA98
   int Blank_1    = 0b00110001;
                    //76543210
   int Blank_0    = 0b10010110;
   SPI_SSP1BUF = Blank_1;
   while (!SPI_FLAG);
   SPI_FLAG = 0;
   SPI_SSP1BUF = Blank_0;
   while (!SPI_FLAG);
   SPI_FLAG = 0;
   output_low(SPI_CS);
   delay_ms(10);
   
   output_high(SPI_CS);
                    //FEDCBA98
   int Decay_1    = 0b01000101;
                    //76543210
   int Decay_0    = 0b00011100;
   SPI_SSP1BUF = Decay_1;
   while (!SPI_FLAG);
   SPI_FLAG = 0;
   SPI_SSP1BUF = Decay_0;
   while (!SPI_FLAG);
   SPI_FLAG = 0;
   output_low(SPI_CS);
   delay_ms(10);
   
   output_high(SPI_CS);
   int Stall_1    = 0b01011001;
   int Stall_0    = 0b00010100;
   SPI_SSP1BUF = Stall_1;
   while (!SPI_FLAG);
   SPI_FLAG = 0;
   SPI_SSP1BUF = Stall_0;
   while (!SPI_FLAG);
   SPI_FLAG = 0;
   output_low(SPI_CS);
   delay_ms(10);
   
   output_high(SPI_CS);
                    //FEDCBA98
   int Drive_1    = 0b01101010;
                    //76543210
   int Drive_0    = 0b10100000;
   SPI_SSP1BUF = Drive_1;
   while (!SPI_FLAG);
   SPI_FLAG = 0;
   SPI_SSP1BUF = Drive_0;
   while (!SPI_FLAG);
   SPI_FLAG = 0;
   output_low(SPI_CS);
   delay_ms(10);
   
   output_high(SPI_CS);
   int Status_1   = 0b01110000;
   int Status_0   = 0b00000000;
   SPI_SSP1BUF = Status_1;
   while (!SPI_FLAG);
   SPI_FLAG = 0;
   SPI_SSP1BUF = Status_0;
   while (!SPI_FLAG);
   SPI_FLAG = 0;
   output_low(SPI_CS);
   delay_ms(10);

   delay_ms(100);
}

// Starts motion cycle
void motion_cycle()
{
   // Start cycle 
   int16 step_count  =  0;
   int16 period      =  0;
   int1  running     =  0;
   int32 i=0; 
   while(true)
   {
      if(input(BT_UP))
      {
         if(input(LM_UP) == 0 && running == 0)
         {
            md_set_direction(UP);
            md_set_state(ON);
            
            //2.5 mikro step git             
            for(i=0; i<2; i++)
            { 
               output_high(DRV_STEP);  //Rampa kalk��
               delay_us(2449);
               output_low(DRV_STEP);
               delay_us(2449);
            }
            md_set_state(OFF);
            delay_ms(200);
            
            if(input(BT_UP)){
            delay_ms(300);
               if(input(BT_UP))  {
                  motion_state = WALK;
                                 }
                            }
            //

         }
         else if(input(LM_UP) && running == 1)
         {
            if(motion_state == ACC || motion_state == RUN)
               motion_state = DEC;
            else if(motion_state == WALK)
               motion_state = STEADY;
         }
      }
      else if(input(BT_DOWN))
      {
         if(input(LM_DOWN) == 0 && running == 0)
         {
            md_set_direction(DOWN);
            md_set_state(ON);
            //2.5 mikro step git             
            for(i=0; i<2; i++)
            { 
               output_high(DRV_STEP);  //Rampa kalk��
               delay_us(2449);
               output_low(DRV_STEP);
               delay_us(2449);
            }
            md_set_state(OFF);
            delay_ms(200);
            
            if(input(BT_DOWN)){
            delay_ms(300);
               if(input(BT_DOWN))  {
                  motion_state = WALK;
                                 }
                            }
         }
         else if(input(LM_DOWN) && running == 1)
         {
            if(motion_state == ACC || motion_state == RUN)
               motion_state = DEC;
            else if(motion_state == WALK)
               motion_state = STEADY;
         }
      }
      else
      {
         if(motion_state == ACC || motion_state == RUN)
            motion_state = DEC;
         else if(motion_state == WALK)
            motion_state = STEADY;
      }
      
      

      switch(motion_state) 
      { 
         case WALK:
            step_count++;
         
            if(step_count == 1)
            {
               running = 1;
               md_set_state(ON);
               period = periods[0];
            }
            else if(step_count == run_lim)
            {
               step_count   = 0;
               motion_state = ACC;
            }
            break;
         case ACC:
            if(step_count == acc_lim - 1)
            {
               motion_state = RUN;
            }
               
            period = periods[step_count];
            step_count++;
            break;
            
         case RUN:
            period = periods[step_count - 1];
            break;
   
         case DEC:
            step_count--;
            // Check if we at last step
            if(step_count == 0)
            {
               motion_state = STEADY;
            }
               
            period = periods[step_count];
            break;
            
         case STEADY:
            if(running)
            {
               running    = 0;
               period     = 0;
               step_count = 0;
               md_set_state(OFF);
            }
            break;
      }
   
      if(running)
      {  
         delay_us(off_time);
         output_low(DRV_STEP);
         delay_us(period - off_time);
         output_high(DRV_STEP);
      }
   }
}

// Main method
void main()
{
   //             76543210
     set_tris_a(0b00101100);       // Set I/O states of the ports
     set_tris_b(0b11001111);
   
   delay_ms(500);
   
   //fprintf(RS232,"\n\n\rMODESIS LASER POSITIONING STAGE\n\n\r");
   
   set_pins();
   set_SPI();
   set_variables();
   md_init();                                                           // Initialize motor driver
   motion_cycle();                                                      // Start motion cycle
}
