/*
 * freeglut_display.c
 *
 * Display message posting, context buffer swapping.
 *
 * Copyright (c) 1999-2000 Pawel W. Olszta. All Rights Reserved.
 * Written by Pawel W. Olszta, <olszta@sourceforge.net>
 * Creation date: Fri Dec 3 1999
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PAWEL W. OLSZTA BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <GL/freeglut.h>
#include "freeglut_internal.h"

/* -- INTERFACE FUNCTIONS -------------------------------------------------- */

/*
 * Marks the current window to have the redisplay performed when possible...
 */
void FGAPIENTRY glutPostRedisplay( void )
{
    FREEGLUT_EXIT_IF_NOT_INITIALISED ( "glutPostRedisplay" );
    FREEGLUT_EXIT_IF_NO_WINDOW ( "glutPostRedisplay" );
    fgStructure.CurrentWindow->State.Redisplay = GL_TRUE;
}

/*
 * Swaps the buffers for the current window (if any)
 */
void FGAPIENTRY glutSwapBuffers( void )
{
    FREEGLUT_EXIT_IF_NOT_INITIALISED ( "glutSwapBuffers" );
    FREEGLUT_EXIT_IF_NO_WINDOW ( "glutSwapBuffers" );

    glFlush( );
    if( ! fgStructure.CurrentWindow->Window.DoubleBuffered )
        return;

#if TARGET_HOST_UNIX_X11
    glXSwapBuffers( fgDisplay.Display, fgStructure.CurrentWindow->Window.Handle );
#elif TARGET_HOST_WIN32 || TARGET_HOST_WINCE
    SwapBuffers( fgStructure.CurrentWindow->Window.Device );
#endif
}

/*** END OF FILE ***/