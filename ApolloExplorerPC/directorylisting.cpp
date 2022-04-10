#include "directorylisting.h"

#include <QFileInfo>
#include <QDebug>
#include <QtEndian>
#include <algorithm>


#include "VNetPCUtils.h"


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
    if( m_Icon )    delete m_Icon;
}

void DirectoryListing::populate( ProtocolMessageDirectoryList_t *newListing )
{
    //Sanity check
    if( newListing == nullptr )
        return;

    //Extract the neccessary details for now
    quint32 entryCount = qFromBigEndian<quint32>( newListing->entryCount );
    quint32 newListingSize = qFromBigEndian<quint32>( newListing->header.length );
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
        //qDebug() << "Converted text: " << entry->filename << " -----> " << convertedName;
        QFileInfo fileInfo( convertedName );

#if 0
        //Debug
        qDebug() << "New Entry";
        qDebug() << "Filename: " << static_cast<char*>( entry->filename );
        //qDebug() << "Filename Length: " << entry->filenameLength;
        qDebug() << "File type: " << QByteArray( entry->type, 4).toHex();
        qDebug() << "File size: " << entry->size;
        qDebug() << "EntrySize: " << entry->entrySize;
#endif

        //The first element contains the details of this directory
        if( index == 0 )
        {
            setPath( convertedName );
            setName( fileInfo.fileName() );
            setType( 0 );
            setSize( 0 );

            //Time for the next one
            entry = reinterpret_cast<ProtocolMessage_DirEntry_t*>( ((char*)entry) + entry->entrySize );
            continue;
        }


        //Create a new DirectoryListing
        DirectoryListing *dirList = new DirectoryListing( this );

        //Extract the necessary information
        dirList->setName( fileInfo.fileName() );
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
        if( m_Entries.empty())
        {
            m_Entries.push_back( newEntry );
        }else
        {
            int listSize = m_Entries.size();
            bool inserted = false;
            for( int i=0; i < listSize; i++ )
            {
                //Get the next entry in the list
                QSharedPointer<DirectoryListing> nextEntry = m_Entries[ i ];

                //Is this name before our name?
                if( newEntry->Name() <= nextEntry->Name() )
                {
                    m_Entries.insert( i, newEntry );
                    inserted = true;
                    break;
                }
            }

            //If we didn't get to insert it then we need to push it to the back
            if( !inserted)  m_Entries.push_back( newEntry );
        }


        //Calculate the pointer for the next entry
        entry = reinterpret_cast<ProtocolMessage_DirEntry_t*>( ((char*)entry) + entry->entrySize );
    }
}

QSharedPointer<DirectoryListing> DirectoryListing::findEntry(QString name)
{
    QVectorIterator<QSharedPointer<DirectoryListing>> iter( m_Entries );
    while( iter.hasNext() )
    {
        QSharedPointer<DirectoryListing> nextEntry = iter.next();
        if( nextEntry->Name() == name )
            return nextEntry;
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
}

const QString &DirectoryListing::Path() const
{
    return m_Path;
}

void DirectoryListing::setPath(const QString &newPath)
{
    m_Path = newPath;
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

const QVector<QSharedPointer<DirectoryListing> > &DirectoryListing::Entries() const
{
    return m_Entries;
}

DirectoryListing& DirectoryListing::operator=(const DirectoryListing &other)
{
    DirectoryListing newListing;
    return *this;
}

bool DirectoryListing::operator>(const DirectoryListing &other)
{
    return this->Name() > other.Name();
}

bool DirectoryListing::operator<(const DirectoryListing &other)
{
    return this->Name() < other.Name();
}

