// 09September 2005
//******************************************************************************************************
// Performs basic I/O for the Omega PCI-DAS1602 
//
// Demonstration routine to demonstrate pci hardware programming
// Demonstrated the most basic DIO and ADC and DAC functions
// - excludes FIFO and strobed operations 
//
// Note :
//
//			22 Sept 2016 : Restructured to demonstrate Sine wave to DA
//
// G.Seet - 26 August 2005
//******************************************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <hw/pci.h>
#include <hw/inout.h>
#include <sys/neutrino.h>
#include <sys/mman.h>
#include <math.h>
																
#define	INTERRUPT		iobase[1] + 0				// Badr1 + 0 : also ADC register
#define	MUXCHAN			iobase[1] + 2				// Badr1 + 2
#define	TRIGGER			iobase[1] + 4				// Badr1 + 4
#define	AUTOCAL			iobase[1] + 6				// Badr1 + 6
#define 	DA_CTLREG		iobase[1] + 8				// Badr1 + 8

#define	 AD_DATA			iobase[2] + 0				// Badr2 + 0
#define	 AD_FIFOCLR		iobase[2] + 2				// Badr2 + 2

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

#define 	DA_Data			iobase[4] + 0				// Badr4 + 0
#define	DA_FIFOCLR		iobase[4] + 2				// Badr4 + 2
	
int badr[5];															// PCI 2.2 assigns 6 IO base addresses

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	
int main() {
struct pci_dev_info info;
void *hdl;

uintptr_t iobase[6];
uintptr_t dio_in;
uint16_t adc_in1, adc_in2;



unsigned int i,j, k,count,mode=4;
unsigned short chan;

unsigned int data[200],  data1[200], check[200];
float delta,dummy, num_w, point, incre,amp;

printf("\fDemonstration Routine for PCI-DAS 1602\n\n");

memset(&info,0,sizeof(info));
if(pci_attach(0)<0) {
  perror("pci_attach");
  exit(EXIT_FAILURE);
  }

																		/* Vendor and Device ID */
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
 
printf("\nReconfirm Iobase:\n");  						// map I/O base address to user space						
for(i=0;i<5;i++) {												// expect CpuBaseAddress to be the same as iobase for PC
  iobase[i]=mmap_device_io(0x0f,badr[i]);	
  printf("Index %d : Address : %x ", i,badr[i]);
  printf("IOBASE  : %x \n",iobase[i]);
  }													
																		// Modify thread control privity
if(ThreadCtl(_NTO_TCTL_IO,0)==-1) {
  perror("Thread Control");
  exit(1);
  }																											



														//*****************************************************************************
														//Digital Port Functions
														//*****************************************************************************																																	

printf("\nDIO Functions\n");													  										
out8(DIO_CTLREG,0x90);					// Port A : Input,  Port B : Output,  Port C (upper | lower) : Output | Output			

dio_in=in8(DIO_PORTA); 					// Read Port A	
printf("Port A : %02x\n", dio_in);																												
																						
out8(DIO_PORTB, dio_in);					// output Port A value -> write to Port B 		



 														//******************************************************************************
														// ADC Port Functions
														//******************************************************************************
														// Initialise Board								
out16(INTERRUPT,0x60c0);				// sets interrupts	 - Clears			
out16(TRIGGER,0x2081);					// sets trigger control: 10MHz, clear, Burst off,SW trig. default:20a0
out16(AUTOCAL,0x007f);					// sets automatic calibration : default

out16(AD_FIFOCLR,0); 						// clear ADC buffer
out16(MUXCHAN,0x0D00);				// Write to MUX register - SW trigger, UP, SE, 5v, ch 0-0 	
														// x x 0 0 | 1  0  0 1  | 0x 7   0 | Diff - 8 channels
														// SW trig |Diff-Uni 5v| scan 0-7| Single - 16 channels
														
while(1)
{														
//printf("\n\nRead multiple ADC\n");
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
																
if(mode==1)
{
delta=(2.0*(adc_in2/6553.5)*3.142)/200.0;					// increment
for(i=0;i<200;i++) {
  dummy= ((adc_in1/65535.0)*(sinf((float)(i*delta))) + 1.0) * 0x8000 ;
  data[i]= (unsigned) dummy;			// add offset +  scale
  //printf("%d, ", data[i]);
  }

 }
 
if (mode==2)
{
	delta=(2.0*(adc_in2/6553.5)*3.142)/200.0;					// increment
	for(i=0;i<200;i++) {
 	 dummy= ((adc_in1/65535.0)*(sinf((float)(i*delta))) + 1.0) * 0x8000;
 	 if (dummy > 0x8000)
 	 data[i]= ((adc_in1/65535.0)+1)*0x8000;			// add offset +  scale
  	else
 	  data[i]= (1-(adc_in1/65535.0))*0x8000;	
 	  }

}

  if (mode==3)
  {
  	data[0]=0x8000;
 	check[0]=0x0000;
 	
 		delta=(2.0*(adc_in2/6553.5)*3.142)/200.0;					// increment
	
	num_w= adc_in2/6553.5;
	point=200/(2*num_w);
	incre=((adc_in1/65535.0)*0x8000)/point;
	for(i=1;i<200;i++) {
 	 dummy= ((adc_in1/65535.0)*(sinf((float)(i*delta))) + 1.0) * 0x8000;
 	 if (dummy > 0x8000)
 	 check[i]= ((adc_in1/65535.0)+1)*0x8000;			// add offset +  scale
  	else
 	  check[i]= (1-(adc_in1/65535.0))*0x8000;	
 	  
 	 if ( check[i]>0x8000)	data[i] = data[i-1]+ incre;
 	   if ( check[i]<0x8000)	data[i] = data[i-1]- incre;
 	  }
 	  
 	  }
 
 if (mode == 4)
 {
 
 	data1[0]=0x8000;
 	data[0]=0x8000;
 	check[0]=0x0000;
 	
 		delta=(2.0*(adc_in2/6553.5)*3.142)/200.0;					// increment
	
	num_w= adc_in2/6553.5;
	point=200/(1*num_w);
	incre=((adc_in1/65535.0)*0x8000)/point;
	for(i=1;i<200;i++) {
 	 dummy= ((adc_in1/65535.0)*(sinf((float)(i*delta))) + 1.0) * 0x8000;
 	 if (dummy > 0x8000)
 	 check[i]= ((adc_in1/65535.0)+1)*0x8000;			// add offset +  scale
  	else
 	  check[i]= (1-(adc_in1/65535.0))*0x8000;	
 	  
 	 if ( check[i]>0x8000)	data1[i] = data1[i-1]+ 1;
 	   if ( check[i]<0x8000)	data1[i] = data1[i-1]- 1;
 	  }
 	  
 	for(i=1;i<200;i++) {
 	if (data1[i] == 0x8000)
 	data[i] =  0x8000;
 	else
 	data[i] = data[i-1] +incre;
 	}
 	
 	
 
 }
 
 


//*********************************************************************************************
// Output wave
//*********************************************************************************************

for(i=0;i<200;i++) {
	out16(DA_CTLREG,0x0a23);			// DA Enable, #0, #1, SW 5V unipolar		2/6
   	out16(DA_FIFOCLR, 0);					// Clear DA FIFO  buffer
   	out16(DA_Data,(short) data[i]);																																		
   	out16(DA_CTLREG,0x0a43);			// DA Enable, #1, #1, SW 5V unipolar		2/6
  	out16(DA_FIFOCLR, 0);					// Clear DA FIFO  buffer
	out16(DA_Data,(short) data[i]);																																		
  	}
}
  	
//**********************************************************************************************
// Reset DAC to default : 5v
//**********************************************************************************************

out16(DA_CTLREG,(short)0x0a23);	
out16(DA_FIFOCLR,(short) 0);			
out16(DA_Data, 0x8fff);						// Mid range - Unipolar																											
  
out16(DA_CTLREG,(short)0x0a43);	
out16(DA_FIFOCLR,(short) 0);			
out16(DA_Data, 0x8fff);				
																																						
printf("\n\nExit Demo Program\n");
pci_detach_device(hdl);

//**********************************************************************************************

return(0);
}
