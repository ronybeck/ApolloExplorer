# ApolloCrossDev Makefile for ApolloExplorer
# 17-2-2026
# Willem Drijver

PWD = $(shell pwd)
DIR = $(notdir $(shell pwd))
APOLLO_V4_HOST = "${AMIGAHOST}:Ram Disk/"

PREFIX 	= $(PWD)/../../ApolloCrossDev/Compilers/GCC-6.50-Stable
TARGET 	= m68k-amigaos

C_COMPILER	= $(PREFIX)/bin/m68k-amigaos-gcc

C_LIB		= -noixemul
C_INCL_CLB	= $(PREFIX)/$(TARGET)/libnix/include
C_INCL_SYS	= $(PREFIX)/$(TARGET)/sys-include
C_INCL_NDK  = $(PREFIX)/$(TARGET)/ndk32-include/Include_H
C_INCL_ALL 	= -I. -I$(C_INCL_CLB) -I$(C_INCL_SYS) -I$(C_INCL_NDK)

C_OPTIONS 	= -O2 -m68040 -m68881 -ffast-math -fomit-frame-pointer 
C_FLAGS = $(C_LIB) $(C_OPTIONS) $(C_INCL_ALL)

ApolloExplorerServer		= ApolloExplorerSrv
ApolloExplorerServer_SRC	= ApolloExplorerServerAmiga.c \
							AEClientThread.c \
							AEDiscoveryThread.c \
							AEUtil.c \
							protocol.c \
							DeletePath.c \
							DirectoryList.c \
							SendFile.c \
							ReceiveFile.c \
							MakeDir.c \
							VolumeList.c

ApolloExplorerTool			= ApolloExplorerTool
ApolloExplorerTool_SRC 		= ApolloExplorerTool.c

${ApolloExplorerServer}: 
	@$(C_COMPILER) ${C_FLAGS} -I.. ${ApolloExplorerServer_SRC} -lamiga -o ${ApolloExplorerServer}
	
${ApolloExplorerTool}:
	$(C_COMPILER) ${C_FLAGS} ${ApolloExplorerTool_SRC} -lamiga -o ${ApolloExplorerTool}
	
all: clean ${ApolloExplorerServer} ${ApolloExplorerTool}
	
clean:
	rm -f ${ApolloExplorerServer} ${ApolloExplorerTool}

