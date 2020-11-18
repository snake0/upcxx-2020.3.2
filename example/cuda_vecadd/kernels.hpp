#ifndef _KERNEL_H
#define _KERNEL_H

void initialize_device_arrays(double *dA, double *dB, int N);

void gpu_vector_sum(double *dA, double *dB, double *dC, int start, int end);

#endif // _KERNEL_H
