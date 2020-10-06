

#include <proto/exec.h>
#include <proto/intuition.h>
#include <devices/inputevent.h>
#include <proto/keymap.h>

struct MsgStruct
{
	ULONG Class;
	UWORD Code;
	WORD MouseX;
	WORD MouseY;
	UWORD rawkey;
};

int GetMessages68k( __reg("a1") struct MsgPort *port, __reg("a0") struct MsgStruct *msg, __reg("d0") int maxmsg )
{
	int i = 0;
	struct IntuiMessage *imsg;
	struct InputEvent ie;

	UBYTE buf[4];
	UWORD result;

	while ((imsg = (struct IntuiMessage *)GetMsg(port)))
	{
		if (i < maxmsg)
		{
			msg[i].Code = imsg->Code;
			msg[i].Class = imsg->Class;
			msg[i].MouseX = imsg->MouseX;
			msg[i].MouseY = imsg->MouseY;

			if( msg[i].Class == IDCMP_RAWKEY && imsg->Code & ~IECODE_UP_PREFIX )
			{
				ie.ie_Class = IECLASS_RAWKEY;
				ie.ie_SubClass = 0;
				ie.ie_Code = imsg->Code;
				ie.ie_Qualifier = imsg->Qualifier;
				ie.ie_EventAddress = 0;

				result = MapRawKey(&ie, buf, 4, 0);
				
				if (result != 1 )
					msg[i].rawkey = 0;

				else
					msg[i].rawkey = buf[0];
			}

			i++;
		}

		ReplyMsg((struct Message *)imsg);
	}

	return i;
}

#if 0

struct InputEvent *InputCode (__reg("a0") struct InputEvent *ielist)
{
	struct InputEvent *ie;
	ie = ielist;

	do
	{
		if (ie->ie_Class == IECLASS_RAWMOUSE)
		{
			switch(ie->ie_Code)
			{
				case IECODE_LBUTTON:	
					ie->ie_Class = IECLASS_RAWKEY;
					ie->ie_Code  = 0x63;
					break;

				case IECODE_LBUTTON|IECODE_UP_PREFIX:	
					ie->ie_Class = IECLASS_RAWKEY;
					ie->ie_Code  = 0x63|IECODE_UP_PREFIX;
					break;
			}
		}
	
		ie = ie->ie_NextEvent;

	} while (ie);	
	
	return(ielist);
}

#endif
