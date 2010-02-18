/******************************************************************************\
 * Copyright (c) 2004-2010
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

#include <qapplication.h>
#include <qmessagebox.h>
#include <qdir.h>
#include <iostream>
#include "global.h"
#include "llconclientdlg.h"
#include "llconserverdlg.h"
#include "settings.h"
#include "testbench.h"


// Implementation **************************************************************
// these pointers are only used for the post-event routine
QApplication* pApp   = NULL;
QDialog* pMainWindow = NULL;


int main ( int argc, char** argv )
{
    std::string strArgument;
    double      rDbleArgument;

    // check if server or client application shall be started
    bool        bIsClient             = true;
    bool        bUseGUI               = true;
    bool        bConnectOnStartup     = false;
    bool        bDisalbeLEDs          = false;
    quint16     iPortNumber           = LLCON_DEFAULT_PORT_NUMBER;
    std::string strIniFileName        = "";
    std::string strHTMLStatusFileName = "";
    std::string strServerName         = "";
    std::string strLoggingFileName    = "";
    std::string strHistoryFileName    = "";

    // QT docu: argv()[0] is the program name, argv()[1] is the first
    // argument and argv()[argc()-1] is the last argument.
    // Start with first argument, therefore "i = 1"
    for ( int i = 1; i < argc; i++ )
    {
        // server mode flag ----------------------------------------------------
        if ( GetFlagArgument ( argc, argv, i, "-s", "--server" ) )
        {
            bIsClient = false;
            cout << "- server mode chosen" << std::endl;
            continue;
        }


        // use GUI flag --------------------------------------------------------
        if ( GetFlagArgument ( argc, argv, i, "-n", "--nogui" ) )
        {
            bUseGUI = false;
            cout << "- no GUI mode chosen" << std::endl;
            continue;
        }


        // disable LEDs flag ---------------------------------------------------
        if ( GetFlagArgument ( argc, argv, i, "-d", "--disableleds" ) )
        {
            bDisalbeLEDs = true;
            cout << "- disable LEDs in main window" << std::endl;
            continue;
        }


        // use logging ---------------------------------------------------------
        if ( GetStringArgument ( argc, argv, i, "-l", "--log", strArgument ) )
        {
            strLoggingFileName = strArgument;
            cout << "- logging file name: " << strLoggingFileName << std::endl;
            continue;
        }


        // port number ---------------------------------------------------------
        if ( GetNumericArgument ( argc, argv, i, "-p", "--port",
                                  0, 65535, rDbleArgument ) )
        {
            iPortNumber = static_cast<quint16> ( rDbleArgument );
            cout << "- selected port number: " << iPortNumber << std::endl;
            continue;
        }


        // HTML status file ----------------------------------------------------
        if ( GetStringArgument ( argc, argv, i, "-m", "--htmlstatus", strArgument ) )
        {
            strHTMLStatusFileName = strArgument;
            cout << "- HTML status file name: " << strHTMLStatusFileName << std::endl;
            continue;
        }

        if ( GetStringArgument ( argc, argv, i, "-a", "--servername", strArgument ) )
        {
            strServerName = strArgument;
            cout << "- server name for HTML status file: " << strServerName << std::endl;
            continue;
        }


        // HTML status file ----------------------------------------------------
        if ( GetStringArgument ( argc, argv, i, "-y", "--history", strArgument ) )
        {
            strHistoryFileName = strArgument;
            cout << "- history file name: " << strHistoryFileName << std::endl;
            continue;
        }


        // initialization file -------------------------------------------------
        if ( GetStringArgument ( argc, argv, i, "-i", "--inifile", strArgument ) )
        {
            strIniFileName = strArgument;
            cout << "- initialization file name: " << strIniFileName << std::endl;
            continue;
        }


        // connect on startup --------------------------------------------------
        if ( GetFlagArgument ( argc, argv, i, "-c", "--connect" ) )
        {
            bConnectOnStartup = true;
            cout << "- connect on startup enabled" << std::endl;
            continue;
        }


        // help (usage) flag ---------------------------------------------------
        if ( ( !strcmp ( argv[i], "--help" ) ) ||
             ( !strcmp ( argv[i], "-h" ) ) || ( !strcmp ( argv[i], "-?" ) ) )
        {
            const std::string strHelp = UsageArguments(argv);
            cout << strHelp;
            exit ( 1 );
        }

        // unknown option ------------------------------------------------------
        cerr << argv[0] << ": ";
        cerr << "Unknown option '" << argv[i] << "' -- use '--help' for help"
            << endl;

        exit ( 1 );
    }

    // Application object
    QApplication app ( argc, argv, bUseGUI );

#ifdef _WIN32
    // Set application priority class -> high priority
    SetPriorityClass ( GetCurrentProcess(), HIGH_PRIORITY_CLASS );

    // For accessible support we need to add a plugin to qt. The plugin has to
    // be located in the install directory of llcon by the installer. Here, we
    // set the path to our application
    QDir ApplDir ( QApplication::applicationDirPath() );
    app.addLibraryPath ( QString ( ApplDir.absolutePath() ) );
#endif

    // init resources
#if defined ( __APPLE__ ) || defined ( __MACOSX )
    Q_INIT_RESOURCE(resources);
#else
    extern int qInitResources();
    qInitResources();
#endif

// TEST -> activate the following line to activate the test bench,
//CTestbench Testbench ( "127.0.0.1", LLCON_DEFAULT_PORT_NUMBER );


    try
    {
        if ( bIsClient )
        {
            // client
            // actual client object
            CClient Client ( iPortNumber );

            // load settings from init-file
            CSettings Settings ( &Client );


// TODO use QString

            Settings.Load ( strIniFileName.c_str() );

            // GUI object
            CLlconClientDlg ClientDlg (
                &Client, bConnectOnStartup, bDisalbeLEDs,
                0
#ifdef _WIN32
                // this somehow only works reliable on Windows
                , Qt::WindowMinMaxButtonsHint
#endif
                );

            // set main window
            pMainWindow = &ClientDlg;
            pApp = &app; // Needed for post-event routine

            // show dialog
            ClientDlg.show();
            app.exec();

            // save settings to init-file

// TODO use QString

            Settings.Save ( strIniFileName.c_str() );
        }
        else
        {
            // server
            // actual server object

// TODO use QString

            CServer Server ( strLoggingFileName.c_str(),
                             iPortNumber,
                             strHTMLStatusFileName.c_str(),
                             strHistoryFileName.c_str(),
                             strServerName.c_str() );

            if ( bUseGUI )
            {
                // GUI object for the server
                CLlconServerDlg ServerDlg ( &Server, 0 );

                // set main window
                pMainWindow = &ServerDlg;
                pApp = &app; // needed for post-event routine

                // show dialog
                ServerDlg.show();
                app.exec();
            }
            else
            {
                // only start application without using the GUI
                cout <<
                    CAboutDlg::GetVersionAndNameStr ( false ).toStdString() <<
                    std::endl;

                app.exec();
            }
        }
    }

    catch ( CGenErr generr )
    {
        // show generic error
        if ( bUseGUI )
        {
            QMessageBox::critical (
                0, APP_NAME, generr.GetErrorText(), "Quit", 0 );
        }
        else
        {
            qDebug() << generr.GetErrorText();
        }
    }

    return 0;
}


/******************************************************************************\
* Command Line Argument Parsing                                                *
\******************************************************************************/
std::string UsageArguments ( char **argv )
{
    return
        "Usage: " + std::string ( argv[0] ) + " [option] [argument]\n"
        "Recognized options:\n"
        "  -s, --server               start server\n"
        "  -n, --nogui                disable GUI (only avaiable for server)\n"
        "  -l, --log                  enable logging, set file name\n"
        "  -i, --inifile              initialization file name (only available for\n"
        "                             client)\n"
        "  -p, --port                 local port number (only avaiable for server)\n"
        "  -m, --htmlstatus           enable HTML status file, set file name (only\n"
        "                             available for server)\n"
        "  -a, --servername           server name required for HTML status (only\n"
        "                             available for server)\n"
        "  -y, --history              enable connection history and set file\n"
        "                             name (only available for server)\n"
        "  -c, --connect              connect to last server on startup (only\n"
        "                             available for client)\n"
        "  -d, --disableleds          disable LEDs in main window (only available\n"
        "                             for client)\n"
        "  -h, -?, --help             this help text\n"
        "Example: " + std::string ( argv[0] ) + " -l -inifile myinifile.ini\n";
}

bool GetFlagArgument ( int argc,
                       char** argv,
                       int& i,
                       std::string strShortOpt,
                       std::string strLongOpt )
{
    if ( ( !strShortOpt.compare ( argv[i] ) ) || ( !strLongOpt.compare ( argv[i] ) ) )
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool GetStringArgument ( int argc,
                         char** argv,
                         int& i,
                         std::string strShortOpt,
                         std::string strLongOpt,
                         std::string& strArg )
{
    if ( ( !strShortOpt.compare ( argv[i] ) ) || ( !strLongOpt.compare ( argv[i] ) ) )
    {
        if ( ++i >= argc )
        {
            cerr << argv[0] << ": ";
            cerr << "'" << strLongOpt << "' needs a string argument" << endl;
            exit ( 1 );
        }

        strArg = argv[i];

        return true;
    }
    else
    {
        return false;
    }
}

bool GetNumericArgument ( int argc,
                          char** argv,
                          int& i,
                          std::string strShortOpt,
                          std::string strLongOpt,
                          double rRangeStart,
                          double rRangeStop,
                          double& rValue)
{
    if ( ( !strShortOpt.compare ( argv[i] ) ) || ( !strLongOpt.compare ( argv[i] ) ) )
    {
        if ( ++i >= argc )
        {
            cerr << argv[0] << ": ";
            cerr << "'" << strLongOpt << "' needs a numeric argument between "
                << rRangeStart << " and " << rRangeStop << endl;
            exit ( 1 );
        }

        char *p;
        rValue = strtod ( argv[i], &p );
        if ( *p || rValue < rRangeStart || rValue > rRangeStop )
        {
            cerr << argv[0] << ": ";
            cerr << "'" << strLongOpt << "' needs a numeric argument between "
                << rRangeStart << " and " << rRangeStop << endl;
            exit ( 1 );
        }

        return true;
    }
    else
    {
        return false;
    }
}


/******************************************************************************\
* Window Message System                                                        *
\******************************************************************************/
void PostWinMessage ( const _MESSAGE_IDENT MessID,
                      const int iMessageParam,
                      const int iChanNum )
{
    // first check if application is initialized
    if ( pApp != NULL )
    {
        CLlconEvent* LlconEv =
            new CLlconEvent ( MessID, iMessageParam, iChanNum );

        // Qt will delete the event object when done
        QCoreApplication::postEvent ( pMainWindow, LlconEv );
    }
}
