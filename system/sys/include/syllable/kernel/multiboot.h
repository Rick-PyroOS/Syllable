#ifndef __F_KERNEL_MULTIBOOT_H__
#define __F_KERNEL_MULTIBOOT_H__

#include <kernel/types.h>

typedef struct
{
    /* the memory used goes from bytes 'mod_start' to 'mod_end-1' inclusive */
    char*	  bm_pStart;
    char*	  bm_pEnd;

    /* Module command line */
    const char*   bm_pzModuleArgs;

    /* padding to take it to 16 bytes (must be zero) */
    uint32	  bm_pad;
} MBBootModule_s;


/*
 *  INT-15, AX=E820 style "AddressRangeDescriptor"
 *  ...with a "size" parameter on the front which is the structure size - 4,
 *  pointing to the next one, up until the full buffer length of the memory
 *  map has been reached.
 */

typedef struct
{
    uint32 mm_nSize;
    uint64 mm_nBaseAddr;
    uint64 mm_nLength;
    uint32 mm_nType;

      /* unspecified optional padding... */
} MBMemoryMapEntry_s;

/* usable memory "Type", all others are reserved.  */
#define MB_MM_MEMORY	1
#define MB_MM_RESERVED	2
#define MB_MM_ACPI_DATA	3
#define MB_MM_ACPI_NVS	4

/*
 *  MultiBoot Info description
 *
 *  This is the struct passed to the boot image.  This is done by placing
 *  its address in the EBX register.
 */

typedef struct
{
    /* MultiBoot info version number */
    uint32 		mbh_nFlags;
 
    /* Available memory from BIOS */
    uint32 		mem_lower;
    uint32 		mem_upper;

    /* "root" partition */
    uint32		mbh_nBootDevice;

    /* Kernel command line */
    const char* 	mbh_pzKernelParams;

    /* Boot-Module list */
    uint32 		mbh_nModuleCount;
    MBBootModule_s*	mbh_psFirstModule;

	      /* (ELF) Kernel section header table */
    uint32 num;
    uint32 size;
    uint32 addr;
    uint32 shndx;

    /* Memory Mapping buffer */
    uint32 mbh_nMemoryMapLength;
    uint32 mbh_nMemoryMapAddr;

    /* The rest is only valid with the matching flag (MB_INFO_*).  GRUB
       legacy's info block ends with the VBE fields. */
    uint32 mbh_nDrives_length;
    uint32 mbh_nDrivesAddr;
    uint32 mbh_nConfigTable;
    uint32 mbh_nBootLoaderName;
    uint32 mbh_nApmTable;

    uint32 mbh_nVbeControlInfo;
    uint32 mbh_nVbeModeInfo;
    uint16 mbh_nVbeMode;
    uint16 mbh_nVbeInterfaceSeg;
    uint16 mbh_nVbeInterfaceOff;
    uint16 mbh_nVbeInterfaceLen;

    /* The video mode the kernel starts in (MB_INFO_FRAMEBUFFER) */
    uint64 mbh_nFramebufferAddr;
    uint32 mbh_nFramebufferPitch;
    uint32 mbh_nFramebufferWidth;
    uint32 mbh_nFramebufferHeight;
    uint8  mbh_nFramebufferBpp;
    uint8  mbh_nFramebufferType;	/* MB_FRAMEBUFFER_TYPE_* */
    uint16 mbh_nFramebufferPad;	/* GRUB aligns what follows (a union with a uint32) */
    /* for MB_FRAMEBUFFER_TYPE_RGB: bit position and size of each colour */
    uint8  mbh_nRedPos;
    uint8  mbh_nRedSize;
    uint8  mbh_nGreenPos;
    uint8  mbh_nGreenSize;
    uint8  mbh_nBluePos;
    uint8  mbh_nBlueSize;
} __attribute__ ((packed)) MultiBootHeader_s;

#define MB_FRAMEBUFFER_TYPE_INDEXED  0
#define MB_FRAMEBUFFER_TYPE_RGB      1
#define MB_FRAMEBUFFER_TYPE_EGA_TEXT 2

/*
 *  Flags to be set in the 'flags' parameter above
 */


#define MB_INFO_MEMORY          0x1 /* is there basic lower/upper memory information? */
#define MB_INFO_BOOTDEV         0x2 /* is there a boot device set? */
#define MB_INFO_CMDLINE         0x4 /* is the command-line defined? */
#define MB_INFO_MODS            0x8 /* are there modules to do something with? */

/* These next two are mutually exclusive */

#define MB_INFO_AOUT_SYMS       0x10 /* is there a symbol table loaded? */
#define MB_INFO_ELF_SHDR        0x20 /* is there an ELF section header table? */
#define MB_INFO_MEM_MAP         0x40 /* is there a full memory map? */
#define MB_INFO_DRIVE_INFO      0x80
#define MB_INFO_CONFIG_TABLE    0x100
#define MB_INFO_BOOT_LOADER_NAME 0x200
#define MB_INFO_APM_TABLE       0x400
#define MB_INFO_VBE_INFO        0x800
#define MB_INFO_FRAMEBUFFER     0x1000 /* is there a framebuffer (graphics mode)? */

/*
 *  The following value must be present in the EAX register.
 */

#define MULTIBOOT_VALID         0x2BADB002

#endif /* __F_KERNEL_MULTIBOOT_H__ */
