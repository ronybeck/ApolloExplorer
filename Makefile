CC=/opt/amiga/bin/m68k-amigaos-gcc
AE_CC=/opt/amiga/bin/m68k-amigaos-gcc
AE_CFLAGS=-std=c99 -O0 -Wall -fstack-protector-all -Wno-pointer-sign
AE_Includes=-I/opt/amiga/m68k-amigaos/ndk-include/

ApolloExplorerServer=ApolloExplorerSrv
ApolloExplorerServer_SRC=	Amiga/ApolloExplorerServerAmiga.c \
			Amiga/AEClientThread.c \
			Amiga/AEDiscoveryThread.c \
			Amiga/AEUtil.c \
			Amiga/protocol.c \
			Amiga/DirectoryList.c \
			Amiga/SendFile.c \
			Amiga/ReceiveFile.c \
			Amiga/MakeDir.c \
			Amiga/VolumeList.c
ApolloExplorerTool=ApolloExplorerTool
ApolloExplorerTool_SRC = Amiga/ApolloExplorerTool.c

${ApolloExplorerServer}: 
	$(AE_CC) ${AE_CFLAGS} ${AE_Includes} -I./Amiga/ -I. ${ApolloExplorerServer_SRC} -lamiga -o ${ApolloExplorerServer}
	
${ApolloExplorerTool}:
	$(AE_CC) ${AE_CFLAGS} ${AE_Includes} -I./Amiga/ -I. ${ApolloExplorerTool_SRC} -lamiga -o ${ApolloExplorerTool}
	
all: clean ${ApolloExplorerServer} ${ApolloExplorerTool}
	
clean:
	rm -f ${ApolloExplorerServer} ${ApolloExplorerTool}
