#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

#define MSR_EFER     0xC0000080
#define MSR_STAR     0xC0000081
#define MSR_LSTAR    0xC0000082
#define MSR_SFMASK   0xC0000084

#define EFER_SCE     1 // System Call Extensions

void Syscall_Init();

// Known Sycalls
#define SYSCALL_PRINT 1

#endif
