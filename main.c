#include <stdio.h>
#include <math.h>

#define RESOLUTION 65536  // 2^16 bits
#define VOLT_AMP 1        // voltage amplitude of DAC (depends on SFR)
#define N 100             // number of points in waveform

void set_dac(float voltage);
void sin_generator(float *A, float amp);
void square_generator(float *A, float amp);
void triangle_generator(float *A, float amp);
void sawtooth_generator(float *A, float amp);

int main(void){
  float sinwave[N];
  sin_generator(sinwave, 1);

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

void sin_generator(float *A, float amp){
  int i;
  for (i = 0; i < N; i++){
    A[i] = amp * sin(i/(2*pi));
  }
}

void square_generator(float *A, float amp){
  int i;
  for (i = 0; i < N; i++){
    if (i < n/2){
      A[i] = -1 * amp;
    }
    else{
      A[i] = amp;
    }
  }
}

void triangle_generator(float *A, float amp){
  int i;
  for (i = 0; i < N; i++){
    if (i < N/2){
      A[i] = 2*amp / (N/2) * i - amp;
    }
    else{
      A[i] = -2*amp / (N/2) * i + 3*amp;
    }
  }
}

void sawtooth_generator(float *A, float amp){
  int i;
  for (i = 0; i < N; i++){
    A[i] = 2*amp / N * i - amp;
  }
}
