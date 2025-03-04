#INCLUDE <30f6015.h> 
#DEVICE ADC=10                                                    // Configures the read_adc return size as 10 bit
#INCLUDE <math.h>

#FUSES NOWDT                                                      // No Watch Dog Timer 
#FUSES HS2_PLL16                                                  // HS crystal, Freq=16*(Fcryst/2)
#FUSES NOCKSFSM                                                   // Clock Switching is disabled, fail Safe clock monitor is disabled 
#FUSES BROWNOUT                                                   // Reset when brownout detected 
#FUSES NOPROTECT                                                  // Code not protected from reading 
#FUSES NOWRT                                                      // Program memory not write protected 
#FUSES NODEBUG                                                    // No Debug mode for ICD

#USE DELAY(clock=120000000)                                       // delay() func. adjusted for 120Mhz Primary Osc.
#USE RS232(stream=RS485,UART1,baud=38400,ENABLE=PIN_G3,parity=N,bits=8,stop=1)  // Set UART1 as RS485 stream
#USE RS232(stream=RS232,UART2,baud=38400,parity=N,bits=8,stop=1)  // Set UART2 as RS232 stream

// Registers of the UART1 module
#WORD UART_IFS0          = 0x088           // UART Interrupt Flag Status Register
// Bits of the IFS0 register
#BIT UART_IFS0_U1RXIF    = UART_IFS0.9     // UART Receiver Interrupt flag status bit  
#BIT UART_IFS0_U1TXIF    = UART_IFS0.10    // UART Transmiter Interrupt flag status bit  

// Led pins
#DEFINE LED          PIN_B8               // Led used in debugging

#DEFINE FLAG_X       PIN_B0               // Y Motor driver is busy when pin high. Input
#DEFINE FLAG_Y       PIN_B1               // X Motor driver is busy when pin high. Input
//!#DEFINE LASER_FLAG   PIN_D6            //Laser Writes when high. Output
#DEFINE LASER_FLAG   PIN_D10              //Laser Writes when high. Output
#DEFINE RX_Disable   PIN_G2               //If low Receive enabled.
#DEFINE TX_Enable    PIN_G3               //If high Transmit enabbled.

char address = 'x';
char start_bit =  '<';
char stop_bit  =  '>';
char SMD_Uart_READY  = 1;
char start_bit_got=0;
char checking_data=0;
char READY = 1;
char LASER = 0;
float step_for_1um = 5.12;
char Laser_Control=0;

char buffer_485[14];
unsigned int data_485_size=0;

void send_data_single(unsigned int32 data)
{
   fprintf(RS232,"G%07lu", data);
}

void send_data_array(unsigned int32 data)
{
   fprintf(RS232,"G%07lu", data);
   char bytes[8];
   sprintf(bytes, "G%07lu", data);
   unsigned int i=0;
   for(i=0; i<8; i++)
   {
      fputc(bytes[i],RS232);
      delay_ms(1);
   }
}

void send_go(unsigned int32 pos)
{
   SMD_Uart_READY =  0;
   unsigned int32 step = pos * step_for_1um; //needs 1.28 steps to move 10um in 64 microstepping mode
   send_data_single(step);
   
   while(READY);     //yola c�kmas�n� bekle
   //M geldi=yola ��kt�
   while(!READY);    //READY bekle
   //R geldi=Hedefe ulast�
   SMD_Uart_READY =  1;
}

void send_write(unsigned int32 start_pos, unsigned int32 stop_pos)
{
   SMD_Uart_READY =  0;
   unsigned int32 step = start_pos * step_for_1um; //needs 1.28 steps to move 10um in 64 microstepping mode 
   send_data_single(step);
   
   while(READY);     //yola c�kmas�n� bekle
   //M geldi=yola ��kt�
   while(!READY);    //READY bekle
   //R geldi=Hedefe ulast�
   
   delay_ms(1000);
   Laser_Control=1;
   step = stop_pos * step_for_1um;
   send_data_single(step);
   
   while(READY);     //yola c�kmas�n� bekle
   //M geldi=yola ��kt�
   while(!READY);    //READY bekle  
   //R geldi=Hedefe ulast�
   Laser_Control=0;
   SMD_Uart_READY =  1;
}

void send_speed(unsigned int16 delay)
{
   fprintf(RS232,"S%03u", delay);
}

void check_rs485_message()
{
      unsigned int i=0;
      switch (buffer_485[1])     //P ya da W ya da S?
      {
         case 'p':   if(data_485_size==8)
                     {
                        unsigned int32 input_p[6];
                        for(i=0; i<6; i++)
                        {
                           input_p[i]=(unsigned)buffer_485[i+2]-48;
                        }
                        unsigned int32 pos = 100000*input_p[0]+10000*input_p[1]+1000*input_p[2]+100*input_p[3]+10*input_p[4]+1*input_p[5];
                        if(pos <= 350000)
                        {
                           if(SMD_Uart_READY)
                           {
                              send_go(pos);
                           }
                        }
                     }  
                     break;
                     
         case 'w':   if(data_485_size==14)
                     {
                        unsigned int32 input_w[12];
                        for(i=0; i<12; i++)
                        {
                           input_w[i]=(unsigned)buffer_485[i+2]-48;
                        }
                        unsigned int32 start_pos = 100000*input_w[0]+10000*input_w[1]+1000*input_w[2]+100*input_w[3]+10*input_w[4]+1*input_w[5];
                        unsigned int32 stop_pos  = 100000*input_w[6]+10000*input_w[7]+1000*input_w[8]+100*input_w[9]+10*input_w[10]+1*input_w[11];
                        if(start_pos <= 350000 && stop_pos <= 350000)
                        {
                           if(SMD_Uart_READY)
                           {
                              send_write(start_pos, stop_pos);  
                           }   
                        }
                     }  
                     break;
                     
         case 's':   if(data_485_size==5)
                     {
                        unsigned int input_s[3];
                        for(i=0; i<3; i++)
                        {
                           input_s[i]=(unsigned)buffer_485[i+2]-48;
                        }
                        unsigned int16 delay = 100*input_s[0]+10*input_s[1]+1*input_s[2];
                        if(delay >= 14)
                        {
                           if(SMD_Uart_READY)
                           {
                              send_speed(delay);;  
                           }   
                        }
                     }  
                     break;
                     
         case 'l':   if(data_485_size==3)
                     {
                        if(buffer_485[2]-48==1)
                        {
                           //laser_a�
                           output_high(Laser_Flag);
                           output_high(LED);
                        }
                        else if(buffer_485[2]-48==0)
                        {
                           //laser_kapat
                           output_low(Laser_Flag);
                           output_low(LED);
                        }
                        
                     }  
                     break;
                     
         default :   return;
      }
}

// RS232 receive byte interrupt
#INT_RDA2
void isr_rs232_message()
{
      char input = fgetc(RS232);
      if(input=='M')      //moving=motor driver is busy
      {
         READY=0;
      }
      else if(input=='A')      //accel completed= laser should turn on
      {
         if(Laser_Control==1)
         {
            LASER=1;
            output_high(Laser_Flag);
            output_high(LED);
         }
         
      }
      else if(input=='D')      //Decel started=laser should turn off
      {
         if(Laser_Control==1)
         {
            LASER=0;
            output_low(Laser_Flag);
            output_low(LED);
         }
      }
      else if(input=='R')      //Reached to destination=now ready
      {
         READY=1;
      }   
      else if(input=='L')      //Limit triggered=emergency stop
      {
         LASER=0;
         output_low(Laser_Flag);
         output_low(LED);
      }     
}

// RS485 receive byte interrupt
#INT_RDA
void isr_rs485_message()
{
   // Receive the RS485 message
   char input = fgetc(RS485);
   if(checking_data==0)
   {
      if(input == start_bit) 
      {  
         //start biti geldi, datalar� buffera dizmeye baslayal�m
         start_bit_got=1;
         data_485_size=0;
      }
      else if(input == stop_bit)
      {
         //stop biti geldi. �imdi datay� i�leme alal�m burada i�lenecek
         start_bit_got=0;
         if(buffer_485[0]==address)
         {
            checking_data=1;
         }
      }
      else if(start_bit_got)
      {  
         //start biti gelmi�ti. Gelen datay� buffer'a dizelim
         buffer_485[data_485_size]=input;
         data_485_size++;
      }
   }
} 

// Main method
void main()
{
   // Set I/O states of the ports
   //           FEDCBA9876543210
   set_tris_b(0b0000000011111111);
   set_tris_c(0b1111111111111111);
   set_tris_d(0b1111101100111111);
   set_tris_e(0b1111111110000000);
   set_tris_f(0b1111111111111100);
   set_tris_g(0b1111111100110011);
   
   // Turn on debug led
   output_low(LED);
   output_low(RX_Disable);
   output_low(LASER_FLAG);
   // Enable RS485 receive byte interrupt
   enable_interrupts(INT_RDA);
   enable_interrupts(INT_RDA2);

   while(true)
   {
      if(checking_data)
      {
         check_rs485_message();
         checking_data=0;
      }
   }
}
