#include "fpga_api.h"
#include <cstring>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define DATA_SIZE SIZE*(SIZE+1)*sizeof(float) // fpga bram data size

#define min(x,y) (((x)<(y))?(x):(y))

FPGA::FPGA(off_t data_addr, off_t api_addr)
{
    fd_ = open("/dev/mem", O_RDWR);
    data_ = static_cast<float*>(mmap(NULL, DATA_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd_, data_addr));
    api_ = static_cast<unsigned int*>(mmap(NULL, sizeof(unsigned int), PROT_READ|PROT_WRITE, MAP_SHARED,fd_, api_addr));
}

FPGA::~FPGA()
{
    munmap(data_, DATA_SIZE );
    munmap(api_, sizeof(unsigned int));
    close(fd_);
}

float* FPGA::matrix(void)
{
	return data_ + SIZE;
}

float* FPGA::vector(void)
{
	return data_;
}

const float* __attribute__((optimize("O0"))) FPGA::run()
{
    *api_ = 0x5555;
    while(*api_ == 0x5555);

    return data_;    
}

void FPGA::largeMV(const float* large_mat, const float* input,
		float* output, int M, int N)
{
	float* vec = this->vector(); // vector size 64
    	float* mat = this->matrix(); // matrix size 64 * 64
	const float* output_fpga;

	int horizon_mult = (M%64==0)? M/64 : M/64 + 1;
	int vertical_mult = (N%64==0)? N/64 : N/64 + 1;

	float* dummy = (float*) calloc(SIZE, sizeof(float));

	for(int i = 0; i < vertical_mult; i++){
	  for(int j = 0; j < horizon_mult; j++){
	    if(j != M/64)
	    	memcpy(vec, input + SIZE * j, sizeof(float)*SIZE);
	    else{
		memcpy(vec, input + SIZE * j, sizeof(float)*(M%64));
		memcpy(vec+M%64, dummy, sizeof(float)*(SIZE-M%64));
	    }

	    if(j!=M/64 && i!=N/64)
	    	for(int k = 0; k < SIZE; k++)
		  memcpy(mat + SIZE*k, large_mat + M*SIZE*i + M*k+SIZE*j, sizeof(float)*SIZE);
	    else{
		if(j==M/64 && i==N/64){
		  for(int k = 0; k < N%64; k++)
		    memcpy(mat + SIZE*k, large_mat + M*SIZE*i + M*k+SIZE*j, sizeof(float)*(M%64));
		  for(int k = 0; k < N%64; k++)
		    memcpy(mat + M%64 + SIZE*k, dummy, sizeof(float)*(SIZE-M%64));
		  for(int k = N%64; k < SIZE; k++)
		    memcpy(mat + SIZE*k, dummy, sizeof(float)*SIZE);
		}
		else if(j==M/64){
		  for(int k = 0; k < SIZE; k++)
		    memcpy(mat + SIZE*k, large_mat + M*SIZE*i + M*k+SIZE*j, sizeof(float)*(M%64));
		  for(int k = 0; k < SIZE; k++)
		    memcpy(mat + M%64 + SIZE*k, dummy, sizeof(float)*(SIZE-M%64));
		}
		else if(i==N/64){
		  for(int k = 0; k < N%64; k++)
		    memcpy(mat + SIZE*k, large_mat + M*SIZE*i + M*k+SIZE*j, sizeof(float)*SIZE);
		  for(int k = N%64; k < SIZE; k++)
		    memcpy(mat + SIZE*k, dummy, sizeof(float)*SIZE);

		}
	    }

	    output_fpga = this->run();	
	if(i != N/64)
	    for(int k = 0; k < SIZE; k++)
		output[k+SIZE*i] += output_fpga[k];
	else
	    for(int k = 0; k < N%64; k++)
		output[k+SIZE*i] += output_fpga[k];
	  }	
	}			

}
