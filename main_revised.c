#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>

#define SIZE 64

typedef union {
  float f;
  unsigned int i;
}foo;

int main(int argc, char** argv)
{
  int i, j;
  foo container;
  foo container2;

  float flat[SIZE*SIZE+SIZE];
  float input_a[SIZE][SIZE];
  float input_b[SIZE];
  float output_cpu[SIZE];
 float output_fpga[SIZE];

  // initialization
  FILE *fd;
  fd = fopen("./input.txt", "r");

  unsigned int a;
  i = 0;
  while ( !feof(fd) )
  {
    if (fscanf(fd, "%X", &a) != EOF)
    {
      container.i = a;
      //printf("%f %X\n", container.f, container.i);
      if (i < SIZE)
        //input_a[i/SIZE][i%SIZE] = container.f;
	input_b[i] = container.f;
      else
        //input_b[i - SIZE*SIZE] = container.f;
	input_a[(i/SIZE)-1][i%SIZE] = container.f;

      flat[i] = container.f;
      //printf("%d, %f\n", i, container.f);
      i++;
    }
  }
  fclose(fd);

for(i = 0; i < SIZE; i++)
  output_cpu[i] = 0.0f;

  // computation
  for (i = 0; i < SIZE; i++)
        for(j = 0; j < SIZE; j++)
                output_cpu[i] += input_a[i][SIZE-1-j] * input_b[SIZE-1-j];

  // FPGA offloading
  // memory load
  int foo;
  foo = open("/dev/mem", O_RDWR | O_NONBLOCK);
  float *fpga_bram = mmap(NULL, (SIZE*SIZE+SIZE)* sizeof(float), PROT_WRITE, MAP_SHARED, foo, 0x40000000);
  for (i = 0; i < SIZE*SIZE+SIZE; i++)
  {
    *(fpga_bram + i) = flat[i];
//      printf("fpga_bram address %-10d value: %f\n", i, flat[i]);
  }
 // run
  unsigned int *fpga_ip = mmap(NULL, sizeof(float), PROT_WRITE, MAP_SHARED, foo,
 0x43c00000);
  *fpga_ip = 0x5555;

float before_output[SIZE];
for(i = 0; i < SIZE; i++)
 before_output[i] = *(fpga_bram+i);

  // wait
  while (*fpga_ip == 0x5555);

  // result
for(i = 0; i < SIZE; i++)
  output_fpga[i] = *(fpga_bram + i);


  // display
  printf("%-10s%-10s%-10s%-10s%-10s%-10s\n", "index", "CPU", "CPU(hex)", "FPGA", "FPGA(hex)", "before_calculation");
for(i = 0; i < SIZE; i++){
  container.f = output_fpga[i];
  container2.f = output_cpu[i];
  printf("%-10d%-10f%-10X%-10f%-10X%-10f\n", i, output_cpu[i], container2.i, output_fpga[i], container.i, before_output[i]);
}
  close(foo);


  return 0;
}
