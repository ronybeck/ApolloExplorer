#include <QCoreApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QSettings>
#include <QThread>
#include <QRegularExpression>
#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

#define DEBUG 0
#include "AEUtils.h"
#include "protocolTypes.h"
#include "hostlister.h"
#include "shellsession.h"


// ---------- terminal raw-mode helpers ----------

static struct termios g_SavedTermios;
static bool g_RawModeActive = false;

static void disableRawMode()
{
    if( g_RawModeActive )
    {
        tcsetattr( STDIN_FILENO, TCSAFLUSH, &g_SavedTermios );
        g_RawModeActive = false;
    }
}

static void enableRawMode()
{
    tcgetattr( STDIN_FILENO, &g_SavedTermios );
    atexit( disableRawMode );

    struct termios raw = g_SavedTermios;
    // Disable echo and canonical (line-buffered) mode; keep signals (Ctrl+C works)
    raw.c_lflag &= ~( ECHO | ICANON );
    // Don't translate CR→LF on input
    raw.c_iflag &= ~ICRNL;
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr( STDIN_FILENO, TCSAFLUSH, &raw );
    g_RawModeActive = true;
}

static void sigintHandler( int sig )
{
    disableRawMode();
    write( STDOUT_FILENO, "\r\n", 2 );
    signal( sig, SIG_DFL );
    raise( sig );
}

// ---------- host resolution helpers ----------

static QStringList getHostList( QSharedPointer<QSettings> settings, const QStringList &positionalArguments )
{
    QStringList hosts;

    HostLister hostLister( settings, nullptr );

    bool scanCompleted = false;
    QVector<QSharedPointer<AmigaHost>> hostList;
    QObject::connect( &hostLister, &HostLister::hostsDiscoveredSignal,
                      [&]( QVector<QSharedPointer<AmigaHost>> hostsFound ) {
        scanCompleted = true;
        hostList = hostsFound;
    });

    while( !scanCompleted )
    {
        QThread::msleep( 10 );
        QCoreApplication::processEvents();
    }

    QString searchPattern;
    if( positionalArguments.size() > 0 )
        searchPattern = positionalArguments[ 0 ];

    if( searchPattern.length() > 0 )
    {
        QStringList matches = hostLister.getHostLike( searchPattern );
        for( const auto &match : matches )
            hosts.append( match );
    }
    else
    {
        for( const QSharedPointer<AmigaHost> &host : hostList )
            hosts.append( host->Name() + " " + host->Address().toString() );
    }

    return hosts;
}

static QString resolveHost( QSharedPointer<QSettings> settings, const QString &host )
{
    static const QRegularExpression ipAddressRegExp( "^(?:[0-9]{1,3}\\.){3}[0-9]{1,3}$" );
    if( ipAddressRegExp.match( host ).hasMatch() )
        return host;

    QStringList hosts = getHostList( settings, QStringList{ host } );
    if( hosts.size() != 1 )
    {
        std::cerr << "ash: hostname '" << host.toStdString()
                  << "' is ambiguous or could not be found." << std::endl;
        return QString();
    }

    QStringList parts = hosts[ 0 ].split( " " );
    if( parts.size() != 2 )
    {
        std::cerr << "ash: no IP address found for host '"
                  << host.toStdString() << "'." << std::endl;
        return QString();
    }

    return parts[ 1 ];
}


int main( int argc, char *argv[] )
{
    QCoreApplication app( argc, argv );
    QCoreApplication::setApplicationName( "ash" );
    QCoreApplication::setApplicationVersion( VERSION_STRING );

    qRegisterMetaType<QHostAddress>();

    QSharedPointer<QSettings> settings(
        new QSettings( "ApolloTeam", "ApolloExplorer" ) );

    QCommandLineParser cmdParser;
    cmdParser.setApplicationDescription(
        "ash - Amiga remote shell\n"
        "\n"
        "Usage:\n"
        "  ash -l                     List available Amiga hosts\n"
        "  ash <host> <command>       Execute a single command and print output\n"
        "  ash <host>                 Interactive shell: type commands, Ctrl+D to quit" );
    cmdParser.addHelpOption();
    cmdParser.addVersionOption();

    const QCommandLineOption listHostOption(
        { "l", "list-hosts" }, "List all available Amiga hosts" );
    cmdParser.addOption( listHostOption );
    cmdParser.addPositionalArgument( "host",    "Amiga hostname or IP address" );
    cmdParser.addPositionalArgument( "command", "Command to execute (optional; interactive if omitted)" );

    cmdParser.process( app );

    if( argc < 2 )
    {
        cmdParser.showHelp();
        return 0;
    }

    // -- List hosts --
    if( cmdParser.isSet( listHostOption ) )
    {
        QStringList hosts = getHostList( settings, cmdParser.positionalArguments() );
        std::cout << "ApolloExplorer Hosts found:" << std::endl;
        for( const QString &host : hosts )
            std::cout << "  " << host.toStdString() << std::endl;
        return 0;
    }

    // -- Need at least a hostname --
    QStringList positional = cmdParser.positionalArguments();
    if( positional.isEmpty() )
    {
        cmdParser.showHelp();
        return 1;
    }

    QString hostArg = positional.takeFirst();
    QString ipAddress = resolveHost( settings, hostArg );
    if( ipAddress.isEmpty() )
        return 1;

    ShellSession session( ipAddress );

    std::cout << "Connecting to " << hostArg.toStdString() << " (" << ipAddress.toStdString() << ")..." << std::endl;
    if( !session.connectToHost() )
    {
        std::cerr << "ash: could not connect to " << hostArg.toStdString() << std::endl;
        return 1;
    }
    std::cout << "Connected." << std::endl;

    int exitCode = 0;

    if( !positional.isEmpty() )
    {
        // Single-command mode
        QString command = positional.join( " " );
        exitCode = session.execute( command );
    }
    else if( !isatty( STDIN_FILENO ) )
    {
        // Non-TTY (pipe/script): read lines without raw mode or tab completion
        char lineBuf[4096];
        while( fgets( lineBuf, sizeof(lineBuf), stdin ) )
        {
            QString line = QString::fromLatin1( lineBuf ).trimmed();
            if( line.isEmpty() ) continue;
            exitCode = session.execute( line );
            if( exitCode != 0 )
                std::cerr << "[exit code: " << exitCode << "]\n";
        }
    }
    else
    {
        // Interactive TTY mode with raw input and tab completion
        signal( SIGINT, sigintHandler );
        enableRawMode();

        auto printPrompt = [&]() {
            QString cwd = session.currentDir();
            if( cwd.isEmpty() )
                std::cout << "ash> " << std::flush;
            else
                std::cout << "ash [" << cwd.toStdString() << "]> " << std::flush;
        };

        QString lineBuffer;
        bool lastWasTab = false;

        printPrompt();

        char c;
        while( read( STDIN_FILENO, &c, 1 ) == 1 )
        {
            if( c == '\t' )
            {
                bool isDoubleTab = lastWasTab;
                lastWasTab = true;

                // Single Tab on empty input: do nothing; double Tab: list current dir
                if( lineBuffer.isEmpty() && !isDoubleTab )
                    continue;

                // Completion token = last whitespace-delimited word in the line
                int lastSpace = lineBuffer.lastIndexOf( ' ' );
                QString linePrefix = lineBuffer.left( lastSpace + 1 );  // text before the token
                QString token      = ( lastSpace >= 0 ) ? lineBuffer.mid( lastSpace + 1 ) : lineBuffer;

                QStringList matches = session.requestCompletion( token );

                if( matches.isEmpty() )
                {
                    // No match: bell
                    std::cout << '\a' << std::flush;
                }
                else if( matches.size() == 1 )
                {
                    // Single match: complete in place
                    for( int i = 0; i < lineBuffer.size(); i++ )
                        std::cout << "\b \b";
                    lineBuffer = linePrefix + matches[0];
                    std::cout << lineBuffer.toStdString() << std::flush;
                }
                else
                {
                    // Multiple matches: print them then redraw the prompt + current input
                    std::cout << "\r\n";
                    int col = 0;
                    for( const QString &m : matches )
                    {
                        std::string ms = m.toStdString();
                        std::cout << ms;
                        col += (int)ms.size();
                        // Pad to next 20-char column for readability
                        int pad = 20 - ( col % 20 );
                        for( int i = 0; i < pad; i++ ) std::cout << ' ';
                        col += pad;
                        if( col >= 80 ) { std::cout << "\r\n"; col = 0; }
                    }
                    if( col > 0 ) std::cout << "\r\n";
                    printPrompt();
                    std::cout << lineBuffer.toStdString() << std::flush;
                }
            }
            else
            {
                lastWasTab = false;

                if( c == '\r' || c == '\n' )
                {
                    std::cout << "\r\n" << std::flush;
                    QString trimmed = lineBuffer.trimmed();
                    if( !trimmed.isEmpty() )
                    {
                        exitCode = session.execute( trimmed );
                        if( exitCode != 0 )
                            std::cerr << "[exit code: " << exitCode << "]\r\n";
                    }
                    lineBuffer.clear();
                    printPrompt();
                }
                else if( c == 127 || c == '\b' )
                {
                    if( !lineBuffer.isEmpty() )
                    {
                        lineBuffer.chop( 1 );
                        std::cout << "\b \b" << std::flush;
                    }
                }
                else if( c == 4 )
                {
                    // Ctrl+D: exit
                    std::cout << "\r\n" << std::flush;
                    break;
                }
                else if( c == 27 )
                {
                    // ESC: swallow the rest of the escape sequence (arrow keys etc.)
                    int flags = fcntl( STDIN_FILENO, F_GETFL, 0 );
                    fcntl( STDIN_FILENO, F_SETFL, flags | O_NONBLOCK );
                    char discard[8];
                    while( read( STDIN_FILENO, discard, sizeof(discard) ) > 0 ) {}
                    fcntl( STDIN_FILENO, F_SETFL, flags );
                }
                else if( (unsigned char)c >= 32 && (unsigned char)c < 127 )
                {
                    lineBuffer += c;
                    std::cout << c << std::flush;
                }
                // Other control characters are silently ignored
            }
        }

        disableRawMode();
        std::cout << std::endl;
    }

    session.disconnectFromHost();
    return exitCode;
}
