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

#include "clientsettingsdlg.h"


/* Implementation *************************************************************/
CClientSettingsDlg::CClientSettingsDlg ( CClient* pNCliP, QWidget* parent,
    Qt::WindowFlags f ) : pClient ( pNCliP ), QDialog ( parent, f )
{
    setupUi ( this );

    // add help text to controls
    QString strJitterBufferSize = tr ( "<b>Jitter Buffer Size:</b> The size of "
        "the network buffer (jitter buffer). The jitter buffer compensates for "
        "the network jitter. The larger this buffer is, the more robust the "
        "connection is against network jitter but the higher is the latency. "
        "This setting is therefore a trade-off between audio drop outs and "
        "overall audio delay.<br>By changing this setting, both, the client "
        "and the server jitter buffer is set to the same value." );
    SliderNetBuf->setWhatsThis ( strJitterBufferSize );
    TextNetBuf->setWhatsThis ( strJitterBufferSize );
    GroupBoxJitterBuffer->setWhatsThis ( strJitterBufferSize );

    // init driver button
#ifdef _WIN32
    ButtonDriverSetup->setText ( "ASIO Setup" );
#else
    // no use for this button for Linux right now, maybe later used
    // for Jack
    ButtonDriverSetup->hide();
#endif

    // init delay and other information controls
    CLEDOverallDelay->SetUpdateTime ( 2 * PING_UPDATE_TIME );
    CLEDOverallDelay->Reset();
    TextLabelPingTime->setText     ( "" );
    TextLabelOverallDelay->setText ( "" );
    TextUpstreamValue->setText     ( "" );


    // init slider controls ---
    // network buffer
    SliderNetBuf->setRange ( MIN_NET_BUF_SIZE_NUM_BL, MAX_NET_BUF_SIZE_NUM_BL );
    UpdateJitterBufferFrame();

    // sound card buffer size
    SliderSndCrdBufferDelay->setRange ( 0,
        CSndCrdBufferSizes::GetNumOfBufferSizes() - 1 );

    // init combo box containing all available sound cards in the system
    cbSoundcard->clear();
    for ( int iSndDevIdx = 0; iSndDevIdx < pClient->GetSndCrdNumDev(); iSndDevIdx++ )
    {
        cbSoundcard->addItem ( pClient->GetSndCrdDeviceName ( iSndDevIdx ).c_str() );
    }
    cbSoundcard->setCurrentIndex ( pClient->GetSndCrdDev() );

    // "OpenChatOnNewMessage" check box
    if ( pClient->GetOpenChatOnNewMessage() )
    {
        cbOpenChatOnNewMessage->setCheckState ( Qt::Checked );
    }
    else
    {
        cbOpenChatOnNewMessage->setCheckState ( Qt::Unchecked );
    }

    // audio compression type
    switch ( pClient->GetAudioCompressionOut() )
    {
    case CT_NONE:
        radioButtonNoAudioCompr->setChecked ( true );
        break;

    case CT_IMAADPCM:
        radioButtonIMA_ADPCM->setChecked ( true );
        break;

    case CT_MSADPCM:
        radioButtonMS_ADPCM->setChecked ( true );
        break;
    }
    AudioCompressionButtonGroup.addButton ( radioButtonNoAudioCompr );
    AudioCompressionButtonGroup.addButton ( radioButtonIMA_ADPCM );
    AudioCompressionButtonGroup.addButton ( radioButtonMS_ADPCM );


    // Connections -------------------------------------------------------------
    // timers
    QObject::connect ( &TimerStatus, SIGNAL ( timeout() ),
        this, SLOT ( OnTimerStatus() ) );
    QObject::connect ( &TimerPing, SIGNAL ( timeout() ),
        this, SLOT ( OnTimerPing() ) );

    // slider controls
    QObject::connect ( SliderNetBuf, SIGNAL ( valueChanged ( int ) ),
        this, SLOT ( OnSliderNetBuf ( int ) ) );
    QObject::connect ( SliderSndCrdBufferDelay, SIGNAL ( valueChanged ( int ) ),
        this, SLOT ( OnSliderSndCrdBufferDelay ( int ) ) );

    // check boxes
    QObject::connect ( cbOpenChatOnNewMessage, SIGNAL ( stateChanged ( int ) ),
        this, SLOT ( OnOpenChatOnNewMessageStateChanged ( int ) ) );
    QObject::connect ( cbAutoJitBuf, SIGNAL ( stateChanged ( int ) ),
        this, SLOT ( OnAutoJitBuf ( int ) ) );

    // combo boxes
    QObject::connect ( cbSoundcard, SIGNAL ( activated ( int ) ),
        this, SLOT ( OnSoundCrdSelection ( int ) ) );

    // buttons
    QObject::connect ( ButtonDriverSetup, SIGNAL ( clicked() ),
        this, SLOT ( OnDriverSetupBut() ) );

    // misc
    QObject::connect ( pClient, SIGNAL ( PingTimeReceived ( int ) ),
        this, SLOT ( OnPingTimeResult ( int ) ) );

    QObject::connect ( &AudioCompressionButtonGroup, SIGNAL ( buttonClicked ( QAbstractButton* ) ),
        this, SLOT ( OnAudioCompressionButtonGroupClicked ( QAbstractButton* ) ) );


    // Timers ------------------------------------------------------------------
    // start timer for status bar
    TimerStatus.start ( DISPLAY_UPDATE_TIME );
}

void CClientSettingsDlg::UpdateJitterBufferFrame()
{
    // update slider value and text
    const int iCurNumNetBuf = pClient->GetSockBufSize();
    SliderNetBuf->setValue ( iCurNumNetBuf );
    TextNetBuf->setText ( "Size: " + QString().setNum ( iCurNumNetBuf ) );

    // if auto setting is enabled, disable slider control
    cbAutoJitBuf->setChecked (  pClient->GetDoAutoSockBufSize() );
    SliderNetBuf->setEnabled ( !pClient->GetDoAutoSockBufSize() );
    TextNetBuf->setEnabled   ( !pClient->GetDoAutoSockBufSize() );
}

void CClientSettingsDlg::UpdateSoundCardFrame()
{
    // update slider value and text
    const int iCurPrefBufIdx   = pClient->GetSndCrdPreferredMonoBlSizeIndex();
    const int iCurActualBufIdx = pClient->GetSndCrdActualMonoBlSize();
    SliderSndCrdBufferDelay->setValue ( iCurPrefBufIdx );

    // preferred size
    TextLabelPreferredSndCrdBufDelay->setText (
        QString().setNum ( (double) CSndCrdBufferSizes::GetBufferSizeFromIndex ( iCurPrefBufIdx ) *
        1000 / SND_CRD_SAMPLE_RATE, 'f', 2 ) + " ms (" +
        QString().setNum ( CSndCrdBufferSizes::GetBufferSizeFromIndex ( iCurPrefBufIdx ) ) + ")" );

    // actual size
    TextLabelActualSndCrdBufDelay->setText (
        QString().setNum ( (double) iCurActualBufIdx *
        1000 / SND_CRD_SAMPLE_RATE, 'f', 2 ) + " ms (" +
        QString().setNum ( iCurActualBufIdx ) + ")" );
}

void CClientSettingsDlg::showEvent ( QShowEvent* showEvent )
{
    // only activate ping timer if window is actually shown
    TimerPing.start ( PING_UPDATE_TIME );

    UpdateDisplay();
}

void CClientSettingsDlg::hideEvent ( QHideEvent* hideEvent )
{
    // if window is closed, stop timer for ping
    TimerPing.stop();
}

void CClientSettingsDlg::OnDriverSetupBut()
{
    pClient->OpenSndCrdDriverSetup();
}

void CClientSettingsDlg::OnSliderNetBuf ( int value )
{
    pClient->SetSockBufSize ( value );
    UpdateJitterBufferFrame();
}

void CClientSettingsDlg::OnSliderSndCrdBufferDelay ( int value )
{
    pClient->SetSndCrdPreferredMonoBlSizeIndex ( value );
    UpdateSoundCardFrame();
}

void CClientSettingsDlg::OnSoundCrdSelection ( int iSndDevIdx )
{
    try
    {
        pClient->SetSndCrdDev ( iSndDevIdx );
    }
    catch ( CGenErr generr )
    {
        QMessageBox::critical ( 0, APP_NAME,
            QString ( "The selected audio device could not be used because "
            "of the following error: " ) + generr.GetErrorText() +
            QString ( " The previous driver will be selected." ), "Ok", 0 );

        // recover old selection
        cbSoundcard->setCurrentIndex ( pClient->GetSndCrdDev() );
    }
    UpdateDisplay();
}

void CClientSettingsDlg::OnAutoJitBuf ( int value )
{
    pClient->SetDoAutoSockBufSize ( value == Qt::Checked );
    UpdateJitterBufferFrame();
}

void CClientSettingsDlg::OnOpenChatOnNewMessageStateChanged ( int value )
{
    pClient->SetOpenChatOnNewMessage ( value == Qt::Checked );
    UpdateDisplay();
}

void CClientSettingsDlg::OnAudioCompressionButtonGroupClicked ( QAbstractButton* button )
{
    if ( button == radioButtonNoAudioCompr )
    {
        pClient->SetAudioCompressionOut ( CT_NONE );
    }

    if ( button == radioButtonIMA_ADPCM )
    {
        pClient->SetAudioCompressionOut ( CT_IMAADPCM );
    }

    if ( button == radioButtonMS_ADPCM )
    {
        pClient->SetAudioCompressionOut ( CT_MSADPCM );
    }
    UpdateDisplay();
}

void CClientSettingsDlg::OnTimerPing()
{
    // send ping message to server
    pClient->SendPingMess();
}

void CClientSettingsDlg::OnPingTimeResult ( int iPingTime )
{
/*
    For estimating the overall delay, use the following assumptions:
    - the mean delay of a cyclic buffer is half the buffer size (since
      for the average it is assumed that the buffer is half filled)
    - consider the jitter buffer on the server side, too
*/

    const int iTotalJitterBufferDelayMS = MIN_SERVER_BLOCK_DURATION_MS *
        ( 2 /* buffer at client and server */ * pClient->GetSockBufSize() ) / 2;


// TODO consider sound card interface block size

const int iTotalSoundCardDelayMS = 0;
//    const int iTotalSoundCardDelayMS = 2 * MIN_SERVER_BLOCK_DURATION_MS +
//        MIN_SERVER_BLOCK_DURATION_MS * ( pClient->GetSndInterface()->GetInNumBuf() +
//          pClient->GetSndInterface()->GetOutNumBuf() ) / 2;

    const int iDelayToFillNetworkPackets =
        ( pClient->GetNetwBufSizeOut() + pClient->GetAudioBlockSizeIn() ) *
        1000 / SYSTEM_SAMPLE_RATE;

    const int iTotalBufferDelay = iDelayToFillNetworkPackets +
        iTotalJitterBufferDelayMS + iTotalSoundCardDelayMS;

    const int iOverallDelay = iTotalBufferDelay + iPingTime;

    // apply values to GUI labels, take special care if ping time exceeds
    // a certain value
    if ( iPingTime > 500 )
    {
        const QString sErrorText = "<font color=""red""><b>&#62;500 ms</b></font>";
        TextLabelPingTime->setText ( sErrorText );
        TextLabelOverallDelay->setText ( sErrorText );
    }
    else
    {
        TextLabelPingTime->setText ( QString().setNum ( iPingTime ) + " ms" );
        TextLabelOverallDelay->setText ( QString().setNum ( iOverallDelay ) + " ms" );
    }

    // color definition: < 40 ms green, < 60 ms yellow, otherwise red
    if ( iOverallDelay <= 40 )
    {
        CLEDOverallDelay->SetLight ( MUL_COL_LED_GREEN );
    }
    else
    {
        if ( iOverallDelay <= 60 )
        {
            CLEDOverallDelay->SetLight ( MUL_COL_LED_YELLOW );
        }
        else
        {
            CLEDOverallDelay->SetLight ( MUL_COL_LED_RED );
        }
    }
}

void CClientSettingsDlg::UpdateDisplay()
{
    // update slider controls (settings might have been changed)
    UpdateJitterBufferFrame();
    UpdateSoundCardFrame();

    if ( !pClient->IsRunning() )
    {
        // clear text labels with client parameters
        TextLabelPingTime->setText     ( "" );
        TextLabelOverallDelay->setText ( "" );
        TextUpstreamValue->setText     ( "" );
    }
    else
    {
        // update upstream rate information label (only if client is running)
        TextUpstreamValue->setText ( QString().setNum ( pClient->GetUploadRateKbps() ) + " kbps" );
    }
}

void CClientSettingsDlg::SetStatus ( const int iMessType, const int iStatus )
{
    switch ( iMessType )
    {
/*
    case MS_SOUND_IN:
        CLEDSoundIn->SetLight ( iStatus );
        break;

    case MS_SOUND_OUT:
        CLEDSoundOut->SetLight ( iStatus );
        break;
*/

    case MS_JIT_BUF_PUT:
    case MS_JIT_BUF_GET:
        // network LED shows combined status of put and get
        CLEDNetw->SetLight ( iStatus );
        break;

    case MS_RESET_ALL:
/*
        CLEDSoundIn->Reset();
        CLEDSoundOut->Reset();
*/
        CLEDNetw->Reset();
        break;
    }
}
