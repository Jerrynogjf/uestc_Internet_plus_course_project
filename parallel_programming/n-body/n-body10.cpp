#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "timer.h"
#include "check.h"

#define SOFTENING 1e-9f
#define BLOCK_SIZE 64
#define MAX_BLOCK_SIZE 512

/*
 * Each body contains x, y, and z coordinate positions,
 * as well as velocities in the x, y, and z directions.
 */

typedef struct { float x, y, z, vx, vy, vz; } Body;

/*
 * Do not modify this function. A constraint of this exercise is
 * that it remain a host function.
 */

void randomizeBodies(float *data, int n) {
  for (int i = 0; i < n; i++) {
    data[i] = 2.0f * (rand() / (float)RAND_MAX) - 1.0f;
  }
}

/*
 * This function calculates the gravitational impact of all bodies in the system
 * on all others, but does not update their positions.
 */

__device__ 
void printNumber(int number)
{
  printf("%d\n", number);
}

__global__ 
void bodyForce(Body *p, float dt, int n) {
  int mutiple = MAX_BLOCK_SIZE / BLOCK_SIZE;
  int index = threadIdx.x + blockIdx.x * blockDim.x;
  int num = threadIdx.x % mutiple;
  __shared__ float lgpx[BLOCK_SIZE], lgpy[BLOCK_SIZE], lgpz[BLOCK_SIZE];
  float Fx = 0.0f; float Fy = 0.0f; float Fz = 0.0f;
  float lgpx0 = p[index / mutiple].x, lgpy0 = p[index / mutiple].y, lgpz0 = p[index / mutiple].z;
  // share memory
  for (unsigned int i = 0; i < n; i+=BLOCK_SIZE){
    if (threadIdx.x % mutiple == 0)
    {
      lgpx[threadIdx.x / mutiple] = p[ i + threadIdx.x / mutiple].x;
      lgpy[threadIdx.x / mutiple] = p[ i + threadIdx.x / mutiple].y;
      lgpz[threadIdx.x / mutiple] = p[ i + threadIdx.x / mutiple].z;
    }
    __syncthreads();
    #pragma unroll
    for (unsigned int j = 0; j < BLOCK_SIZE / mutiple; j++) {
      float dx = ( lgpx[j * mutiple + num] - lgpx0 );
      float dy = ( lgpy[j * mutiple + num] - lgpy0 );
      float dz = ( lgpz[j * mutiple + num] - lgpz0 );
      float distSqr = dx*dx + dy*dy + dz*dz + SOFTENING;
      float invDist = rsqrtf(distSqr);
      float invDist3 = invDist * invDist * invDist;
      Fx += dx * invDist3; 
      Fy += dy * invDist3; 
      Fz += dz * invDist3;
    }
    __syncthreads();
  }
   atomicAdd(&p[index / mutiple].vx, dt*Fx); 
   atomicAdd(&p[index / mutiple].vy, dt*Fy); 
   atomicAdd(&p[index / mutiple].vz, dt*Fz);
}

__global__ void nextPos(Body *p, float dt, int n){
  int index = threadIdx.x + blockIdx.x * blockDim.x;
  if (index < n){
    p[index].x += p[index].vx*dt;
    p[index].y += p[index].vy*dt; 
    p[index].z += p[index].vz*dt;
  }
}

int main(const int argc, const char** argv) {

  /*
   * Do not change the value for `nBodies` here. If you would like to modify it,
   * pass values into the command line.
   */

  int nBodies = 2<<11;
  int salt = 0;
  if (argc > 1) nBodies = 2<<atoi(argv[1]);

  /*
   * This salt is for assessment reasons. Tampering with it will result in automatic failure.
   */

  if (argc > 2) salt = atoi(argv[2]);

  const float dt = 0.01f; // time step
  const int nIters = 10;  // simulation iterations

  int bytes = nBodies * sizeof(Body);
  float *buf;
  cudaMallocHost(&buf, bytes); 

  /*
   * As a constraint of this exercise, `randomizeBodies` must remain a host function.
   */

  randomizeBodies(buf, 6 * nBodies); // Init pos / vel data

  double totalTime = 0.0;

  float *d_buf;
  cudaMalloc(&d_buf, bytes);
  Body *d_p = (Body*) d_buf;

  int nBlocks = (nBodies + BLOCK_SIZE - 1) / BLOCK_SIZE;
  /*
   * This simulation will run for 10 cycles of time, calculating gravitational
   * interaction amongst bodies, and adjusting their positions to reflect.
   */
  cudaMemcpy(d_buf, buf, bytes, cudaMemcpyHostToDevice);

  /*******************************************************************/
  // Do not modify these 2 lines of code.
  for (int iter = 0; iter < nIters; iter++) {
    StartTimer();

  /*******************************************************************/
  /*
   * You will likely wish to refactor the work being done in `bodyForce`,
   * as well as the work to integrate the positions.
   */
   bodyForce<<<nBlocks, MAX_BLOCK_SIZE>>>(d_p, dt, nBodies); // compute interbody forces

  /*
   * This position integration cannot occur until this round of `bodyForce` has completed.
   * Also, the next round of `bodyForce` cannot begin until the integration is complete.
   */
   nextPos<<<nBlocks, BLOCK_SIZE>>>(d_p, dt, nBodies);

   if (iter == nIters - 1)
   {
     cudaMemcpy(buf, d_buf, bytes, cudaMemcpyDeviceToHost);   
   }

  /*******************************************************************/
  // Do not modify the code in this section.
   const double tElapsed = GetTimer() / 1000.0;
   totalTime += tElapsed;
 }
 double avgTime = totalTime / (double)(nIters);
 float billionsOfOpsPerSecond = 1e-9 * nBodies * nBodies / avgTime;

#ifdef ASSESS
 checkPerformance(buf, billionsOfOpsPerSecond, salt);
#else
 checkAccuracy(buf, nBodies);
 printf("%d Bodies: average %0.3f Billion Interactions / second\n", nBodies, billionsOfOpsPerSecond);
 salt += 1;
#endif
  /*******************************************************************/

  /*
   * Feel free to modify code below.
   */

 cudaFreeHost(buf);
 cudaFree(d_buf);
}