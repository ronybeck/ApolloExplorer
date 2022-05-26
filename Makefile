CC=/opt/amiga/bin/m68k-amigaos-gcc
CFLAGS=-std=c99 -O0
#works
AE_CC=/opt/amiga/bin/m68k-amigaos-gcc
AE_CFLAGS=-std=c99 -O0 -Wall -fstack-protector-all -Wno-pointer-sign
AE_Includes=-I/opt/amiga/m68k-amigaos/ndk-include/

TARGET=ApolloExplorerServerAmiga
AMIGA_SRC=	Amiga/ApolloExplorerServerAmiga.c \
			Amiga/AEServerThread.c \
			Amiga/AEDiscoveryThread.c \
			Amiga/AEUtil.c \
			Amiga/protocol.c \
			Amiga/DirectoryList.c \
			Amiga/SendFile.c \
			Amiga/ReceiveFile.c \
			Amiga/MakeDir.c \
			Amiga/VolumeList.c

${TARGET}: 
	$(AE_CC) ${AE_CFLAGS} ${AE_Includes} -I./Amiga/ -I. ${AMIGA_SRC} -lamiga -o ${TARGET}
	
all: clean ${}
	
clean:
	rm -f ${TARGET}
