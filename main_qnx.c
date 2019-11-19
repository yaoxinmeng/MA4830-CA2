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

#define N 100             // number of points in waveform
#define BUFFER 50         // length of input string buffer
#define PI 3.14159265359  // value of pi
#define NUM_THREADS 3     // number of threads created

#define	INTERRUPT		iobase[1] + 0				// Badr1 + 0 : also ADC register
#define	MUXCHAN			iobase[1] + 2				// Badr1 + 2
#define	TRIGGER			iobase[1] + 4				// Badr1 + 4
#define	AUTOCAL			iobase[1] + 6				// Badr1 + 6
#define DA_CTLREG		iobase[1] + 8				// Badr1 + 8

#define	AD_DATA			iobase[2] + 0				// Badr2 + 0
#define	AD_FIFOCLR	iobase[2] + 2				// Badr2 + 2

#define	TIMER0			iobase[3] + 0				// Badr3 + 0
#define	TIMER1			iobase[3] + 1				// Badr3 + 1
#define	TIMER2			iobase[3] + 2				// Badr3 + 2
#define	COUNTCTL		iobase[3] + 3				// Badr3 + 3
#define	DIO_PORTA		iobase[3] + 4				// Badr3 + 4
#define	DIO_PORTB		iobase[3] + 5				// Badr3 + 5
#define	DIO_PORTC		iobase[3] + 6				// Badr3 + 6
#define	DIO_CTLREG	iobase[3] + 7				// Badr3 + 7
#define	PACER1			iobase[3] + 8				// Badr3 + 8
#define	PACER2			iobase[3] + 9				// Badr3 + 9
#define	PACER3			iobase[3] + a				// Badr3 + a
#define	PACERCTL		iobase[3] + b				// Badr3 + b

#define DA_DATA			iobase[4] + 0				// Badr4 + 0
#define	DA_FIFOCLR	iobase[4] + 2				// Badr4 + 2

// global variables
int condition = 0;    // convars condition
void *hdl;            // pci attach pointer
uintptr_t iobase[6];  // I/O addresses
int badr[5];          // base addresses
int kill_sig = 0;     // check if SIGINT is sent

// waveform-related vars
float data[N];        // waveform array
float freq, mean, amp;
int mode;

// thread variables
pthread_mutex_t aread_mutex = PTHREAD_MUTEX_INITIALIZER; //aread mutex
pthread_cond_t aread_cond = PTHREAD_COND_INITIALIZER; //aread convar
pthread_t thread[NUM_THREADS];
pthread_attr_t attr;

// waveform generators
void sin_generator();
void square_generator();
void triangle_generator();
void sawtooth_generator();

// command line i/o functions
void print_help();              // bring up command menu
int dread_waveform_config();    // read waveform config from command line
void INThandler(int sig);       // catch SIGINT
void writeFile();                   // write to file

// analog functions
void setup_peripheral();        // setup peripherals

// thread management
void *read_command();           // read commands from help menu
void *aread_waveform_config();  // read wavform params from potentiometers
void *print_wave();             // output to DAC


int main(void){
  int rc, i;

  pthread_mutex_init(&aread_mutex, NULL);

  // normalise data array
  for (i=0; i<N; i++){
    data[i] = 0;
  }

  // setup peripherals
  setup_peripheral();

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

  // catch kill sig
  signal(SIGINT, INThandler);
  while(!kill_sig);

  // initiate exit sequence
  printf("Exiting...\n");
  // close all threads
  for (i=0; i<NUM_THREADS; i++){
    pthread_cancel(thread[i]);
  }

  //detach pci and reset DAC
  out16(DA_CTLREG,(short)0x0a23);
  out16(DA_FIFOCLR,(short) 0);
  out16(DA_DATA, 0x8fff);						// Mid range - Unipolar

  out16(DA_CTLREG,(short)0x0a43);
  out16(DA_FIFOCLR,(short) 0);
  out16(DA_DATA, 0x8fff);
  pci_detach_device(hdl);

  printf("\nExit Success!\n");

  return 0;
}

void sin_generator(){
  int i;
  float delta, dummy;

  delta=(2.0*PI)/N;					// increment
  for(i=0; i<N; i++) {
    dummy = (amp/2*(sinf((float)(i*delta))) + 1 + mean) * 0x8000 ;
    data[i]= (unsigned) dummy;			// add offset +  scale
  }
}

void square_generator(){
  int i;
  float delta, dummy;


	delta=(2.0*3.142)/200.0;					// increment
  for(i=0; i<N; i++) {
    dummy = (amp/2*(sinf((float)(i*delta))) + 1.0) * 0x8000;
  if (dummy > 0x8000)
    data[i]= (amp/2 + mean)*0x8000;			// add offset +  scale
  else
    data[i]= (mean - amp/2)*0x8000;
  }
}

void triangle_generator(){
  float incre;
  int i;

  incre = amp/(N/2) * 0x8000;
  data[0] = 0x8000*(mean+1) + (N/4)*incre;   // start at max point
  for(i=1; i<N; i++) {
    if(i <= N/2)
      data[i] = data[i-1] - incre;
    else
      data[i] = data[i-1] + incre;
  }
}

void sawtooth_generator(){
  float incre;
  int i;

  incre = amp/N * 0x8000;
  data[0] = 0x8000*(mean+1) - (N/2)*incre;  // start at max point
  for(i=1; i<N; i++) {
    data[i] = data[i-1] + incre;
  }
}

void print_help(){
  /* This functions prints out the help menu */
  printf("\n************************************************************\n");
  printf("WAVEFORM GENERATOR\n");
  printf("\t-d Configure Waveform from Keyboard\n");
  printf("\t-a Configure Waveform from Analog Board\n");
  printf("\t-w Write\n");
  printf("\t-q Quit\n");
  printf("************************************************************\n");
}

int dread_waveform_config(){
  /* This function configures waveform with digital input */
  float input;
  char string[BUFFER];
  int success = 0;

  printf("\nEnter the amplitude of waveform (0 to 1): ");
  fflush(stdout);
  if(scanf("%f", &input) == 1 && input <= 1 && input >= 0){   // check if input is of float type
    amp = input;

    printf("Enter the mean value of waveform (-0.5 to 0.5): ");
    fflush(stdout);
    if(scanf("%f", &input) == 1 && input >= -0.5 && input <= 0.5){   // check if input is of float type
      mean = input;

      printf("Enter the frequency of waveform (0.1 to 5Hz): ");
      fflush(stdout);
      if(scanf("%f", &input) == 1 && input >= 1 && input <= 5){   // check if input is of float type
        freq = input;

        printf("Enter the type of waveform (sine, square, triangle or sawtooth): ");
        fflush(stdout);
        scanf("%s", string);

        if (strcmp(string, "sine") == 0){
          mode = 0;
          sin_generator();
          success = 1;
        }
        else if (strcmp(string, "square") == 0){
          mode = 1;
          square_generator();
          success = 1;
        }
        else if (strcmp(string, "triangle") == 0){
          mode = 2;
          triangle_generator();
          success = 1;
        }
        else if (strcmp(string, "sawtooth") == 0){
          mode = 3;
          sawtooth_generator();
          success = 1;
        }
        else{   // throw error
          printf("Error - unrecognised input!\n");
        }
      }
      else{
        printf("Error - unrecognised frequency!\n");
      }
    }
    else{
      printf("Error - unrecognised mean!\n");
    }
  }
  else{
    printf("Error - unrecognised amplitude!\n");
  }

  fflush(stdout);
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
            condition = 0;
            dread_waveform_config();
            break;
          case 'a':   // analog confiq
            pthread_mutex_lock(&aread_mutex);
            condition = 1;
            pthread_cond_signal(&aread_cond);
            pthread_mutex_unlock(&aread_mutex);
            break;
          case 'w':   //write to file
            condition = 0;
            pthread_mutex_lock(&aread_mutex);
            writeFile();
            pthread_mutex_unlock(&aread_mutex);
            break;
          case 'q':   // quit
            // send kill sig
            raise(SIGINT);
            delay(5);
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
  kill_sig = 1;
}

void setup_peripheral(){
  struct pci_dev_info info;

  uintptr_t dio_in;

  unsigned int i;

  printf("\fSetting up connection to PCI-DAS 1602...\n\n");

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
  printf("Port A : %02x\n\n", dio_in);
  out8(DIO_PORTB, dio_in);					// output Port A value -> write to Port B

  // ADC Port Functions
  // Initialise Board
  out16(INTERRUPT,0x60c0);				// sets interrupts	 - Clears
  out16(TRIGGER,0x2081);					// sets trigger control: 10MHz, clear, Burst off,SW trig. default:20a0
  out16(AUTOCAL,0x007f);					// sets automatic calibration : default

  out16(AD_FIFOCLR,0); 						// clear ADC buffer
  out8(DIO_CTLREG,0x90);					// Port A : Input,  Port B : Output,  Port C (upper | lower) : Output | Output
  out16(MUXCHAN,0x0D00);

  printf("Initialising...\n\n");
}

void *print_wave(){
  int i;
  while(1){
    for(i=0; i<N; i++) {
    	out16(DA_CTLREG,0x0a23);			// DA Enable, #0, #1, SW 5V unipolar		2/6
      out16(DA_FIFOCLR, 0);					// Clear DA FIFO  buffer
      out16(DA_DATA,(short) data[i]);
      delay(1000/(N * freq));
    }
  }
}

void *aread_waveform_config(){
  uint16_t adc_in1, adc_in2;
  unsigned int i, count,mode;
  unsigned short chan;

  while(1){
    pthread_mutex_lock(&aread_mutex);
    while(condition == 0) pthread_cond_wait(&aread_cond, &aread_mutex);
    count=0x00;
    while(count <0x02) {
      chan = ((count & 0x0f)<<4) | (0x0f & count);
      out16(MUXCHAN,0x0D00|chan);		// Set channel	 - burst mode off.
      delay(1);				 							// allow mux to settle
      out16(AD_DATA,0); 						// start ADC
      while(!(in16(MUXCHAN) & 0x4000));

	    mode=in8(DIO_PORTA)-240; 				// Read Port A

      if(count == 0x00)
        adc_in1=in16(AD_DATA);
      if(count == 0x01){
        if(mode>7){
          raise(SIGINT);
        }
        else if(mode <= 3){       // A/D 2 controls freq
          adc_in2 = in16(AD_DATA);
          freq = adc_in2/6553.5 + 0.1;  // min freq = 0.1 Hz
        }
        else{                     // A/D 2 controls mean
          adc_in2 = in16(AD_DATA);
          mean = adc_in2/65530.5 - 0.5;   // mean value between -0.5 and 0.5
        }
      }

      fflush(stdout);
      count++;
      delay(5);
    }				// Write to MUX register - SW trigger, UP, DE, 5v, ch 0-7

    // Setup waveform array
    amp = adc_in1/(65635.0);    // A/D 1 controls amplitude
    switch (mode){
      case 0:
      case 4:
        sin_generator();
        break;
      case 1:
      case 5:
        square_generator();
        break;
      case 2:
      case 6:
        triangle_generator();
        break;
      case 3:
      case 7:
        sawtooth_generator();
        break;
      default:
       break;
    }
    pthread_mutex_unlock( &aread_mutex);
  }
}

void writeFile(){
  FILE *fp;
  int i;

  if ((fp = fopen("output.txt","w+")) == NULL) {
    perror("Cannot open"); exit(1);
  }

  // print configurations
  fprintf(fp, "Configurations\n");
  switch (mode){
    case 0:
      fprintf(fp, "Wave Type: sine\n");
      break;
    case 1:
      fprintf(fp, "Wave Type: square\n");
      break;
    case 2:
      fprintf(fp, "Wave Type: triangle\n");
      break;
    case 3:
      fprintf(fp, "Wave Type: sawtooth\n");
      break;
    default:
      fprintf(fp, "Wave Type: ERROR\n");
      break;
  }
  fprintf(fp, "Amplitude: %.2f\n", amp);
  fprintf(fp, "Frequency: %.2f\n", freq);
  fprintf(fp, "Mean: %.2f\n", mean);

  // print data
  fprintf(fp, "\nData points\n");
  for(i=0; i<N; i++)
    fprintf(fp, "Data[%d]: %8d\n", i, data[i]);

  fclose(fp);
}
