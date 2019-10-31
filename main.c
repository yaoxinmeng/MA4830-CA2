#include <stdio.h>
#include <math.h>
#include <string.h>

#define RESOLUTION 65536  // 2^16 bits
#define VOLT_AMP 1        // voltage amplitude of DAC (depends on SFR)
#define N 100             // number of points in waveform
#define BUFFER 50         // length of input string buffer
#define PI 3.14159265359  // value of pi

// global variables
float freq;
float wave[N];

// waveform generators
void sin_generator(float amp, float mean);
void square_generator(float amp, float mean);
void triangle_generator(float amp, float mean);
void sawtooth_generator(float amp, float mean);

// command line i/o functions
void print_help();              // bring up command menu
int dread_waveform_config();    // read waveform config from command line
void read_command();            // read commands from help menu

// analog i/o functions

// peripherals communication functions
void set_dac(float voltage);          // output points in waveform to DAC


int main(void){
  print_help();

  while(1){
    // catch kill sig
    read_command();
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
  printf("\n************************************************************\n");
  printf("WAVEFORM GENERATOR\n");
  printf("\t-h Help Menu\n");
  printf("\t-d Configure Waveform from Keyboard\n");
  printf("\t-a Configure Waveform from Analog Board\n");
  printf("\t-q Quit\n");
  printf("************************************************************\n");
}

int dread_waveform_config(){
  float amp, mean, input;
  char string[BUFFER];

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
      }
      else if (strcmp(string, "square") == 0){
        square_generator(amp, mean);
      }
      else if (strcmp(string, "triangle") == 0){
        triangle_generator(amp, mean);
      }
      else if (strcmp(string, "sawtooth") == 0){
        sawtooth_generator(amp, mean);
      }
      else{   // throw error
        printf("Error!");
        return 0;
      }

      return 1;
    }
    else{
      printf("Error!");
      return 0;
    }
  }
  else{
    printf("Error!");
    return 0;
  }
}

void read_command(){
  char input[BUFFER];
  scanf("%s", input);

  // check if input is a command
  if (input[0] == '-'){
    if(input[2] != '\0'){   // check if command is single letter
      printf("Error - \"%s\" is not recognised as a command!\n", input);
    }
    else{
      switch (input[1]){      // check command type
        case 'h':   // help
          print_help();
          break;
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
