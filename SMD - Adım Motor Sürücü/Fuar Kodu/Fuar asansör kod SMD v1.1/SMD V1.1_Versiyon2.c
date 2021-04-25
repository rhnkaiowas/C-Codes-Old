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

#USE   DELAY(clock=32000000)                                         // delay() func. adjusted for 20Mhz Primary Osc.

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

int16 step_count = 0;
int16 const run_lim          = 10;                                  // Duration of the slow motion (per count) before acclerating to high speed  
unsigned int16   const acc_lim          = 550;                                  // Number of steps before we hit max speed. acc=10000 dec=10000 
unsigned int16 const periods[acc_lim] = {2610,2620,2620,2620,2620,2620,2619,2619,2618,2618,2617,2617,2616,2616,2615,
2614,2613,2612,2611,2610,2609,2608,2607,2606,2605,2604,2602,2601,2600,2598,2597,2595,2594,2592,2590,2589,2587,2585,
2583,2581,2579,2577,2575,2573,2571,2569,2567,2565,2562,2560,2558,2555,2553,2550,2548,2545,2543,2540,2537,2534,2532,
2529,2526,2523,2520,2517,2514,2511,2508,2505,2502,2498,2495,2492,2489,2485,2482,2478,2475,2471,2468,2464,2461,2457,
2453,2450,2446,2442,2438,2434,2431,2427,2423,2419,2415,2411,2406,2402,2398,2394,2390,2385,2381,2377,2372,2368,2364,
2359,2355,2350,2346,2341,2336,2332,2327,2322,2318,2313,2308,2303,2298,2293,2289,2284,2279,2274,2269,2264,2259,2253,
2248,2243,2238,2233,2227,2222,2217,2212,2206,2201,2195,2190,2185,2179,2174,2168,2163,2157,2151,2146,2140,2135,2129,
2123,2117,2112,2106,2100,2094,2088,2083,2077,2071,2065,2059,2053,2047,2041,2035,2029,2023,2017,2011,2004,1998,1992,
1986,1980,1974,1967,1961,1955,1949,1942,1936,1930,1923,1917,1910,1904,1898,1891,1885,1878,1872,1865,1859,1852,1846,
1839,1833,1826,1819,1813,1806,1800,1793,1786,1780,1773,1766,1759,1753,1746,1739,1732,1726,1719,1712,1705,1698,1692,
1685,1678,1671,1664,1657,1651,1644,1637,1630,1623,1616,1609,1602,1595,1588,1581,1574,1567,1560,1553,1546,1539,1532,
1525,1518,1511,1504,1497,1490,1483,1476,1469,1462,1455,1448,1441,1434,1427,1420,1413,1406,1398,1391,1384,1377,1370,
1363,1356,1349,1342,1335,1328,1320,1313,1306,1299,1292,1285,1278,1271,1264,1257,1250,1243,1235,1228,1221,1214,1207,
1200,1193,1186,1179,1172,1165,1158,1151,1144,1137,1130,1123,1116,1109,1102,1095,1088,1081,1074,1067,1060,1053,1046,
1039,1032,1025,1018,1011,1004,997,990,984,977,970,963,956,949,943,936,929,922,915,909,902,895,888,882,875,868,861,
855,848,841,835,828,822,815,808,802,795,789,782,776,769,763,756,750,743,737,731,724,718,711,705,699,692,686,680,674,
667,661,655,649,643,637,630,624,618,612,606,600,594,588,582,576,570,564,558,553,547,541,535,529,524,518,512,506,501,
495,490,484,478,473,467,462,456,451,446,440,435,429,424,419,414,408,403,398,393,388,382,377,372,367,362,357,352,348,
343,338,333,328,323,319,314,309,305,300,295,291,286,282,277,273,269,264,260,256,251,247,243,239,235,230,226,222,218,
214,210,207,203,199,195,191,188,184,180,177,173,170,166,163,159,156,152,149,146,143,139,136,133,130,127,124,121,118,
115,112,109,107,104,101,98,96,93,91,88,86,83,81,79,76,74,72,70,68,66,64,62,60,58,56,54,52,51,49,47,46,44,43,41,40,39,
37,36,35,34,33,32,31,30,29,28,27,26,25,25,24,24,23,23,22,22,21,21,21,21,21,21,20

};

int const off_time = 10;                                             // Off time of the pwm signal (should be smaller than period)
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
      restart_wdt();//watchdog s�f�rlanacak
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
   int16 step_count = 0;
   int16 period     = 0;
   int1  running    = 0;
   int   upwards    = 1; 
   while(true)
   {
      if(upwards)
      {
         if(input(LM_UP) == 1)
         {
            
            upwards = 0;
            md_set_state(OFF);
            delay_ms(1000);
            md_set_state(ON);
            delay_ms(1000);
         }
         
         if(input(LM_UP) == 0 && running == 0)
         {
            md_set_direction(UP);
            motion_state = WALK;
         }
         else if(input(LM_UP) && running == 1)
         {
            if(motion_state == ACC || motion_state == RUN)
               motion_state = DEC;
            else if(motion_state == WALK)
               motion_state = STEADY;
         }
      }
      else
      {   
         if(input(LM_DOWN)== 1)
         {
            
            upwards= 1;
            md_set_state(OFF);
            delay_ms(1000);
            md_set_state(ON);
            delay_ms(1000);
         }
         
         if(input(LM_DOWN) == 0 && running == 0)
         {
            md_set_direction(DOWN);
            motion_state = WALK;
         }
         else if(input(LM_DOWN) && running == 1)
         {
            if(motion_state == ACC || motion_state == RUN)
               motion_state = DEC;
            else if(motion_state == WALK)
               motion_state = STEADY;
         }
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
         delay_us(period - off_time + 14);
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
   md_init();           // Initialize motor driver
   motion_cycle();      // Start motion cycle
}
