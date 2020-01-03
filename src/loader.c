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
	OSDriver_Register = (uint32_t (*)(char* driver_name, uint32_t name_length, void* buffer1, void* buffer2))0x010277B8;
	OSDriver_Deregister = (uint32_t (*)(char* driver_name, uint32_t name_length))0x010277C4;
	OSDriver_CopyToSaveArea = (uint32_t (*)(char* driver_name, uint32_t name_length, void* buffer, uint32_t buffer_size))0x010277DC;

	void *mem = OSAllocFromSystem(0x100, 64);
	memset(mem, 0, 0x100);

	int fd = IM_Open();
	IM_SetDeviceState(fd, mem, 3, 0, 0);
	IM_Close(fd);

	OSFreeToSystem(mem);

	wait();
	wait();

	/* Alloc thread and stack */
	OSContext *thread1 = (OSContext*)OSAllocFromSystem(0x1000, 32);
	uint32_t *tdstack1 = (uint32_t *)OSAllocFromSystem(0x1000, 4);

	/* Shutdown GX2 on its running core */
	OSCreateThread(thread1, GX2Shutdown, 0, NULL, tdstack1 + 0x400, 0x1000, 0, (1 << GX2GetMainCoreId()) | 0x10);
	OSResumeThread(thread1);

	/* Wait a bit */
	wait();

	/* Init GX2 on our core */
	GX2Init(NULL);
	ScreenInit();

	/* HID driver hardcoded address */
	OSDriver *driverhax =  (OSDriver*)0xFF245F3C;

	/* patch HID driver save area */
	GPU_Write32((uint32_t)&driverhax->save_area, KERNEL_SYSCALL_TABLE_5 + (0x34 * 4));

	print("Done patching HID driver");

	uint32_t syscalls[2] = {KERNEL_CODE_READ, KERNEL_CODE_WRITE};
	char drvname[3] = {'H', 'I', 'D'};

	/* patch syscall table */
	OSDriver_CopyToSaveArea(drvname, 3, syscalls, 8);

	/* Map R-X memory as RW- elsewhere */
	kern_write((uint32_t*)KERNEL_ADDRESS_TABLE + 0x12, 0x31000000); // Physical Address
	kern_write((uint32_t*)KERNEL_ADDRESS_TABLE + 0x13, 0x28305800); // Flags (Same as MEM2)

	/*
	*
	* Put your code here
	*
	*/

	print("Press the HOME button to exit !");

	VPADData vpad_data;
	int error = -1;

	while(1)
	{

		VPADRead(0, &vpad_data, 1, &error);
		if (vpad_data.btn_hold & BUTTON_HOME)
			break;
		
	}

	for (int i = 0; i < 2; i++)
	{
		fillScreen(0, 0, 0, 0);
		flipBuffers();
	}

	_Exit();

	while(1);

}

/* NOT OPTIMIZED AT ALL */
uint32_t _byteswap_long(uint32_t val)
{
    val = ((val << 8) & 0xFF00FF00 ) | ((val >> 8) & 0xFF00FF ); 
    return (val << 16) | (val >> 16);
}

/* send a memory write gpu packet */
void GPU_Write32(uint32_t vaddr, uint32_t data)
{
	uint32_t *pm4_packet = (uint32_t *)OSAllocFromSystem(32, 32);
	pm4_packet[0] = make_pm4_type3_packet_header(MEM_WRITE, 4);
	pm4_packet[1] = (uint32_t)OSEffectiveToPhysical((void*)vaddr) & ~3;
	pm4_packet[2] = (1 << 18) | (0 << 17) | (0 << 16) | 0;
	pm4_packet[3] = _byteswap_long(data);
	pm4_packet[4] = 0;
	for (int i = 0; i < 3; ++i)
		pm4_packet[5 + i] = 0x80000000;

	DCFlushRange(pm4_packet, 32);

	GX2DirectCallDisplayList(pm4_packet, 32);
	GX2Flush();

	OSFreeToSystem(pm4_packet);

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

/* Chadderz's kernel read function */
uint32_t kern_read(const void *addr)
{
	uint32_t result;
	asm(
		"li 3,1\n"
		"li 4,0\n"
		"li 5,0\n"
		"li 6,0\n"
		"li 7,0\n"
		"lis 8,1\n"
		"mr 9,%1\n"
		"li 0,0x3400\n"
		"mr %0,1\n"
		"sc\n"
		"nop\n"
		"mr 1,%0\n"
		"mr %0,3\n"
		:	"=r"(result)
		:	"b"(addr)
		:	"memory", "ctr", "lr", "0", "3", "4", "5", "6", "7", "8", "9", "10",
			"11", "12"
	);

	return result;
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