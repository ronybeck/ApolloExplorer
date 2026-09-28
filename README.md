# ApolloExplorer

ApolloExplorer is a fast and easy to use GUI based file sharing tool to transfer files between your PC/Mac and your Amiga.
On the Amiga side the ApolloExplorer Server runs in the background and allows ApolloExplorer Clients on PC/Mac to connect.
Once connected files can be transfer between PC/Mac and Amiga by means of a simple "drag and drop".

ACP and ASH are two additional command line tools to use in a terminal window on your PC/Mac.

**ACP** can copy files between PC/Mac and Amiga, same as ApolloExplorer GUI Client. Type `acp --help` for more info.

**ASH** opens a full interactive remote shell connection from your PC/Mac to your Amiga. TYpe `ash --help` for more info.


# License

This is released und the MIT license.

# Screen Shots
![Icons](/ApolloExplorerPC/images/AE_Icon_Example.png)

![acp](/ApolloExplorerPC/images/acp_exampl.png)

# Amiga Server Install

1. Open a CLI Terminal window
2. Copy ApolloExplorerSrv and ApolloExplorerTool to C:
3. Type `run apolloexplorersrv' to start the Server
4. Type `apolloexplorertool list' to show connected Clients
5. Type `apolloexplorertool kill' to stop the Server
 
# Linux Client Install (Debian/Ubuntu)

1. Open a Terminal window
2. Install QT6: `sudo apt install qt6-base-dev`
3. Install DPKG: `sudo apt install dpkg`
4. Install ApolloExplorer: `dpkg -i apolloexplorer_1.4.0_amd64.deb`

After installation you will find the ApolloExplorer Client for Linux in your Application collection.
ACP and ASH command line tools are installed in the `usr/bin` system folder.

# macOS Client Install (Intel/Silicon Universal)

1. Doubleclick on ApolloExplorer-1.4.0.pkg
2. If you get the warning "Apple could not verify . . ." then open Settings and Choose then Privacy & Security
3. Click on "Open Anyway" and fill in your local Apple password
If you also want to use the ACP or ASH command line tools, continue:
4. Open a Terminal window
5. Install HomeBrew: `/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"`
6. Install QT6: `brew install qt`
7. Type `sudo install_name_tool -add_rpath /opt/homebrew/lib /usr/local/bin/acp`
8. Type `sudo codesign --force --sign - /usr/local/bin/acp`
9. Type `sudo install_name_tool -add_rpath /opt/homebrew/lib /usr/local/bin/ash`
10. Type `sudo codesign --force --sign - /usr/local/bin/ash`

After installation you will find the ApolloExplorer Client for macOS in the Application folder.
ACP and ASH command line tools are installed in the `usr/local/bin` system folder.

# Windows Client Install

1. Doubleclick on ApolloExplorer-1.4.0 (MSI installer)

After installation you will find the ApolloExplorer Client for Windows in the Programs folder.
ACP and ASH command line tools are installed in the `C:\Program Files\ApolloExplorer` system folder.

# FAQ

1. If have installed server and client but I do not see my Apollo V4 device in the Client window
- Open a terminal window on your ApolloExplorer Windows/Linus/macOS Client and ping your Apollo V4 device to check network connection
- Windows 11 users with virtual box installed: Disable the virtual ethernet connection of virtual box as this conflicts with Apollo explorer
- If server and client are on different subnets, ensure that UDP discovery broadcast is permitted between the two because routers typically do not forward 255.255.255.255 across subnets.
