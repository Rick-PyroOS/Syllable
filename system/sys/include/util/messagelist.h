#ifndef __F_UTIL_MESSAGELIST_H__
#define __F_UTIL_MESSAGELIST_H__

namespace os
{

/** \anchor os_gui_sysmessages
 * This is the various messages that is sent by the system. Most of these
 * are handled internally by the various GUI classes but every now and then
 * you might have to deal with them on your own. Input event messages like
 * M_MOUSE_DOWN/M_MOUSE_UP/M_MOUSE_MOVED/M_KEY_DOWN, etc etc are all translated
 * into calls of the various os::View virtual hook members so you don't have
 * to know anything about the raw messages to deal with them.
 *
 *
 * \par M_KEY_DOWN/M_KEY_UP:
 *	This messages are sendt to the active window to report keyboard
 *	activity. They are translated into calls of View::KeyDown()
 *	View::KeyUp() on the active view by the Window class so you
 *	normally don't have to deal with it on your own. Not all
 *	members of the message are extracted and passed to the
 *	virtual hooks though so you might occationally have to
 *	mess with the raw message yourself.
 * \par
 *	<table><tr><td>Name</td><td>Type</td><td>Description</td></tr>
 *	<tr><td>_raw_key</td><td>T_INT32</td><td>The raw untranslated key-code</td></tr>
 *	<tr><td>_qualifiers</td><td>T_INT32</td><td>The current \ref os_gui_qualifiers "qualifier" mask</td></tr>
 *	<tr><td>_string</td><td>T_STRING</td><td>String containing a single UTF-8 encoded character.</td></tr>
 *	<tr><td>_raw_string</td><td>T_STRING</td><td>String containing a single UTF-8 encoded character.</td></tr>
 *	</table>
 *	<p>
 * \par M_MOUSE_DOWN/M_MOUSE_UP
 *	This messages are send when the user click one of the mouse buttons.
 *	They are translated into calls of View::MouseDown() and View::MouseUp()
 *	by the Window class and are rarly touched from user code.
 * \par
 *	<table><tr><td>Name</td><td>Type</td><td>Description</td></tr>
 *	<tr><td>_button</td><td>T_INT32</td><td>Button number. Buttons are numbered from 1 where 1 is
 *						the left button, 2 is the right button, and 3 is the
 *						middle button. The mouse driver can also suppor
 *						additional buttons and this will then be assigned
 *						numbers from 4 and up.</td></tr>
 *	<tr><td>_drag_message</td><td>T_MESSAGE</td><td>This member is only present in M_MOUSE_UP
 *							messages and only if the M_MOUSE_UP was the
 *							end of a drag and drop operating. The content
 *							is the data being dragged as defined by the
 *							View::BeginDrag() member.</td></tr>
 *	</table>
 * 
 *****************************************************************************/

enum
{
	// ****** server messages (for all servers) 0 - 999
	M_SERVER_PING = 0,
	M_SERVER_RESTART,
	M_SERVER_QUIT,
	
	// ****** registrar messages 1000 - 1999
	
	// ****** appserver messages 2000 - 2999
    /* Attention! */
	/* Be careful not to change the sequence of message codes here, */
	/* as doing so would break binary compatibility! */
	/* UPDATE ---- BINARY COMPATIBILITY IS NOW BROKEN WITH 0.6.6 */
	
	M_ABOUT_REQUESTED = 10000000,
	M_WINDOW_ACTIVATED,
	M_APP_ACTIVATED,
	M_ARGV_RECEIVED,
	M_QUIT_REQUESTED,
	M_CLOSE_REQUESTED,

	M_KEY_DOWN,
	M_KEY_UP,
	M_MINIMIZE,
	M_MOUSE_DOWN,
	M_MOUSE_UP,
	M_MOUSE_MOVED,
	M_WHEEL_MOVED,
	M_READY_TO_RUN,
	M_PATHS_RECEIVED,
	M_SCREEN_CHANGED,
	M_VALUE_CHANGED,
	M_VIEW_MOVED,
	M_VIEW_RESIZED,

	M_WINDOW_MOVED,
	M_WINDOW_RESIZED,
	M_DESKTOP_ACTIVATED,
	M_SCREENMODE_CHANGED,
	M_ZOOM,
	M_PAINT,
	M_COLOR_CONFIG_CHANGED,
	M_FONT_CHANGED,
	M_MENU_EVENT,
	M_WINDOWS_CHANGED,
    _M_UNUSED3,
	M_QUIT,			/* Quit if you want	*/
	M_TERMINATE,			/* Quit NOW	*/
    _M_UNUSED4,
    _M_UNUSED5,
    _M_UNUSED6,
    _M_UNUSED7,
    _M_UNUSED8,
    _M_UNUSED9,
	M_MOVE_WINDOW,
	M_RESIZE_WINDOW,
	M_WINDOW_FRAME_CHANGED,

	/* Add new appserver message codes here. */

	M_SET_PROPERTY = 20000000,
	M_GET_PROPERTY,
	M_CREATE_PROPERTY,
	M_DELETE_PROPERTY,
	M_GET_SUPPORTED_SUITES,
	M_CUT,
	M_COPY,
	M_PASTE,
	M_LOAD_REQUESTED,
	M_SAVE_REQUESTED,
	M_FILE_REQUESTER_CANCELED,

	M_MESSAGE_NOT_UNDERSTOOD,
	M_NO_REPLY,
	M_REPLY,
	M_SIMPLE_DATA,
	M_MIME_DATA,
	M_ARCHIVED_OBJECT,
	M_UPDATE_STATUS_BAR,
	M_RESET_STATUS_BAR,
	M_NODE_MONITOR,

   	M_FONT_REQUESTED,
	M_FONT_REQUESTER_CANCELED,

	M_BOX_REQUESTED,
	M_BOX_REQUESTER_CANCELED,

	M_COLOR_REQUESTER_CANCELED,
	M_COLOR_REQUESTED,
	M_COLOR_REQUESTER_CHANGED,
	
	M_CLIPBOARD_CHANGED,
	
	M_YEAR_CHANGED,
	M_MONTH_CHANGED,
	M_CALENDAR_DATE_INVOKED,
	M_CHECKROW_CHANGED,
	M_CHECKNODE_CHANGED,
    
	M_FIRST_EVENT = 3900,
	M_LAST_EVENT = 3999,
    
	// ****** mediaserver messages 3000 - 3999
	
	// ****** inputserver messages 4000 - 4999
	
	// ****** volumeserver messages 5000 - 5999
	
	// ****** netserver messages 6000 - 6999
	
	// ****** displayserver messages 7000 - 7999
	
	// ****** nodemonitor messages 8000 - 8999
	
	// NON-SYSTEM MESSAGES START AT 100000
    

};

}
#endif /* __F_UTIL_MESSAGELIST_H__ */



