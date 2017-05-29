#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>

#define SIZE 64

typedef union {
  float f;
  unsigned int i;
}foo;

struct timeval st[10];

int st2time (struct timeval st_) 
{
  return st_.tv_sec * 1000 * 1000 + st_.tv_usec;
}

int main(int argc, char** argv)
{
  int i, j;
  foo container;

  float flat[SIZE * (SIZE + 1)];
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
      flat[i] = container.f;
      i++;
    }
  }
  fclose(fd);

  // memory load
  int foo;
  foo = open("/dev/mem", O_RDWR);
  float *ps_dram = mmap(NULL, (SIZE * (SIZE + 1))* sizeof(float), PROT_READ|PROT_WRITE, MAP_SHARED, foo, 0x10000000);

  // MCPY1: DRAM -> non-cacheable DRAM
  gettimeofday (&st[0], NULL);
  memcpy( ps_dram, flat, SIZE * (SIZE + 1) * sizeof(float) );
  //for (i = 0; i < SIZE * (SIZE + 1); i++)
  //{
  //  *(ps_dram + i) = flat[i];
  //
  gettimeofday (&st[1], NULL);

  // DMA1 : DRAM -> BRAM
  unsigned int *fpga_dma = mmap(NULL, 16*sizeof(unsigned int), PROT_READ|PROT_WRITE, MAP_SHARED, foo, 0x7E200000);
  
  gettimeofday (&st[2], NULL);
  *(fpga_dma+6) = 0x10000000; // Source Address
  *(fpga_dma+8) = 0xC0000000; // Destination Address
  *(fpga_dma+10) = SIZE * (SIZE + 1) * sizeof(float); // 4160 data
  while ((*(fpga_dma+1) & 0x00000002) == 0); // DMA execution
  gettimeofday (&st[3], NULL);

  // Check if the data is valid
  float *fpga_bram = mmap(NULL, (SIZE * (SIZE + 1))* sizeof(float), PROT_READ|PROT_WRITE, MAP_SHARED, foo, 0x40000000);
  
  int num_mismatch = 0;
  for (i = 0; i < SIZE * (SIZE + 1); i++)
  {
    if ( *(fpga_bram + i) != *(ps_dram + i) )
    {
      printf("%f, %f\n", *(fpga_bram + i), *(ps_dram + i) );
      num_mismatch++;
    }
  }
  
  printf("The number of mismatch (dram <-> bram) : %d\n", num_mismatch);


  // M*V run
/*
  unsigned int *fpga_ip = mmap(NULL, sizeof(float), PROT_READ|PROT_WRITE, MAP_SHARED, foo, 0x43c00000);

  *fpga_ip = 0x5555; 
  // wait
gettimeofday (&st[4], NULL); 
  while (*fpga_ip == 0x5555);
gettimeofday (&st[5], NULL);
*/


  // DMA2: BRAM -> non-cacheable DRAM
gettimeofday (&st[6], NULL);
  *(fpga_dma+6) = 0xC0000000;
  *(fpga_dma+8) = 0x10000000;
  *(fpga_dma+10) = SIZE * sizeof(float);
  while ((*(fpga_dma+1) & 0x00000002) == 0);
gettimeofday (&st[7], NULL);

  // MCPY2: non-cacheable DRAM -> output_fpga[]
  gettimeofday (&st[8], NULL);
  memcpy(output_fpga, ps_dram, SIZE * sizeof(float) );
  gettimeofday (&st[9], NULL);

/*
  // display
  printf("%-10s%-10s%-10s%-10s\n", "index", "CPU", "FPGA", "FPGA(hex)");
  for (i = 0; i < SIZE; i++)
  {
    container.f = output_fpga[i];
    printf("%-10d%-10f%-10f%-10X\n", i, output[i], output_fpga[i], container.i);
  }
*/

  printf("%s: %d\n", "MCPY1", st2time(st[ 1]) - st2time(st[ 0]));
  printf("%s: %d\n", "DMA1", st2time(st[ 3]) - st2time(st[ 2]));
  printf("%s: %d\n", "M*V", st2time(st[ 5]) - st2time(st[ 4]));
  printf("%s: %d\n", "DMA2", st2time(st[ 7]) - st2time(st[ 6]));
  printf("%s: %d\n", "MCPY2", st2time(st[9]) - st2time(st[8]));
  
  close(foo);

  return 0;
}
