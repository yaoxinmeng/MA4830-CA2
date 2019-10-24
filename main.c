#include <stdio.h>
#include <math.h>
#include <string.h>

#define RESOLUTION 65536  // 2^16 bits
#define VOLT_AMP 1        // voltage amplitude of DAC (depends on SFR)
#define N 100             // number of points in waveform
#define BUFFER 50         // length of input string buffer
#define PI 3.14159265359  // value of pi

// waveform generators
void sin_generator(float *A, float amp, float mean);
void square_generator(float *A, float amp, float mean);
void triangle_generator(float *A, float amp, float mean);
void sawtooth_generator(float *A, float amp, float mean);

// command line i/o functions
void help();                          // bring up command menu
int read_waveform_config(float *A);   // read waveform config from command line
int read_waveform_type(void);         // use string compare to find waveform type

// peripherals communication functions
void set_dac(float voltage);            // output points in waveform to DAC


int main(void){
  float sinwave[N];
  if(read_waveform_config(sinwave)){
    int i;
    for (i = 0; i < N; i++){
      printf("%f\n", sinwave[i]);
    }
  }

  return 0;
}


void set_dac(float voltage){
  // calaculate data to send to DAC module
  voltage = voltage/VOLT_AMP;
  int data;
  if (voltage < 0){
    data = (int)(-1 * voltage * RESOLUTION/2);
  }
  else{
    data = (int)(voltage * RESOLUTION/2 + RESOLUTION/2);
  }

  // communicate with DAC module

}

void sin_generator(float *A, float amp, float mean){
  int i;
  for (i = 0; i < N; i++){
    A[i] = mean + amp * sin((i*2*PI) / N);
  }
}

void square_generator(float *A, float amp, float mean){
  int i;
  for (i = 0; i < N; i++){
    if (i < N/2){
      A[i] = -1 * amp + mean;
    }
    else{
      A[i] = amp + mean;
    }
  }
}

void triangle_generator(float *A, float amp, float mean){
  int i;
  for (i = 0; i < N; i++){
    if (i < N/2){
      A[i] = 2*amp / (N/2) * i - amp + mean;
    }
    else{
      A[i] = -2*amp / (N/2) * i + 3*amp + mean;
    }
  }
}

void sawtooth_generator(float *A, float amp, float mean){
  int i;
  for (i = 0; i < N; i++){
    A[i] = 2*amp / N * i - amp + mean;
  }
}

void help(){
  printf("\n************************************************************\n");
  printf("WAVEFORM GENERATOR\n");
  printf("\t-h Help Menu\n");
  printf("\t-w Configure Waveform\n");
  printf("\t-q Quit\n");
  printf("************************************************************\n");
}

int read_waveform_type(void){
  char input[BUFFER];
  printf("Enter the type of waveform (sine, square, triangle or sawtooth): ");
  scanf("%s", input);

  if (strcmp(input, "sine") == 0){
    return 1;
  }
  else if (strcmp(input, "square") == 0){
    return 2;
  }
  else if (strcmp(input, "triangle") == 0){
    return 3;
  }
  else if (strcmp(input, "sawtooth") == 0){
    return 4;
  }
  else{
    // throw error
    return -1;
  }
}

int read_waveform_config(float *A){
  float amp, mean, input;

  printf("\nEnter the amplitude of waveform: ");
  if(scanf("%f", &input) == 1){   // check if input is of float type
    amp = input;

    printf("Enter the mean value of waveform: ");
    if(scanf("%f", &input) == 1){   // check if input is of float type
      mean = input;

      switch(read_waveform_type()){
        case 1:   // sine
          sin_generator(A, amp, mean);
          break;
        case 2:   // square
          square_generator(A, amp, mean);
          break;
        case 3:   // triangle
          triangle_generator(A, amp, mean);
          break;
        case 4:   // sawtooth
          sawtooth_generator(A, amp, mean);
          break;
        default:  // error
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
