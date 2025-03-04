#INCLUDE <16f1826.h> 

#FUSES INTRC_IO      // Internal RC clock (OSC1 and OSC2 pins are normal I/O)
#FUSES NOWDT         // Watch Dog Timer disabled
#FUSES PUT           // Power Up Timer enabled
#FUSES NOMCLR        // Master Clear pin is used for I/O
#FUSES PROTECT       // Code protected from reads
#FUSES CPD           // Data EEPROM code protected
#FUSES BROWNOUT      // Brownout Reset enabled
#FUSES BORV25        // Brownout Reset at 2.5V
#FUSES NOCLKOUT      // Disable clock output on OSC2
#FUSES NOIESO        // Internal External Switch Over Mode disabled
#FUSES NOFCMEN       // Fail-safe clock monitor disabled
#FUSES WRT           // Program memory write protected                                              
#FUSES NOLVP         // Low Voltage Programming disabled

#USE   DELAY(internal = 32MHz)
#USE   RS232(stream=RS232, baud=38400, xmit=PIN_B5, rcv=PIN_B2, parity=N, bits=8, stop=1)

#DEFINE DRV_STEP           PIN_A0                  // Step output pin to driver (Rising edge causes the indexer to move one step)
#DEFINE DRV_RESET          PIN_A1                  // Reset output pin to driver (Active-high reset input initializes all internal logic and disables the Hbridge outputs. Internal pulldown.)
#DEFINE DRV_DIR            PIN_A4                  // Direction output pin to driver (Logic level, sets the direction of stepping)
#DEFINE SPI_SDO            PIN_A6                  // SPI data output pin
#DEFINE SPI_CS             PIN_A7                  // SPI chip select pin
#DEFINE SPI_SDI            PIN_B1                  // SPI data input pin
#DEFINE SPI_SCK            PIN_B4                  // SPI clock output pin
#DEFINE DRV_STALL          PIN_B6                  // Stall input pin from driver (Internal stall detect mode: logic low when motor stall detected. Pull up mevcut)
#DEFINE DRV_FAULT          PIN_B7                  // Fault input pin from driver (Logic low when in fault condition. Pull up mevcut)

#DEFINE LIMIT_SWITCH       PIN_A2                  // Fault output pin to user
#DEFINE USER_DIR           PIN_A3                  // Direction input pin from user
#DEFINE USER_STEP          PIN_B0                  // Step input pin from user
#DEFINE HOME_SWITCH        PIN_B3                  // Enable input pin from user

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
#BIT PIN_SDO_SELECT     = PIN_APFCON0.6      // SDO pin selection bit 
#BIT PIN_SS_SELECT      = PIN_APFCON0.5      // SS pin selection bit 

unsigned int32 current_distance  = 0;
int1 reg_rs232_message           = 0;
int1 mode                        = 0; // mode = 0: 32x mode = 1: 256x

unsigned int16 pulse_per_turn    = 6400;   //limit bast�g�nda 1cm a�abilmesi i�in gereken pulse
unsigned int16 delay             = 30;
unsigned int16 home_distance     = 4000;

unsigned int16 acc_lim           = 600;             // Number of steps before we hit max speed
unsigned int const periods[600]={
100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
100,100,100,100,100,100,100,100,100,100,100,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,98,98,
98,98,98,98,98,98,98,98,98,98,97,97,97,97,97,97,97,97,97,97,96,96,96,96,96,96,96,96,96,95,95,95,
95,95,95,95,95,94,94,94,94,94,94,94,94,93,93,93,93,93,93,93,92,92,92,92,92,92,92,91,91,91,91,91,
91,90,90,90,90,90,90,89,89,89,89,89,89,88,88,88,88,88,88,87,87,87,87,87,87,86,86,86,86,86,85,85,
85,85,85,85,84,84,84,84,84,83,83,83,83,83,82,82,82,82,82,81,81,81,81,81,80,80,80,80,80,79,79,79,
79,79,78,78,78,78,77,77,77,77,77,76,76,76,76,76,75,75,75,75,74,74,74,74,74,73,73,73,73,72,72,72,
72,71,71,71,71,71,70,70,70,70,69,69,69,69,68,68,68,68,68,67,67,67,67,66,66,66,66,65,65,65,65,64,
64,64,64,63,63,63,63,62,62,62,62,62,61,61,61,61,60,60,60,60,59,59,59,59,58,58,58,58,57,57,57,57,
56,56,56,56,55,55,55,55,54,54,54,54,53,53,53,53,52,52,52,52,51,51,51,51,50,50,50,50,49,49,49,49,
48,48,48,48,47,47,47,47,46,46,46,46,45,45,45,45,44,44,44,44,43,43,43,43,42,42,42,42,41,41,41,41,
40,40,40,40,39,39,39,39,39,38,38,38,38,37,37,37,37,36,36,36,36,35,35,35,35,34,34,34,34,33,33,33,
33,33,32,32,32,32,31,31,31,31,30,30,30,30,30,29,29,29,29,28,28,28,28,27,27,27,27,27,26,26,26,26,
25,25,25,25,25,24,24,24,24,24,23,23,23,23,22,22,22,22,22,21,21,21,21,21,20,20,20,20,20,19,19,19,
19,19,18,18,18,18,18,17,17,17,17,17,16,16,16,16,16,16,15,15,15,15,15,14,14,14,14,14,14,13,13,13,
13,13,13,12,12,12,12,12,12,11,11,11,11,11,11,10,10,10,10,10,10,9,9,9,9,9,9,9,8,8,8,8,8,8,8,7,7,7,
7,7,7,7,7,6,6,6,6,6,6,6,6,5,5,5,5,5,5,5,5,5,4,4,4,4,4,4,4,4,4,4,3,3,3,3,3,3,3,3,3,3,3,3,2,2,2,2,2,
2,2,2,2,2,2,2,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};

// Sets alternative pin functions
void set_pins()
{
   // Set RA6 as SDO pin
   PIN_SDO_SELECT = 1;
   // Set RA5 as SS pin
   PIN_SS_SELECT = 1;
   // Set RB5 as TX pin
}
// Sets SPI parameters
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
// Sets variables to default values 
void set_variables()
{
   output_low(SPI_CS);     // Chip select is active high so keep it low to prevent out-of-sync transaction 
   output_low(DRV_RESET);  // Reset is active high so keep reset pin low to activate driver 
   output_low(DRV_DIR);    // Set default direction
   output_low(DRV_STEP);   // Keep step output low until a step command is received from the user
}

// Writes the given register byte to the driver
void write_register_byte(unsigned int8 reg_byte)
{
   // write the byte to spi buffer
   SPI_SSP1BUF = reg_byte;
   // Wait until the end of the write operation
   while (!SPI_FLAG);
   // Clear the write-completed-flag of the spi module
   SPI_FLAG = 0;
}
// Writes the given register to the driver
void write_register(unsigned int16 reg)
{
   // start spi write operation by setting the chip select port to high
   output_high(SPI_CS);
   // Get and write the MSB of the register
   write_register_byte(make8(reg, 1));
   // Get and write the MSB of the register
   write_register_byte(make8(reg, 0));
   // stop spi write operation by setting the chip select port to low
   output_low(SPI_CS);
   delay_ms(10);
}
// Sets driver parameters
void set_driver()
{
   if(mode == 0)
      write_register(41); //32x microstepping
   else

      write_register(65); //256x microstepping
      
   //write_register(4310);   //high torque
   write_register(4146); //lowtorque
   write_register(8272);
   write_register(12669);
   //write_register(17448);   //slow auto mixed decay when high torque
   write_register(17704); //auto mixed decay when paused when low torque
   write_register(22804);
   write_register(24736);
   //write_register(28672);
   delay_ms(10);
}

// Moves system by given number of steps 
void motion_cycle(int32 target_step)
{
   write_register(4310);   //high torque
   write_register(17448);   //slow auto mixed decay when high torque
   delay_ms(10);
   // System is moving
   putc('M');
   // Laser on
   putc('A');
   unsigned int32 i=0;
   
   if(target_step >= (2*acc_lim))
   {
      target_step = target_step-2*acc_lim;
      for(i=0; i<acc_lim; i++)
      {
         output_high(DRV_STEP);
         delay_us(3);
         output_low(DRV_STEP);
         delay_us(periods[i] + delay - 3);
      }
      for(i=0; i<(target_step); i++)
      {


         output_high(DRV_STEP);
         if(input(LIMIT_SWITCH) == 1)
         {
            // Limit switch is triggered
            putc('L');
            return;            
         }
         delay_us(3);
         output_low(DRV_STEP);
         delay_us(delay - 3);
      }
      
      for(i=acc_lim; i>0; i--)
      {
         output_high(DRV_STEP);
         delay_us(3);
         output_low(DRV_STEP);
         delay_us(periods[i-1] + delay - 3);
      }
   }
   else
   {
      for(i=0; i<target_step; i++)
      {

         output_high(DRV_STEP);
         if(input(LIMIT_SWITCH) == 1)
         {
            // Limit switch is triggered
            putc('L');
            return;            
         }
         delay_us(3);
         output_low(DRV_STEP);
         delay_us(periods[0] - 3);
      }
   } 
   
   // Laser off
   putc('D');
   // Reached to target destination
   putc('R');
   write_register(4146); //lowtorque
   write_register(17704); //auto mixed decay when paused when low torque
}
// Sends system to home position
void homing_cycle()
{ 
   write_register(4310);   //high torque
   write_register(17448);   //slow auto mixed decay when high torque
   putc('M');
   
   output_low(DRV_DIR);
   delay_ms(100); 
   
   unsigned int16 i = 0;
   for(i=0; i<acc_lim; i++)
   {

      output_high(DRV_STEP);
      if(input(LIMIT_SWITCH) == 1)
      {
         // Limit switch is triggered
         putc('L');
         return;            
      }
      delay_us(3);
      output_low(DRV_STEP);
      delay_us(periods[i] + delay - 3);
   }
   while(input(HOME_SWITCH) == 1)
   {

      output_high(DRV_STEP);
      if(input(LIMIT_SWITCH) == 1)
      {
         // Limit switch is triggered
         putc('L');
         return;            
      }
      delay_us(3);
      output_low(DRV_STEP);
      delay_us(delay - 3);
   }
   for(i=acc_lim; i>0; i--)
   {

      output_high(DRV_STEP);
      if(input(LIMIT_SWITCH) == 1)
      {
         // Limit switch is triggered
         putc('L');
         return;            
      }
      delay_us(3);
      output_low(DRV_STEP);
      delay_us(periods[i-1] + delay - 3);
   }
   
   output_high(DRV_DIR);
   delay_ms(500);
   
   for(i=home_distance; i>0; i--)
   {

      output_high(DRV_STEP);
      if(input(LIMIT_SWITCH) == 1)
      {
         // Limit switch is triggered
         putc('L');
         return;            
      }
      delay_us(8);
      output_low(DRV_STEP);
      delay_us(1000);
   }
   
   current_distance = 0;
   write_register(4146); //lowtorque
   write_register(17704); //auto mixed decay when paused when low torque
   putc('R');
}
// Calculates the displacement
void go_position(int32 user_distance)
{
   if(user_distance>current_distance)
   {
      output_high(DRV_DIR);
      motion_cycle(user_distance - current_distance);
   }
   else if(user_distance<current_distance)
   {
      output_low(DRV_DIR);
      motion_cycle(current_distance-user_distance);
   }
   else
   {
      putc('M');
      putc('R');
   }
   
   current_distance = user_distance;
}

// Handles the messages of RS232 connection
void rs232_message()
{
   char command = fgetc(RS232);
   unsigned int i=0;
   unsigned int32 input[7];
      
   switch (command)
   {
      case 'G':   for(i=0; i<7; i++)
                  {
                     input[i]=(unsigned)(fgetc(RS232)-48);
                  }
                  
                  int32 data=1000000*input[0]+100000*input[1]+10000*input[2]+1000*input[3]+100*input[4]+10*input[5]+input[6];
                  
                  go_position(data);
                  break;
      case 'S':   if(mode == 0)
                  {        
                     for(i=0; i<3; i++)
                     {
                        input[i]=(unsigned)(fgetc(RS232)-48);
                     }
                     
                     delay = 100*input[0] + 10*input[1] + input[2];
                     
                     if(delay > 2000)
                        delay =  2000;
                     else if (delay < 30)
                        delay = 30;
                  }
                  break;
      default :   return; 
   }
}

#INT_RDA
void isr_rs232_message()
{
   disable_interrupts(INT_RDA);
   // Receive the RS232 message
   reg_rs232_message = 1;
} 

void main()
{
   // Set I/O states of the ports
   //           76543210                  
   set_tris_a(0b00101100);       
   set_tris_b(0b11001111);

   // Set alternative pin functions
   set_pins();
   // Set SPI parameters
   set_SPI();
   // Set variables to default values
   set_variables();
   // Set driver
   set_driver();
   // Send system to home position
   homing_cycle();
   
   enable_interrupts(global);
   enable_interrupts(INT_RDA);
   while(true)
   {  
      if(input(LIMIT_SWITCH))
      {
         // Disable RS232 receive byte interrupt
         disable_interrupts(INT_RDA);

         output_toggle(DRV_DIR);
         delay_ms(500);
         for(unsigned int16 i=0; i <= pulse_per_turn; i++)
         {
            output_high(DRV_STEP);
            delay_us(10);
            output_low(DRV_STEP);
            delay_us(1000);
         }
         homing_cycle();
         
         clear_interrupt(INT_RDA);
         enable_interrupts(INT_RDA);  
      }
      if(reg_rs232_message)
      {
         reg_rs232_message = 0;
         rs232_message();
         
         clear_interrupt(INT_RDA);
         enable_interrupts(INT_RDA);
      }
   }   
}
