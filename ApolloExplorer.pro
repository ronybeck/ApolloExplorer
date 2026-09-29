TEMPLATE=subdirs
SUBDIRS=ApolloExplorerPC AmigaIconReader \
    acp

# ash is an interactive terminal client built on POSIX termios/tcsetattr and
# raw STDIN_FILENO reads, none of which exist on Windows. It was never built
# by build_windows_client.ps1 either; porting it to the Windows Console API
# is a separate, larger effort.
!win32 {
    SUBDIRS += ash
}
