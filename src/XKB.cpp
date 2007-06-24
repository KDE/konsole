/*
 Originally comes from NumLockX http://dforce.sh.charactervut.characterz/~seli/en/numlockx

 NumLockX
 
 Copyright (C) 2000-2001 Lubos Lunak        <l.lunak@kde.org>
 Copyright (C) 2001      Oswald Buddenhagen <ossi@kde.org>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

****************************************************************************/

#include <config-konsole.h>

#if defined(HAVE_XKB)
    #include <QtGui/QX11Info>


    #include <X11/Xlib.h>

    #define explicit myexplicit
    #include <X11/XKBlib.h>
    #undef explicit

    #include <X11/keysym.h>

/* the XKB stuff is based on code created by Oswald Buddenhagen <ossi@kde.org> */
int xkb_init()
{
    int xkb_opcode, xkb_event, xkb_error;
    int xkb_lmaj = XkbMajorVersion;
    int xkb_lmin = XkbMinorVersion;
    return XkbLibraryVersion( &xkb_lmaj, &xkb_lmin )
        && XkbQueryExtension( QX11Info::display(), &xkb_opcode, &xkb_event, &xkb_error,
			       &xkb_lmaj, &xkb_lmin );
}
    
#if 0
// This method doesn't work in all cases. The atom "ScrollLock" doesn't seem
// to exist on all XFree versions (at least it's not here with my 3.3.6) - DF
static unsigned int xkb_mask_modifier( XkbDescPtr xkb, const char *name )
{
    int i;
    if( !xkb || !xkb->names )
	return 0;

    Atom atom = XInternAtom( xkb->dpy, name, true );
    if (atom == None)
        return 0;

    for( i = 0;
         i < XkbNumVirtualMods;
	 i++ )
    {
	if (atom == xkb->names->vmods[i] )
	{
	    unsigned int mask;
	    XkbVirtualModsToReal( xkb, 1 << i, &mask );
	    return mask;
	}
    }
    return 0;
}

static unsigned int xkb_scrolllock_mask()
{
    XkbDescPtr xkb;
    if(( xkb = XkbGetKeyboard( QX11Info::display(), XkbAllComponentsMask, XkbUseCoreKbd )) != NULL )
    {
        unsigned int mask = xkb_mask_modifier( xkb, "ScrollLock" );
        XkbFreeKeyboard( xkb, 0, True );
        return mask;
    }
    return 0;
}

#else
unsigned int xkb_scrolllock_mask()
{
    int scrolllock_mask = 0;
    XModifierKeymap* map = XGetModifierMapping( QX11Info::display() );
    KeyCode scrolllock_keycode = XKeysymToKeycode( QX11Info::display(), XK_Scroll_Lock );
    if( scrolllock_keycode == NoSymbol ) {
        XFreeModifiermap(map);
        return 0;
    }
    for( int i = 0;
         i < 8;
         ++i )
        {
       if( map->modifiermap[ map->max_keypermod * i ] == scrolllock_keycode )
               scrolllock_mask += 1 << i;
       }

    XFreeModifiermap(map);
    return scrolllock_mask;
}
#endif


unsigned int scrolllock_mask = 0;
        
int xkb_set_on()
{
    if (!scrolllock_mask)
    {
       if( !xkb_init())
          return 0;
       scrolllock_mask = xkb_scrolllock_mask();
       if( scrolllock_mask == 0 )
          return 0;
    }
    XkbLockModifiers ( QX11Info::display(), XkbUseCoreKbd, scrolllock_mask, scrolllock_mask);
    return 1;
}
    
int xkb_set_off()
{
    if (!scrolllock_mask)
    {
       if( !xkb_init())
          return 0;
       scrolllock_mask = xkb_scrolllock_mask();
       if( scrolllock_mask == 0 )
          return 0;
    }
    XkbLockModifiers ( QX11Info::display(), XkbUseCoreKbd, scrolllock_mask, 0);
    return 1;
}

void scrolllock_set_on()
{
    xkb_set_on();
}

void scrolllock_set_off()
{
    xkb_set_off();
}

#endif // defined(HAVE_XKB)

