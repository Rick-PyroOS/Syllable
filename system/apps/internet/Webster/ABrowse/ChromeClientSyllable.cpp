/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"
#include "ChromeClientSyllable.h"

#include "Frame.h"
#include "FrameView.h"
#include "FrameLoadRequest.h"
#include "NotImplemented.h"

#include <util/message.h>
#include <gui/requesters.h>


namespace WebCore
{

    
ChromeClientSyllable::ChromeClientSyllable()
{
    
}


ChromeClientSyllable::~ChromeClientSyllable()
{
    
}


void ChromeClientSyllable::chromeDestroyed()
{
    notImplemented();
}

void ChromeClientSyllable::setWindowRect(const FloatRect&)
{
    notImplemented();
}


FloatRect ChromeClientSyllable::windowRect()
{
    notImplemented();
    return IntRect(0, 0, 100, 100);
}


FloatRect ChromeClientSyllable::pageRect()
{
    notImplemented();
    return FloatRect(0, 0, 100, 100);
}


float ChromeClientSyllable::scaleFactor()
{
    notImplemented();
    return 1;
}


void ChromeClientSyllable::focus()
{
    notImplemented();
}


void ChromeClientSyllable::unfocus()
{
    notImplemented();
}

bool ChromeClientSyllable::canTakeFocus(FocusDirection)
{
	 notImplemented();
	return( true );
}

void ChromeClientSyllable::takeFocus(FocusDirection)
{
	 notImplemented();
}


Page* ChromeClientSyllable::createWindow(Frame*, const FrameLoadRequest&, const WindowFeatures&)
{
    notImplemented();
    return 0;
}


Page* ChromeClientSyllable::createModalDialog(Frame*, const FrameLoadRequest&)
{
    return 0;
}


void ChromeClientSyllable::show()
{
    notImplemented();
}


bool ChromeClientSyllable::canRunModal()
{
    notImplemented();
    return false;
}


void ChromeClientSyllable::runModal()
{
    notImplemented();
}


void ChromeClientSyllable::setToolbarsVisible(bool)
{
    notImplemented();
}


bool ChromeClientSyllable::toolbarsVisible()
{
    notImplemented();
    return false;
}


void ChromeClientSyllable::setStatusbarVisible(bool)
{
    notImplemented();
}


bool ChromeClientSyllable::statusbarVisible()
{
    notImplemented();
    return false;
}


void ChromeClientSyllable::setScrollbarsVisible(bool)
{
    notImplemented();
}


bool ChromeClientSyllable::scrollbarsVisible()
{
    notImplemented();
    return true;
}


void ChromeClientSyllable::setMenubarVisible(bool)
{
    notImplemented();
}

bool ChromeClientSyllable::menubarVisible()
{
    notImplemented();
    return false;
}

void ChromeClientSyllable::setResizable(bool)
{
    notImplemented();
}

void ChromeClientSyllable::addMessageToConsole(const String& message, unsigned int lineNumber,
                                         const String& sourceID)
{
	printf( "MESSAGE %s:%i %s\n", os::String( sourceID ).c_str(), lineNumber, os::String( message ).c_str() );
}

bool ChromeClientSyllable::canRunBeforeUnloadConfirmPanel()
{
    notImplemented();
    return false;
}

bool ChromeClientSyllable::runBeforeUnloadConfirmPanel(const String& message, Frame* frame)
{
    notImplemented();
    return false;
}

void ChromeClientSyllable::closeWindowSoon()
{
    notImplemented();
}

void ChromeClientSyllable::runJavaScriptAlert(Frame*, const String& msg)
{
	printf("ChromeClientSyllable::runJavaScriptAlert()\n");
    os::Alert* pcAlert = new os::Alert( "JavaScript", msg, os::Alert::ALERT_INFO, "Ok", NULL );
	pcAlert->Go();
}

bool ChromeClientSyllable::runJavaScriptConfirm(Frame*, const String& msg)
{
	printf("ChromeClientSyllable::runJavaScriptConfirm()\n");
	os::Alert* pcAlert = new os::Alert( "JavaScript", msg, os::Alert::ALERT_QUESTION, "Yes", "No", NULL );
	return( pcAlert->Go() == 0 );
}

bool ChromeClientSyllable::runJavaScriptPrompt(Frame*, const String& message, const String& defaultValue, String& result)
{
    notImplemented();
    return false;
}

void ChromeClientSyllable::setStatusbarText(const String&)
{
    notImplemented();
}

bool ChromeClientSyllable::shouldInterruptJavaScript()
{
    notImplemented();
    return false;
}

bool ChromeClientSyllable::tabsToLinks() const
{
    return false;
}

IntRect ChromeClientSyllable::windowResizerRect() const
{
    return IntRect();
}

void ChromeClientSyllable::addToDirtyRegion(const IntRect&)
{
}

void ChromeClientSyllable::scrollBackingStore(int, int, const IntRect&, const IntRect&)
{
}

void ChromeClientSyllable::updateBackingStore()
{
}


void ChromeClientSyllable::mouseDidMoveOverElement(const HitTestResult&, unsigned /*modifierFlags*/)
{
    notImplemented();
}

void ChromeClientSyllable::setToolTip(const String&)
{
    notImplemented();
}

void ChromeClientSyllable::print(Frame*)
{
    notImplemented();
}

void ChromeClientSyllable::exceededDatabaseQuota(Frame*, const String& databaseName)
{
	notImplemented();
}

}







