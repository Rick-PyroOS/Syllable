/*
 *  The Syllable kernel
 *	Bochs/QEMU "DISPI" graphics kernel driver
 *
 *	Claims the QEMU standard VGA, bochs-display and QXL (VGA mode) PCI devices
 *	and hands them to the appserver's "bochs" video driver.  Like the other
 *	graphics kernel drivers it only probes PCI and passes PCI information and
 *	config space through; all hardware programming is in the appserver driver.
 *
 *	Based on the VMware graphics kernel driver:
 *	Copyright (C) 2004 James Hayhurst
 *	Copyright (C) 2003 Arno Klenke
 *	Copyright (C) 1999 - 2001 Kurt Skauen
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU Library
 *  General Public License as published by the Free Software
 *  Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <posix/errno.h>

#include <kernel/malloc.h>
#include <kernel/string.h>
#include <kernel/stdlib.h>
#include <kernel/device.h>
#include <kernel/pci.h>
#include <appserver/pci_graphics.h>

#define PCI_VENDOR_ID_BOCHS		0x1234
#define PCI_DEVICE_ID_BOCHS_VGA		0x1111	/* QEMU -vga std and -device bochs-display */
#define PCI_VENDOR_ID_REDHAT		0x1b36
#define PCI_DEVICE_ID_REDHAT_QXL	0x0100	/* QEMU -vga qxl, used here only in its VGA mode */

#define PCI_CLASS_DISPLAY		0x03
#define PCI_SUBCLASS_DISPLAY_VGA	0x00

struct gfx_device
{
	int nVendorID;
	int nDeviceID;
	bool bVGAOnly;		/* Only claim it when it is the VGA-compatible head */
	char *zVendorName;
	char *zDeviceName;
};

struct gfx_node
{
	PCI_Info_s sInfo;
	char zName[255];
};

#define APPSERVER_DRIVER "bochs"

struct gfx_device g_sDevices[] = {
	{PCI_VENDOR_ID_BOCHS, PCI_DEVICE_ID_BOCHS_VGA, false, "QEMU", "Standard VGA"},
	/* A secondary QXL head (class 0x0380) has no VGA mode, so leave it alone */
	{PCI_VENDOR_ID_REDHAT, PCI_DEVICE_ID_REDHAT_QXL, true, "QEMU", "QXL (VGA mode)"},
};

status_t gfx_open( void *pNode, uint32 nFlags, void **pCookie )
{
	struct gfx_node *psNode = pNode;

	printk( "%s opened\n", psNode->zName );
	return ( 0 );
}

status_t gfx_close( void *pNode, void *pCookie )
{
	struct gfx_node *psNode = pNode;

	printk( "%s closed\n", psNode->zName );
	return ( 0 );
}

status_t gfx_ioctl( void *pNode, void *pCookie, uint32 nCommand, void *pArgs, bool bFromKernel )
{
	struct gfx_node *psNode = pNode;
	int nError = 0;

	switch ( nCommand )
	{
	case IOCTL_GET_APPSERVER_DRIVER:
		memcpy_to_user( pArgs, APPSERVER_DRIVER, strlen( APPSERVER_DRIVER ) + 1 );
		break;
	case PCI_GFX_GET_PCI_INFO:
		memcpy_to_user( pArgs, &psNode->sInfo, sizeof( PCI_Info_s ) );
		break;
	case PCI_GFX_READ_PCI_CONFIG:
		{
			struct gfx_pci_config sConfig;
			PCI_bus_s *psBus = get_busmanager( PCI_BUS_NAME, PCI_BUS_VERSION );
			memcpy_from_user( &sConfig, pArgs, sizeof( struct gfx_pci_config ) );

			if ( psBus == NULL )
			{
				nError = -ENODEV;
			}
			else
			{
				sConfig.m_nValue = psBus->read_pci_config( sConfig.m_nBus, sConfig.m_nDevice, sConfig.m_nFunction, sConfig.m_nOffset, sConfig.m_nSize );
				memcpy_to_user( pArgs, &sConfig, sizeof( struct gfx_pci_config ) );
			}
		}
		break;
	case PCI_GFX_WRITE_PCI_CONFIG:
		{
			struct gfx_pci_config sConfig;
			PCI_bus_s *psBus = get_busmanager( PCI_BUS_NAME, PCI_BUS_VERSION );
			memcpy_from_user( &sConfig, pArgs, sizeof( struct gfx_pci_config ) );

			if ( psBus == NULL )
			{
				nError = -ENODEV;
			}
			else
			{
				int nVal = psBus->write_pci_config( sConfig.m_nBus, sConfig.m_nDevice,
					sConfig.m_nFunction, sConfig.m_nOffset, sConfig.m_nSize, sConfig.m_nValue );

				sConfig.m_nValue = nVal;
				memcpy_to_user( pArgs, &sConfig, sizeof( struct gfx_pci_config ) );
			}
		}
		break;
	default:
		nError = -ENOIOCTLCMD;
	}
	return ( nError );
}

DeviceOperations_s g_sOperations = {
	gfx_open,
	gfx_close,
	gfx_ioctl,
	NULL,
	NULL
};

status_t device_init( int nDeviceID )
{
	PCI_bus_s *psBus;
	int nNumDevices = sizeof( g_sDevices ) / sizeof( struct gfx_device );
	int nDeviceNum;
	PCI_Info_s sInfo;
	int nPCINum;
	char zTemp[255];
	char zNodePath[255];
	struct gfx_node *psNode;
	bool bDevFound = false;

	/* Get PCI busmanager */
	psBus = get_busmanager( PCI_BUS_NAME, PCI_BUS_VERSION );
	if ( psBus == NULL )
		return ( -ENODEV );

	/* Look for the device */
	for ( nPCINum = 0; psBus->get_pci_info( &sInfo, nPCINum ) == 0; ++nPCINum )
	{
		for ( nDeviceNum = 0; nDeviceNum < nNumDevices; ++nDeviceNum )
		{
			if ( sInfo.nVendorID != g_sDevices[nDeviceNum].nVendorID || sInfo.nDeviceID != g_sDevices[nDeviceNum].nDeviceID )
				continue;

			if ( g_sDevices[nDeviceNum].bVGAOnly && ( sInfo.nClassBase != PCI_CLASS_DISPLAY || sInfo.nClassSub != PCI_SUBCLASS_DISPLAY_VGA ) )
				continue;

			/* std VGA and bochs-display share an ID; bochs-display is class 0x0380 */
			if ( sInfo.nVendorID == PCI_VENDOR_ID_BOCHS && ( sInfo.nClassBase != PCI_CLASS_DISPLAY || sInfo.nClassSub != PCI_SUBCLASS_DISPLAY_VGA ) )
				sprintf( zTemp, "%s %s", g_sDevices[nDeviceNum].zVendorName, "bochs-display" );
			else
				sprintf( zTemp, "%s %s", g_sDevices[nDeviceNum].zVendorName, g_sDevices[nDeviceNum].zDeviceName );
			if ( claim_device( nDeviceID, sInfo.nHandle, zTemp, DEVICE_VIDEO ) != 0 )
			{
				continue;
			}

			printk( "%s found\n", zTemp );

			/* Create private node */
			psNode = kmalloc( sizeof( struct gfx_node ), MEMF_KERNEL | MEMF_CLEAR | MEMF_NOBLOCK );
			if ( psNode == NULL )
			{
				printk( "Error: Out of memory\n" );
				continue;
			}
			memcpy( &psNode->sInfo, &sInfo, sizeof( PCI_Info_s ) );
			strcpy( psNode->zName, zTemp );

			/* Create node path */
			sprintf( zNodePath, "graphics/bochs_%i_0x%x_0x%x", nPCINum, ( uint )sInfo.nVendorID, ( uint )sInfo.nDeviceID );

			if ( create_device_node( nDeviceID, sInfo.nHandle, zNodePath, &g_sOperations, psNode ) < 0 )
			{
				printk( "Error: Failed to create device node %s\n", zNodePath );
				continue;
			}

			bDevFound = true;
		}
	}

	if ( !bDevFound )
	{
		disable_device( nDeviceID );
		return ( -ENODEV );
	}

	return ( 0 );
}

status_t device_uninit( int nDeviceID )
{
	return ( 0 );
}
