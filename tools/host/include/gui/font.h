/* Host shim: util/string.cpp includes gui/font.h only for the UTF-8 helper
 * below (copied from the real header).  The rest of the font API needs the
 * appserver. */
#ifndef __F_GUI_FONT_H__
#define __F_GUI_FONT_H__

#include <atheos/types.h>

inline int utf8_char_length( uint8 nFirstByte )
{
    return( ((0xe5000000 >> ((nFirstByte >> 3) & 0x1e)) & 3) + 1 );
}

#endif /* __F_GUI_FONT_H__ */
