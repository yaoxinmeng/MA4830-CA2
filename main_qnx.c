#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <hw/pci.h>
#include <hw/inout.h>
#include <sys/neutrino.h>
#include <sys/mman.h>
#include <math.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

#define RESOLUTION 65536  // 2^16 bits
#define VOLT_AMP 1        // voltage amplitude of DAC (depends on SFR)
#define N 200             // number of points in waveform
#define BUFFER 50         // length of input string buffer
#define PI 3.14159265359  // value of pi
#define NUM_THREADS 2     // number of threads created

#define	INTERRUPT		iobase[1] + 0				// Badr1 + 0 : also ADC register
#define	MUXCHAN			iobase[1] + 2				// Badr1 + 2
#define	TRIGGER			iobase[1] + 4				// Badr1 + 4
#define	AUTOCAL			iobase[1] + 6				// Badr1 + 6
#define DA_CTLREG		iobase[1] + 8				// Badr1 + 8

#define	AD_DATA			iobase[2] + 0				// Badr2 + 0
#define	AD_FIFOCLR		iobase[2] + 2				// Badr2 + 2

#define	TIMER0				iobase[3] + 0				// Badr3 + 0
#define	TIMER1				iobase[3] + 1				// Badr3 + 1
#define	TIMER2				iobase[3] + 2				// Badr3 + 2
#define	COUNTCTL			iobase[3] + 3				// Badr3 + 3
#define	DIO_PORTA		iobase[3] + 4				// Badr3 + 4
#define	DIO_PORTB		iobase[3] + 5				// Badr3 + 5
#define	DIO_PORTC		iobase[3] + 6				// Badr3 + 6
#define	DIO_CTLREG		iobase[3] + 7				// Badr3 + 7
#define	PACER1				iobase[3] + 8				// Badr3 + 8
#define	PACER2				iobase[3] + 9				// Badr3 + 9
#define	PACER3				iobase[3] + a				// Badr3 + a
#define	PACERCTL			iobase[3] + b				// Badr3 + b

#define DA_DATA				iobase[4] + 0				// Badr4 + 0
#define	DA_FIFOCLR		iobase[4] + 2				// Badr4 + 2

// global variables
float freq;       // frequency
float data[N];    // waveform array
// thread variables
pthread_mutex_t printf_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t thread[NUM_THREADS];
pthread_attr_t attr;
void *hdl;
uintptr_t iobase[6];
int badr[5];

// waveform generators
void sin_generator(float amp, float mean, float freq);
void square_generator(float amp, float mean, float freq);
void triangle_generator(float amp, float mean, float freq);
void sawtooth_generator(float amp, float mean, float freq);

// command line i/o functions
void print_help();              // bring up command menu
int dread_waveform_config();    // read waveform config from command line
void *read_command();            // read commands from help menu
void INThandler(int sig);

// analog functions
void* print_wave();           // output to DAC
void setup_peripheral();      // setup peripherals
void *aread_waveform_config(); // read wavform params from potentiometers

int main(void){
  int rc;
  pthread_mutex_init(&printf_mutex, NULL);

  // initialise threads
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  rc = pthread_create(&thread[0], &attr, &read_command, NULL);
  if(rc){
    printf("ERROR - return code from pthread_create() is %d\n", rc);
    exit(-1);
  }
  rc = pthread_create(&thread[1], &attr, &print_wave, NULL);
  if(rc){
    printf("ERROR - return code from pthread_create() is %d\n", rc);
    exit(-1);
  }
  rc = pthread_create(&thread[2], &attr, &aread_waveform_config, NULL);
  if(rc){
    printf("ERROR - return code from pthread_create() is %d\n", rc);
    exit(-1);
  }

  // setup peripherals
  setup_peripheral();

  while(1){
    // catch kill sig
    signal(SIGINT, INThandler);
  }

  return 0;
}


void set_dac(float voltage){
  int data;
  // calaculate data to send to DAC module
  voltage = voltage/VOLT_AMP;
  if (voltage < 0){
    data = (int)(-1 * voltage * RESOLUTION/2);
  }
  else{
    data = (int)(voltage * RESOLUTION/2 + RESOLUTION/2);
  }

  // communicate with DAC module

}

void sin_generator(float amp, float mean, float freq){
  int i;
  float delta, dummy;

  delta=(2.0*freq*PI)/N;					// increment
  for(i=0;i<N;i++) {
    dummy= (amp*(sinf((float)(i*delta))) + 1.0) * 0x8000 ;
    data[i]= (unsigned) dummy + mean;			// add offset +  scale
    //printf("%d, ", data[i]);
    }
}

void square_generator(float amp, float mean, float freq){
	int i;
  float delta, dummy;
	delta=(2.0*freq*PI)/N;					// increment
	for(i=0;i<N;i++) {
		dummy= (amp*(sinf((float)(i*delta))) + 1.0) * 0x8000;
		if (dummy > 0x8000)
			data[i]= (amp+1)*0x8000 + mean;			// add offset +  scale
		else
			data[i]= (1-amp)*0x8000 + mean;
	}
}

void triangle_generator(float amp, float mean, float freq){
  float check[N], incre, dummy, delta;
  int i;

  check[0] = 0x0000;
  data[0] = 0x8000;
  delta=(2.0*freq*PI)/200.0;					// increment
  incre=(amp*0x8000)/(N/(2*freq));

  for(i=1;i<N;i++) {
    dummy= (amp*(sinf((float)(i*delta))) + 1.0) * 0x8000;
    if (dummy > 0x8000)
      check[i]= (amp+1)*0x8000;			// add offset +  scale
    else
      check[i]= (1-amp)*0x8000;

    if ( check[i]>0x8000)
      data[i] = data[i-1]+ incre + mean;
    if ( check[i]<0x8000)
      data[i] = data[i-1]- incre + mean;
  }
}

void sawtooth_generator(float amp, float mean, float freq){
  float data1[N], check[N], delta, dummy, incre;
  int i;

  data1[0]=0x8000;
  data[0]=0x8000;
  check[0]=0x0000;
  delta=(2.0*freq*PI)/200.0;					// increment
  incre=(amp*0x8000)/(200/(1*freq));

  for(i=1;i<200;i++) {
    dummy= (amp*(sinf((float)(i*delta))) + 1.0) * 0x8000;
    if (dummy > 0x8000)
      check[i]= (amp+1)*0x8000;			// add offset +  scale
    else
      check[i]= (1-amp)*0x8000;

    if ( check[i]>0x8000)
      data1[i] = data1[i-1]+ 1;
    if ( check[i]<0x8000)
      data1[i] = data1[i-1]- 1;
  }

  for(i=1;i<200;i++) {
    if (data1[i] == 0x8000)
      data[i] =  0x8000;
    else
      data[i] = data[i-1] +incre;
  }
}

void print_help(){
  printf("\n************************************************************\n");
  printf("WAVEFORM GENERATOR\n");
  printf("\t-d Configure Waveform from Keyboard\n");
  printf("\t-a Configure Waveform from Analog Board\n");
  printf("\t-q Quit\n");
  printf("************************************************************\n");
  fflush(stdin);
}

int dread_waveform_config(){
  float amp, mean, input;
  char string[BUFFER];
  int success = 0;

  printf("\nEnter the amplitude of waveform: ");
  if(scanf("%f", &input) == 1){   // check if input is of float type
    amp = input;

    printf("Enter the mean value of waveform: ");
    if(scanf("%f", &input) == 1){   // check if input is of float type
      mean = input;

      printf("Enter the type of waveform (sine, square, triangle or sawtooth): ");
      scanf("%s", string);

      if (strcmp(string, "sine") == 0){
        sin_generator(amp, mean, 1);
        success = 1;
      }
      else if (strcmp(string, "square") == 0){
        square_generator(amp, mean, 1);
        success = 1;
      }
      else if (strcmp(string, "triangle") == 0){
        triangle_generator(amp, mean, 1);
        success = 1;
      }
      else if (strcmp(string, "sawtooth") == 0){
        sawtooth_generator(amp, mean, 1);
        success = 1;
      }
      else{   // throw error
        printf("Error - unrecognised input!\n");
      }
    }
    else{
      printf("Error - unrecognised input!\n");
    }
  }
  else{
    printf("Error - unrecognised input!\n");
  }

  fflush(stdin);
  return success;
}

void *read_command(){
  char input[BUFFER];
  while(1){
    // generate command menu and get input
    print_help();
    scanf("%s", input);

    // check if input is a command
    if (input[0] == '-'){
      if(input[2] != '\0'){   // check if command is single letter
        printf("Error - \"%s\" is not recognised as a command!\n", input);
      }
      else{
        switch (input[1]){      // check command type
          case 'd':   // digital config
            // calls and checks in config is sensible
            if(dread_waveform_config()){
              int i;
              for (i = 0; i < N; i++){
                printf("%f\n", data[i]);
              }
            }
            break;
          case 'a':   // analog confiq

            break;
          case 'q':   // quit
            // send kill sig
            raise(SIGINT);
            break;
          default:    // not any of the letters above
            printf("Error - \"%s\" is not recognised as a command!\n", input);
        }
      }
    }
    else {
      printf("Error - \"%s\" is not recognised as a command!\n", input);
    }
  }
}

void INThandler(int sig){
  char c;
  int i;

  printf("Exiting...\n");
  // close all threads
  for (i=0;i<NUM_THREADS;i++){
    pthread_cancel(thread[i]);
  }

  //detach pci and reset DAC
  out16(DA_CTLREG,(short)0x0a23);
  out16(DA_FIFOCLR,(short) 0);
  out16(DA_DATA, 0x8fff);						// Mid range - Unipolar

  out16(DA_CTLREG,(short)0x0a43);
  out16(DA_FIFOCLR,(short) 0);
  out16(DA_DATA, 0x8fff);

  printf("\n\nExit Demo Program\n");
  pci_detach_device(hdl);

  // send kill sig
  signal(sig, SIGINT);
  exit(0);
}

void setup_peripheral(){
  struct pci_dev_info info;

  uintptr_t dio_in;

  unsigned int i;

  printf("\fDemonstration Routine for PCI-DAS 1602\n\n");

  memset(&info,0,sizeof(info));
  if(pci_attach(0)<0) {
    perror("pci_attach");
    exit(EXIT_FAILURE);
  }

  // Vendor and Device ID
  info.VendorId=0x1307;
  info.DeviceId=0x01;

  if ((hdl=pci_attach_device(0, PCI_SHARE|PCI_INIT_ALL, 0, &info))==0) {
    perror("pci_attach_device");
    exit(EXIT_FAILURE);
  }

  // Determine assigned BADRn IO addresses for PCI-DAS1602
  printf("\nDAS 1602 Base addresses:\n\n");
  for(i=0;i<5;i++) {
    badr[i]=PCI_IO_ADDR(info.CpuBaseAddress[i]);
    printf("Badr[%d] : %x\n", i, badr[i]);
  }

  // map I/O base address to user space
  printf("\nReconfirm Iobase:\n");
  for(i=0;i<5;i++) {			// expect CpuBaseAddress to be the same as iobase for PC
    iobase[i]=mmap_device_io(0x0f,badr[i]);
    printf("Index %d : Address : %x ", i,badr[i]);
    printf("IOBASE  : %x \n",iobase[i]);
  }

  // Modify thread control privity
  if(ThreadCtl(_NTO_TCTL_IO,0)==-1) {
    perror("Thread Control");
    exit(1);
  }

  //Digital Port Functions
  printf("\nDIO Functions\n");
  out8(DIO_CTLREG,0x90);					// Port A : Input,  Port B : Output,  Port C (upper | lower) : Output | Output
  dio_in=in8(DIO_PORTA); 					// Read Port A
  printf("Port A : %02x\n", dio_in);
  out8(DIO_PORTB, dio_in);					// output Port A value -> write to Port B

  // ADC Port Functions
  // Initialise Board
  out16(INTERRUPT,0x60c0);				// sets interrupts	 - Clears
  out16(TRIGGER,0x2081);					// sets trigger control: 10MHz, clear, Burst off,SW trig. default:20a0
  out16(AUTOCAL,0x007f);					// sets automatic calibration : default

  out16(AD_FIFOCLR,0); 						// clear ADC buffer
  out16(MUXCHAN,0x0D00);				// Write to MUX register - SW trigger, UP, SE, 5v, ch 0-0
  														// x x 0 0 | 1  0  0 1  | 0x 7   0 | Diff - 8 channels
  														// SW trig |Diff-Uni 5v| scan 0-7| Single - 16 channels

}

void* print_wave(){
  int i;

  while(1){
    for(i=0;i<200;i++) {
    	out16(DA_CTLREG,0x0a23);			// DA Enable, #0, #1, SW 5V unipolar		2/6
      out16(DA_FIFOCLR, 0);					// Clear DA FIFO  buffer
      out16(DA_DATA,(short) data[i]);
      out16(DA_CTLREG,0x0a43);			// DA Enable, #1, #1, SW 5V unipolar		2/6
      out16(DA_FIFOCLR, 0);					// Clear DA FIFO  buffer
    	out16(DA_DATA,(short) data[i]);
    }
  }
}

void *aread_waveform_config(){
  uint16_t adc_in1, adc_in2;
  unsigned int i, count,mode=4;
  unsigned short chan;

  count=0x00;
  while(count <0x02) {
    chan= ((count & 0x0f)<<4) | (0x0f & count);
    out16(MUXCHAN,0x0D00|chan);		// Set channel	 - burst mode off.
    delay(1);				 								// allow mux to settle
    out16(AD_DATA,0); 							// start ADC
    while(!(in16(MUXCHAN) & 0x4000));

    if (count == 0x00)
      adc_in1=in16(AD_DATA);

    if (count == 0x01)
      adc_in2=in16(AD_DATA);

    fflush( stdout );
    count++;
    delay(5);
  }									// Write to MUX register - SW trigger, UP, DE, 5v, ch 0-7

  //**********************************************************************************************
  // Setup waveform array
  //**********************************************************************************************
  switch (mode){
    case 1:
      sin_generator(adc_in1/65535.0, 0, adc_in2/6553.5);
      break;
    case 2:
      square_generator(adc_in1/65535.0, 0, adc_in2/6553.5);
      break;
    case 3:
      triangle_generator(adc_in1/65535.0, 0, adc_in2/6553.5);
      break;
    case 4:
      sawtooth_generator(adc_in1/65535.0, 0, adc_in2/6553.5);
      break;
    default:
      break;
  }

}
