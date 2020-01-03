#ifndef LOADER_H
#define LOADER_H

#include "../libwiiu/src/coreinit.h"
#include "../libwiiu/src/types.h"
#include "../libwiiu/src/draw.h"
#include "../libwiiu/src/vpad.h"

// GPU (pm4) constants
#define MEM_SEMAPHORE 0x39 // Sends Signal & Wait semaphores to the Semaphore Block.
#define WAIT_REG_MEM 0x3C // Wait Until a Register or Memory Location is a Specific Value.
#define MEM_WRITE 0x3D // Write DWORD to Memory For Synchronization
#define CP_INTERRUPT 0x40 // Generate Interrupt from the Command Stream
#define SURFACE_SYNC 0x43 // Synchronize Surface or Cache
#define COND_WRITE 0x45 // Conditional Write to Memory or to a Register

#define SIGNAL_SEMAPHORE (6 << 29)
#define WAIT_SEMAPHORE (7 << 29)

// Kernel constants
#define KERNEL_CODE_READ 0xFFF023D4
#define KERNEL_CODE_WRITE 0xFFF023F4

#define KERNEL_SYSCALL_TABLE_1                      0xFFE84C70
#define KERNEL_SYSCALL_TABLE_2                      0xFFE85070
#define KERNEL_SYSCALL_TABLE_3                      0xFFE85470
#define KERNEL_SYSCALL_TABLE_4                      0xFFEAAA60
#define KERNEL_SYSCALL_TABLE_5                      0xFFEAAE60

#define KERNEL_ADDRESS_TABLE                        0xFFEAB7A0
#define KERNEL_DRIVER_PTR                           0xFFEAB530

#define KERNEL_HEAP_VIRT                            0xFF200000
#define KERNEL_HEAP_PHYS                            0x1B800000

#define FIRST_INDEX_OFFSET                          0x00000008

// Misc constants
#define COLOR_CONSTANT                              0x40404040

typedef struct _heap_ctxt_t
{
    uint32_t base;
    uint32_t end;
    int first_index;
    int last_index;
    uint32_t unknown;
} heap_ctxt_t;

typedef struct _heap_block
{
	uint32_t addr;
	uint32_t size;
	uint32_t prev_idx;
	uint32_t next_idx;
} heap_block_t;

typedef struct _OSDriver
{
    char name[0x40];
    uint32_t unknown;
    uint32_t save_area; // 0x1000 byte cross-process memory
    struct _OSDriver *next_drv;
} OSDriver; // Size = 0x4c

uint32_t (*OSDriver_Register)(char *driver_name, uint32_t name_length, void *buf1, void *buf2);
uint32_t (*OSDriver_Deregister)(char *driver_name, uint32_t name_length);
uint32_t (*OSDriver_CopyToSaveArea)(char *driver_name, uint32_t name_length, void *buf1, uint32_t size);

bool (*OSCreateThread)(void *thread, void *entry, int argc, void *args, uint32_t *stack, uint32_t stack_size, int priority, uint16_t attr);
int  (*OSResumeThread)(void *thread);
int  (*OSGetThreadAffinity)(void *thread);
void*(*OSGetCurrentThread)(void);
void (*OSExitThread)(int code);
void (*OSJoinThread)(void *thread, int *ret_code);
int  (*OSIsThreadTerminated)(void *thread);

void (*GX2Init)(void* attr);
int  (*GX2GetMainCoreId)(void);
void (*GX2Shutdown)(void);
void (*GX2Flush)(void);
int  (*GX2DirectCallDisplayList)(void* dl_list, uint32_t size);
int  (*GX2SetSemaphore)(uint64_t *ptr, int signal_or_wait);

int  (*OSScreenPutFontEx)(int bufferNum, unsigned int posX, unsigned int line, const char* buffer);
int  (*OSScreenClearBufferEx)(int bufferNum, uint32_t val);
void (*OSScreenInit)(void);
int  (*OSScreenGetBufferSizeEx)(int bufferNum);
int  (*OSScreenSetBufferEx)(int bufferNum, void* addr);
int  (*OSScreenFlipBuffersEx)(int bufferNum);

void (*DCFlushRange)(void *buffer, uint32_t length);
void (*DCInvalidateRange)(void *buffer, uint32_t length);
void (*DCTouchRange)(void *buffer, uint32_t length);
void*(*OSEffectiveToPhysical)(void *vaddr);
void*(*OSAllocFromSystem)(uint32_t size, int align);
void (*OSFreeToSystem)(void *ptr);
void*(*memset)(void *dst, uint32_t val, uint32_t size);
void*(*memcpy)(void *dst, void *src, uint32_t size);

int  (*IM_Open)();
int  (*IM_Close)(int fd);
int  (*IM_SetDeviceState)(int fd, void *mem, int state, int a, int b);

int  (*OSGetCoreId)();
void (*__PPCExit)();
void (*_Exit)();

int(*VPADRead)(int controller, VPADData *buffer, unsigned int num, int *error);

/* Functions and variable definition */
void _start();

/* NOT OPTIMIZED AT ALL */
uint32_t _byteswap_long(uint32_t val);

void ScreenInit();
void ScreenClear();
void print(char *msg);
void wait();
void main_code(uint32_t useless, uint32_t *packet);
void patch_kernel(OSDriver *driverhax, uint32_t *syscalls);
void GPU_Write32(uint32_t vaddr, uint32_t data);

uint32_t make_pm4_type3_packet_header(uint32_t opcode, uint32_t count);
uint32_t kern_read(const void *addr);
void kern_write(void *addr, uint32_t value);
void memset32(int *p, int v, unsigned int c);

int sy;
uint32_t* framebuffer_drc;
uint32_t  framebuffer_drc_size;

#endif /* LOADER_H */
