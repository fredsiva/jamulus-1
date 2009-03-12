/******************************************************************************\
 * Copyright (c) 2004-2009
 *
 * Author(s):
 *  Volker Fischer
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later 
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT 
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more 
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
\******************************************************************************/

#if !defined ( SOUNDBASE_HOIHGEH8_3_4344456456345634565KJIUHF1912__INCLUDED_ )
#define SOUNDBASE_HOIHGEH8_3_4344456456345634565KJIUHF1912__INCLUDED_

#include <qthread.h>
#include "global.h"
#include "util.h"


/* Classes ********************************************************************/
class CSoundBase : public QThread
{
    Q_OBJECT

public:
    CSoundBase ( const bool bNewIsCallbackAudioInterface,
        void (*fpNewProcessCallback) ( CVector<short>& psData, void* pParg ),
        void* pParg ) : fpProcessCallback ( fpNewProcessCallback ),
        pProcessCallbackArg ( pParg ), bRun ( false ),
        bIsCallbackAudioInterface ( bNewIsCallbackAudioInterface ) {}
    virtual ~CSoundBase() {}

    virtual int  Init ( const int iNewPrefMonoBufferSize );
    virtual void Start();
    virtual void Stop();
    bool         IsRunning() const { return bRun; }

    virtual void OpenDriverSetup() {}

    // TODO this should be protected but since it is used
    // in a callback function it has to be public -> better solution
    void EmitReinitRequestSignal() { emit ReinitRequest(); }

protected:
    // function pointer to callback function
    void (*fpProcessCallback) ( CVector<short>& psData, void* arg );
    void* pProcessCallbackArg;

    // callback function call for derived classes
    void ProcessCallback ( CVector<short>& psData )
    {
        (*fpProcessCallback) ( psData, pProcessCallbackArg );
    }

    // these functions should be overwritten by derived class for
    // non callback based audio interfaces
    virtual bool Read  ( CVector<short>& psData ) { printf ( "no sound!" ); return false; }
    virtual bool Write ( CVector<short>& psData ) { printf ( "no sound!" ); return false; }

    void run();
    bool bRun;
    bool bIsCallbackAudioInterface;

    CVector<short> vecsAudioSndCrdStereo;

signals:
    void ReinitRequest();
};

#endif /* !defined ( SOUNDBASE_HOIHGEH8_3_4344456456345634565KJIUHF1912__INCLUDED_ ) */
