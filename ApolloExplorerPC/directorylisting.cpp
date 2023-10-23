#include "directorylisting.h"

#include <QFileInfo>
#include <QDebug>
#include <QtEndian>
#include <algorithm>

#define DEBUG 1
#include "AEUtils.h"


DirectoryListing::DirectoryListing(QObject *parent) :
    QObject(parent),
    m_Name( "" ),
    m_Path( "" ),
    m_Size( 0 ),
    m_Type(  ),
    m_Icon( nullptr )
{

}

DirectoryListing::~DirectoryListing()
{
    //DBGLOG << "Deleting point " << this;
    if( m_Icon )    delete m_Icon;
    m_Icon = nullptr;
}

void DirectoryListing::populate( ProtocolMessageDirectoryList_t *newListing )
{
    //Sanity check
    if( newListing == nullptr )
        return;

    //Extract the neccessary details for now
    quint32 entryCount = qFromBigEndian<quint32>( newListing->entryCount );
    //quint32 newListingSize = qFromBigEndian<quint32>( newListing->header.length );
    ProtocolMessage_DirEntry_t *entry = reinterpret_cast<ProtocolMessage_DirEntry_t*>( &newListing->entries[ 0 ] );
    //qDebug() << "New Directory Listing contains " << entryCount << " entries.";

    //Go through the supplied list
    for( unsigned int index = 0;  index < entryCount; index++ )
    {
        //Some endian conversion
        //entry->filenameLength = qFromBigEndian<quint32>( entry->filenameLength );
        entry->type = qFromBigEndian<quint32>( entry->type );
        entry->entrySize = qFromBigEndian<quint32>( entry->entrySize );
        QString convertedName = convertFromAmigaTextEncoding( entry->filename );
        QFileInfo fileInfo( convertedName );

#if 0
        //Debug
        qDebug() << "New Entry " << index;
        qDebug() << "Filename: " << static_cast<char*>( entry->filename );
        qDebug() << "File type: " << QByteArray( entry->type, 4).toHex();
        qDebug() << "File size: " << entry->size;
        qDebug() << "EntrySize: " << entry->entrySize;
#endif

        //The first element contains the details of this directory
        if( index == 0 )
        {
            setPath( convertedName );
            //setName( fileInfo.fileName() );
            setType( 0 );
            setSize( 0 );

            //Time for the next one
            entry = reinterpret_cast<ProtocolMessage_DirEntry_t*>( ((char*)entry) + entry->entrySize );
            continue;
        }


        //Create a new DirectoryListing
        DirectoryListing *dirList = new DirectoryListing();

        //Extract the necessary information
        //dirList->setName( fileInfo.fileName() );
        dirList->setType( entry->type );
        dirList->setSize( qFromBigEndian<quint32>( entry->size ) );

        //Set the path
        if( Path().endsWith( ':' ) )
            dirList->setPath( Path() + convertedName );
        else
            dirList->setPath( Path() + "/" + convertedName );
        //qDebug() << "Added entry '" << dirList->Name() << "' of size " << dirList->Size() << " which is of type " << QByteArray( dirList->Type(), 4).toHex();

        //The icon needs to be added as well
        if( entry->type == DET_USERDIR )
            dirList->setIcon( new QPixmap( "://browser/icons/directory.png" ) );
        else
            dirList->setIcon( new QPixmap( "://browser/icons/file.png" ) );

        //Store the resulting entry in our list
        QSharedPointer<DirectoryListing> newEntry( dirList );
        m_Entries.push_back( newEntry );

        //Calculate the pointer for the next entry
        entry = reinterpret_cast<ProtocolMessage_DirEntry_t*>( ((char*)entry) + entry->entrySize );
    }
}

QSharedPointer<DirectoryListing> DirectoryListing::findEntry(QString name, bool recursive )
{
    QVectorIterator<QSharedPointer<DirectoryListing>> iter( m_Entries );
    while( iter.hasNext() )
    {
        QSharedPointer<DirectoryListing> nextEntry = iter.next();
        if( nextEntry->Name() == name )
            return nextEntry;

        if( nextEntry->Path() == name )
            return nextEntry;

        //If this is a directory and recursion is activated, go into that directory
        if( nextEntry->isDirectory() && recursive )
        {
            QSharedPointer<DirectoryListing> subdirEntry = nextEntry->findEntry( name, recursive );
            if( subdirEntry != nullptr )
                return subdirEntry;
        }

    }
    return nullptr;
}

const QString &DirectoryListing::Name() const
{
    return m_Name;
}

void DirectoryListing::setName(const QString &newName)
{
    m_Name = newName;
    reformPath();
}

const QString &DirectoryListing::Path() const
{
    return m_Path;
}

void DirectoryListing::setPath(const QString &newPath)
{
    m_Path = newPath;

    //We need to extract the parent path from this.

    //Drives don't have parents though
    if( m_Path.endsWith( ":" ) )
    {
        m_Parent = "";  //This is a drive hence no parent.
        m_Name = m_Path;
        m_Name.chop( 1 );   //Remove the trailing ":"
        m_Type = DET_ROOT;
        return;
    }

    //No ":" or "/" characters means this is a drive
    if( !m_Path.contains( "/" ) && !m_Path.contains( ":" ) )
    {
        m_Name = m_Path;
        m_Path += ":";
        m_Parent = "";
        m_Type = DET_ROOT;
        return;
    }

    //Directories hopefully have a trailing "/" so we should remove that
    if( m_Path.endsWith( "/" ) )
    {
        m_Type = DET_USERDIR;
        m_Path.chop( 1 );
    }
    m_Parent = m_Path;

    //Chop off the last component of the path after the (2nd) last ":" or "/"
    QRegExp parentEnd( "[:/]" );
    quint32 parentEndPos = m_Parent.lastIndexOf( parentEnd );
    m_Name = m_Parent.right( m_Parent.length() - parentEndPos - 1);
    m_Parent.chop( m_Parent.length() - parentEndPos - 1 );
}

quint32 DirectoryListing::Size() const
{
    return m_Size;
}

void DirectoryListing::setSize(quint32 newSize)
{
    m_Size = newSize;
}

quint32 DirectoryListing::Type() const
{
    return m_Type;
}

void DirectoryListing::setType(quint32 newType)
{
    m_Type = newType;
}

QPixmap *DirectoryListing::Icon() const
{
    return m_Icon;
}

void DirectoryListing::setIcon(QPixmap *newIcon)
{
    m_Icon = newIcon;
}

bool DirectoryListing::isDirectory()
{
    if( m_Type == DET_USERDIR ) return true;
    return false;
}

const QVector<QSharedPointer<DirectoryListing> > &DirectoryListing::Entries() const
{
    return m_Entries;
}

/*
DirectoryListing& DirectoryListing::operator=(const DirectoryListing &other)
{
    DirectoryListing newListing;
    return *this;
}
*/

bool DirectoryListing::operator>(const DirectoryListing &other)
{
    return this->Name() > other.Name();
}

bool DirectoryListing::operator<(const DirectoryListing &other)
{
    return this->Name() < other.Name();
}


const QString &DirectoryListing::Parent() const
{
    return m_Parent;
}

void DirectoryListing::setParent(const QString &newParent)
{
    m_Parent = newParent;
    reformPath();
}

void DirectoryListing::reformPath()
{
    //If parent is a directory
    if( m_Parent.endsWith( "/" ) )
    {
        m_Path = m_Parent + m_Name;
        return;
    }

    //If the parent is a drive
    if( m_Parent.endsWith( ":" ) )
    {
        m_Path = m_Parent + m_Name;
        return;
    }

    //If it isn't clear based on the last character but has at least one ":" in the path.....
    if( m_Parent.contains( ":" ) )
    {
        //This will be a directory for sure
        m_Path = m_Parent + "/" + m_Name;
        m_Parent += "/";
        return;
    }

    //If this parent is empty i.e. this is a drive......
    if( m_Parent.length() == 0 || m_Type == DET_ROOT )
    {
        m_Path = m_Name + ":";
        m_Parent = "";
        return;
    }

    //There is only one posibility left.  The parent is a drive
    m_Path = m_Parent + ":" + m_Name;
    m_Parent += ":";
}

QSharedPointer<AmigaInfoFile> DirectoryListing::getAmigaInfoFile() const
{
    return m_AmigaInfoFile;
}

void DirectoryListing::setAmigaInfoFile( QSharedPointer<AmigaInfoFile> newAmigaInfoFile, quint32 iconHeight )
{
    m_AmigaInfoFile = newAmigaInfoFile;

    //If we don't have a pixmap already
    if( m_Icon != nullptr ) delete m_Icon;

    //We should get the icon out of the info file and set that as our own
    //Get the best image we can for the icon
    if( m_AmigaInfoFile->getBestImage1().width() > 0 )
    {
        m_Icon = new QPixmap( QPixmap::fromImage( m_AmigaInfoFile->getBestImage1().scaledToHeight( iconHeight, Qt::SmoothTransformation ) ) );
        return;
    }
}

