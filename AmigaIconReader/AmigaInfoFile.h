#ifndef AMIGAINFOTILE_H
#define AMIGAINFOTILE_H

//Much of this work is based on information from:
//http://www.evillabs.net/index.php/Amiga_Icon_Formats
//http://amigadev.elowar.com/
//https://github.com/jsummers/deark/blob/master/modules/amigaicon.c

#include <QObject>
#include <QImage>
#include <QPixmap>

#ifndef __MINGW32__
typedef quint32 APTR;
typedef quint8 UBYTE;
typedef qint32 LONG;
typedef quint32 ULONG;
typedef qint16 WORD;
typedef quint16 UWORD;
typedef qint8 BYTE;
#endif


typedef struct
{
    qint16 LeftEdge;    /* starting offset relative to some origin */
    qint16 TopEdge;     /* starting offsets relative to some origin */
    qint16 Width;       /* pixel size (though data is word-aligned) */
    qint16 Height;
    qint16 Depth;       /* >= 0, for images you create		*/
    quint32 ImageData; /* pointer to the actual word-aligned bits */
    quint8 PlanePick;
    quint8 PlaneOnOff;
    quint32 NextImage;
} __attribute__((packed)) Image_t;

typedef struct {
    quint32 ga_Next;   //always 0
    qint16  ga_LeftEdge;
    qint16  ga_TopEdge;
    qint16  ga_Width;
    qint16  ga_Height;
    quint16 ga_Flags;
    /*
        bit 2 always set (image 1 is an image ;-)
        bit 1 if set, we use 2 image-mode,
        bit 0 if set we use backfill mode, else complement mode
        complement mode: gadget colors are inverted
        backfill mode: like complement, but region
        outside (color 0) of image is not inverted
        As you see, it makes no sense having bit 0 and 1 set.
    */
    quint16 ga_Activation;      //undefined
    quint16 ga_GadgetType;      //<undefined>
    quint32 ga_GadgetRender;    //<boolean> unused??? always true
    quint32 ga_SelectRender;    //<boolean> (true if second image present)
    quint32 ga_GadgetText;      //<undefined> always 0 ???
    qint32  ga_MutualExclude;   //<undefined>
    quint32 ga_SpecialInfo;     //<undefined>
    quint16 ga_GadgetID;        //<undefined>
    quint32 ga_UserData;
} __attribute__((packed)) Gadget_t;

struct NewWindow
{
#if 0
    WORD LeftEdge, TopEdge;		/* screen dimensions of window */
    WORD Width, Height;			/* screen dimensions of window */
    UBYTE DetailPen, BlockPen;		/* for bar/border/gadget rendering */
    ULONG IDCMPFlags;			/* User-selected IDCMP flags */
    ULONG Flags;			/* see Window struct for defines */
    struct Gadget *FirstGadget;
    struct Image *CheckMark;
    UBYTE *Title;			  /* the title text for this window */
    struct Screen *Screen;
    struct BitMap *BitMap;
    WORD MinWidth, MinHeight;	    /* minimums */
    UWORD MaxWidth, MaxHeight;	     /* maximums */
    UWORD Type;
#else
    char padding[48];
#endif

} __attribute__((packed));

typedef struct
{
    quint32 dd_Flags;       /* flags for drawer */
    quint16 dd_ViewModes;	/* view mode for drawer */
} __attribute__((packed)) DrawerData2_t;

typedef struct
{
        struct NewWindow	dd_NewWindow;	/* args to open window */
        qint32  dd_CurrentX;    /* current x coordinate of origin */
        qint32  dd_CurrentY;	/* current y coordinate of origin */
        //DrawerData2_t dd_Extended;
} __attribute__((packed)) DrawerData_t;



typedef enum
{
    DISK = 1,
    DRAWER = 2,
    TOOL = 3,
    PROJECT = 4,
    GARBAGE = 5,
    DEVICE = 6,
    KICK = 7,
    APPICON = 8
} __attribute__((packed)) IconType_t;

typedef struct {
    quint16     do_Magic;           //magic number at start of file. Always 0xE310
    quint16     do_Version;         //Always 1
    Gadget_t    do_Gadget;
    quint8      do_Type;
    quint8      pad1;
    quint32     do_DefaultTool;    //To be interpreted from disk as a boolean
    quint32     do_ToolTypes;     //To be interpreted from disk as a boolean
    qint32      do_CurrentX;
    qint32      do_CurrentY;
    quint32     do_DrawerData;
    quint32     do_ToolWindow;     //only applies to tools
    qint32      do_StackSize;       //only applies to tools

    //DrawerData_t      drawData;       //if ic_DrawerData is not zero (see below)
    //Image_t           firstImage;
    //Image_t           secondImage;
    //DefaultTool       text            if ic_DefaultTool not zero (format see below)
    //ToolTypes         texts           if ic_ToolTypes not zero (format see below)
    //ToolWindow        text            if ic_ToolWindow not zero (format see below)
    //                                  this is an extension, which was never implemented
    //struct DrawerData2
} __attribute__((packed)) OS2Icon_t;

class AmigaInfoFile : public QObject
{
    Q_OBJECT


public:
    AmigaInfoFile();

    //Load an Amiga .info file
    bool loadFile( QString path );

    //Process the files contents
    bool process( QByteArray data );

    //Properties of the icon
    IconType_t getType();
    void setType( IconType_t type );
    quint16 getVersion();
    void setVersion( quint16 version );
    quint32 getStackSize();
    void setStackSize( quint32 size );
    QString getDefaultTool();
    void setDefaultTool( QString text );
    QVector<QString> getToolTypes();
    void getOS2IconParameters( quint8 &depth, quint32 &width, quint32 &height );
    qint16 getPriority();

    //icon Decoding
    QByteArray decodeOS2Icon( QByteArray data, quint64 &offset, quint8 depth, quint32 width, quint32 height );
    void decodeOS35Icon(QByteArray data, quint64 &offset, QByteArray &imageData, QByteArray &paletteData, quint32 &imageWidth, quint32 &imageHeight, quint8 &colourCount, qint16 &transparentColour );

    //Icon conversion
    QImage drawIndexedOS2Icon( QByteArray data, quint32 width, quint32 height );
    QImage drawIndexedOS35Icon( QByteArray imageData, QByteArray paletteData, quint8 numberOfColours, quint32 width, quint32 height, qint16 transparentColour );

    //Get the icons
    QImage getOS2IconImage1();
    QImage getOS2IconImage2();
    QImage getOS35Image1();
    QImage getOS35Image2();
    QImage getBestImage1();
    QImage getBestImage2();
    bool hasImage();

    //Reset
    void reset();

private:
    OS2Icon_t m_Icon;
    DrawerData_t m_DrawerData;
    DrawerData2_t m_DrawerData2;
    Image_t m_Image1;
    Image_t m_Image2;
    QImage m_OS2Image1;
    QImage m_OS2Image2;
    QImage m_OS35Image1;
    QImage m_OS35Image2;
    QString m_DefaultTool;
    QVector<QString> m_ToolTypes;
    qint16 m_Priority;
    QString m_ToolWindow;
    bool m_HasImage;

};

#endif // OS2ICON_H
