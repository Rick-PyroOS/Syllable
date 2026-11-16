/*
 *  The Syllable kernel
 *  Boot screen: the boot splash with its progress bar, and the boot console
 *  (black text screen, or text drawn into a graphics framebuffer).
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU Library
 *  General Public License as published by the Free Software
 *  Foundation.
 */

#ifndef __F_INC_BOOTSPLASH_H__
#define __F_INC_BOOTSPLASH_H__

#include <kernel/types.h>
#include <kernel/multiboot.h>

/* Before paging: what GRUB handed over (text or a framebuffer) and the
 * quiet= and splash= kernel arguments. */
void bootsplash_early( const MultiBootHeader_s *psInfo, const char *pzParams );
/* Once areas work: map the framebuffer and draw the splash (or clear the
 * screen for the framebuffer console). */
void bootsplash_init( void );
/* Progress, in percent; the bar only moves forward. */
void bootsplash_progress( int nPercent );
/* One driver more is being initialised (moves the bar a little). */
void bootsplash_driver_step( void );
/* Init is starting: fill the bar and stop drawing, because the appserver
 * will set its own video mode. */
void bootsplash_done( void );
/* A panic: show the kernel log on screen, whatever the splash was doing. */
void bootsplash_panic( void );

/* dbconsole.c: writes the boot console; bootsplash.c decides where */
bool bootsplash_text_wanted( void );
bool bootsplash_fb_write( const char *pData, int nSize );

#endif /* __F_INC_BOOTSPLASH_H__ */
