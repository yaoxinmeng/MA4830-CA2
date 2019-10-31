#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

#define RESOLUTION 65536  // 2^16 bits
#define VOLT_AMP 1        // voltage amplitude of DAC (depends on SFR)
#define N 100             // number of points in waveform
#define BUFFER 50         // length of input string buffer
#define PI 3.14159265359  // value of pi
#define NUM_THREADS 5     // number of threads created

// global variables
float freq;       // frequency
float wave[N];    // waveform array
// waveform configuration thread vars
pthread_mutex_t printf_mutex = PTHREAD_MUTEX_INITIALIZER;

// waveform generators
void sin_generator(float amp, float mean);
void square_generator(float amp, float mean);
void triangle_generator(float amp, float mean);
void sawtooth_generator(float amp, float mean);

// command line i/o functions
void print_help();              // bring up command menu
int dread_waveform_config();    // read waveform config from command line
void *read_command();            // read commands from help menu
void INThandler(int sig);

// analog i/o functions

// peripherals communication functions
void set_dac(float voltage);          // output points in waveform to DAC


int main(void){
  // thread variables
  pthread_t thread[NUM_THREADS];
  pthread_attr_t attr;
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

void sin_generator(float amp, float mean){
  int i;
  for (i = 0; i < N; i++){
    wave[i] = mean + amp * sin((i*2*PI) / N);
  }
}

void square_generator(float amp, float mean){
  int i;
  for (i = 0; i < N; i++){
    if (i < N/2){
      wave[i] = -1 * amp + mean;
    }
    else{
      wave[i] = amp + mean;
    }
  }
}

void triangle_generator(float amp, float mean){
  int i;
  for (i = 0; i < N; i++){
    if (i < N/2){
      wave[i] = 2*amp / (N/2) * i - amp + mean;
    }
    else{
      wave[i] = -2*amp / (N/2) * i + 3*amp + mean;
    }
  }
}

void sawtooth_generator(float amp, float mean){
  int i;
  for (i = 0; i < N; i++){
    wave[i] = 2*amp / N * i - amp + mean;
  }
}

void print_help(){
  pthread_mutex_lock(&printf_mutex);
  printf("\n************************************************************\n");
  printf("WAVEFORM GENERATOR\n");
  printf("\t-d Configure Waveform from Keyboard\n");
  printf("\t-a Configure Waveform from Analog Board\n");
  printf("\t-q Quit\n");
  printf("************************************************************\n");
  pthread_mutex_unlock(&printf_mutex);
}

int dread_waveform_config(){
  float amp, mean, input;
  char string[BUFFER];
  int success = 0;

  pthread_mutex_lock(&printf_mutex);
  printf("\nEnter the amplitude of waveform: ");
  if(scanf("%f", &input) == 1){   // check if input is of float type
    amp = input;

    printf("Enter the mean value of waveform: ");
    if(scanf("%f", &input) == 1){   // check if input is of float type
      mean = input;

      printf("Enter the type of waveform (sine, square, triangle or sawtooth): ");
      scanf("%s", string);

      if (strcmp(string, "sine") == 0){
        sin_generator(amp, mean);
        success = 1;
      }
      else if (strcmp(string, "square") == 0){
        square_generator(amp, mean);
        success = 1;
      }
      else if (strcmp(string, "triangle") == 0){
        triangle_generator(amp, mean);
        success = 1;
      }
      else if (strcmp(string, "sawtooth") == 0){
        sawtooth_generator(amp, mean);
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

  pthread_mutex_unlock(&printf_mutex);
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
                printf("%f\n", wave[i]);
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

  printf("Exiting...\n");
  signal(sig, SIGINT);
  exit(0);
}
