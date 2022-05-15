#include "remotefiletablemodel.h"
#define DEBUG 1
#include "AEUtils.h"

#include <QStyle>

RemoteFileTableModel::RemoteFileTableModel(  QSharedPointer<DirectoryListing> directoryListing, QObject *parent)
    : QAbstractTableModel{parent},
      m_HeaderNames( ),
      m_Index(),
      m_DirectoryListing(),
      m_FileList( ),
      m_SortColumn( SORT_NAME ),
      m_ReverseOrder( false ),
      m_ShowInfoFiles( false )
{
    //Setup the header names
    m_HeaderNames << "Name" << "Type" << "Size";

    //Do the initial sorting
    updateListing( directoryListing );
}

int RemoteFileTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED( parent )
    return m_FileList.size();
}

int RemoteFileTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED( parent )
    return m_HeaderNames.size();
}

static inline QString prettyFileSize( qint32 size )
{
#define GigaByte 1024*1024*1024
#define MegaByte 1024*1024
#define KiloByte 1024

    if( size > GigaByte )
    {
        return QString( QString::number( size / GigaByte, 10 ) + QString( " GBytes" ) );
    }
    if( size > MegaByte )
    {
        return QString( QString::number( size / MegaByte, 10 ) + QString( " MBytes" ) );
    }
    if( size > KiloByte )
    {
        return QString( QString::number( size / KiloByte, 10 ) + QString( " KBytes" ) );
    }

    return QString( QString::number( size, 10 ) + QString( " Bytes" ) );
}

QVariant RemoteFileTableModel::data(const QModelIndex &index, int role) const
{
    //Get the directory listing in question
    QSharedPointer<DirectoryListing> directoryListing = m_Index[ index ];

    switch( role )
    {
        case Qt::DisplayRole:
        {
            //Get the text
            if( directoryListing.isNull() )
                return QVariant();
            if( index.column() == 0 )
                return directoryListing->Name();
            if( index.column() == 1 )
            {
                switch( directoryListing->Type() )
                {
                    case DET_ROOT:
                    {
                        return QString( "root" );
                        break;
                    }
                    case DET_USERDIR:
                    {
                        return QString( "directory" );
                        break;
                    }
                    case DET_SOFTLINK:
                    {
                        return QString( "softlink" );
                        break;
                    }
                    case DET_LINKDIR:
                    {
                        return QString( "linkdir" );
                        break;
                    }
                    case DET_FILE:
                    {
                        return QString( "file" );
                        break;
                    }
                    case DET_LINKFILE:
                    {
                        return QString( "linkfile" );
                        break;
                    }
                    case DET_PIPEFILE:
                    {
                        return QString( "pipefile" );
                        break;
                    }
                    default:
                    {
                        return QString( "unknown type" );
                        break;
                    }
                }
            }
            if( index.column() == 2 )
                return prettyFileSize( directoryListing->Size() );
            break;
        }
        case Qt::DecorationRole:
        {
            //Get the icon
            if( directoryListing.isNull() || index.column() != 0 )
                return QVariant();
            return directoryListing->Icon()->toImage();
            break;
        }
        case Qt::EditRole:
        case Qt::ToolTipRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
        case Qt::SizeHintRole:
        {
            //DBGLOG << "Getting size hint.";
            //DBGLOG << "Icon size is: " << directoryListing->Icon()->size();
            return directoryListing->Icon()->size();
            break;
        }
        case Qt::FontRole:
        case Qt::TextAlignmentRole:
        case Qt::BackgroundRole:
        case Qt::ForegroundRole:
        case Qt::CheckStateRole:
        case Qt::InitialSortOrderRole:
        default:
            return QVariant();
        break;
    }

    return QVariant();
}

bool RemoteFileTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    Q_UNUSED( index )
    Q_UNUSED( value )
    Q_UNUSED( role )

    //This method doesn't make sense here
    return false;
}

QVariant RemoteFileTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    //Simply ignore orientation vertical
    if( orientation == Qt::Vertical )
        return QVariant();

    switch( role )
    {
        case Qt::DisplayRole:
        {
            //Get the text
            if( section < m_HeaderNames.size() )
            {
                return m_HeaderNames[ section ];
            }
            break;
        }
        case Qt::SizeHintRole:

        break;
        case Qt::DecorationRole:
        {
            //Is this the currently selected section?
            if( section == m_SortColumn )
            {
                if( m_ReverseOrder )
                {
                    QIcon icon( QIcon::fromTheme( "arrow-up" ) );
                    return icon;
                }else
                {
                    QIcon icon( QIcon::fromTheme( "arrow-down" ) );
                    return icon;
                }
            }
            break;
        }
        case Qt::EditRole:
        case Qt::ToolTipRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
        case Qt::FontRole:
        case Qt::TextAlignmentRole:
        case Qt::BackgroundRole:
        case Qt::ForegroundRole:
        case Qt::CheckStateRole:
        case Qt::InitialSortOrderRole:
        default:
        break;
    }

    return QVariant();
}

Qt::ItemFlags RemoteFileTableModel::flags(const QModelIndex &index) const
{
    Q_UNUSED( index )
    Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
    return flags;
}

void RemoteFileTableModel::updateListing(QSharedPointer<DirectoryListing> newListing )
{
    //Add the new listing
    m_DirectoryListing = newListing;

    //Produce a file list
    m_FileList = m_DirectoryListing->Entries();

    //If we aren't to show any info files, remove these entries now
    if( !m_ShowInfoFiles )
    {
        QVectorIterator<QSharedPointer<DirectoryListing>> iter(m_FileList);
        while( iter.hasNext() )
        {
            auto entry = iter.next();
            if( entry->Name().endsWith( ".info" ) )
            {
                m_FileList.removeAll( entry );
            }
        }
    }

    //Perform the sorting
    sortEntries();
}

QSharedPointer<DirectoryListing> RemoteFileTableModel::getDirectoryListingForIndex(const QModelIndex &index) const
{
    if( m_Index.contains( index ) )
        return m_Index[ index ];
    return nullptr;
}

void RemoteFileTableModel::showInfoFiles(bool enable)
{
    m_ShowInfoFiles = enable;
    updateListing( m_DirectoryListing );
}

void RemoteFileTableModel::sortEntries()
{
    //Perform the sort
    if( m_ReverseOrder )
    {
        switch( m_SortColumn )
        {
            case SORT_NAME:
                std::sort( m_FileList.begin(), m_FileList.end(), []( QSharedPointer<DirectoryListing> a, QSharedPointer<DirectoryListing> b )
                {
                    return a->Name() > b->Name();
                });
            break;
            case SORT_TYPE:
                std::sort( m_FileList.begin(), m_FileList.end(), []( QSharedPointer<DirectoryListing> a, QSharedPointer<DirectoryListing> b )
                {
                    return a->Type() > b->Type();
                });
            break;
            case SORT_SIZE:
                std::sort( m_FileList.begin(), m_FileList.end(), []( QSharedPointer<DirectoryListing> a, QSharedPointer<DirectoryListing> b )
                {
                    return a->Size() > b->Size();
                });
            break;
        }
    }else
    {
        switch( m_SortColumn )
        {
            case SORT_NAME:
                std::sort( m_FileList.begin(), m_FileList.end(), []( QSharedPointer<DirectoryListing> a, QSharedPointer<DirectoryListing> b )
                {
                    return a->Name() < b->Name();
                });
            break;
            case SORT_TYPE:
                std::sort( m_FileList.begin(), m_FileList.end(), []( QSharedPointer<DirectoryListing> a, QSharedPointer<DirectoryListing> b )
                {
                    return a->Type() < b->Type();
                });
            break;
            case SORT_SIZE:
                std::sort( m_FileList.begin(), m_FileList.end(), []( QSharedPointer<DirectoryListing> a, QSharedPointer<DirectoryListing> b )
                {
                    return a->Size() < b->Size();
                });
            break;
        }
    }

    //Now we need to inform the view that the current entries will be removed (because we need to re-add them in the new order)
    beginRemoveRows( QModelIndex(), 0, m_FileList.size() - 1  );
    m_Index.clear();
    endRemoveRows();

    //Now we add them back in the new order
    QVectorIterator<QSharedPointer<DirectoryListing>> indexIter( m_FileList );
    beginInsertRows( QModelIndex(), 0, m_FileList.size() - 1 );
    quint32 entryCount = 0;
    while( indexIter.hasNext() )
    {
        QSharedPointer<DirectoryListing> entry = indexIter.next();
        QModelIndex nameIndex = createIndex( entryCount, 0 );
        m_Index[ nameIndex ] = entry;
        QModelIndex typeIndex = createIndex( entryCount, 1 );
        m_Index[ typeIndex ] = entry;
        QModelIndex sizeIndex = createIndex( entryCount, 2 );
        m_Index[ sizeIndex ] = entry;
        entryCount++;
    }
    endInsertRows();
}

void RemoteFileTableModel::onHeaderSectionClicked( int section )
{
    //If the section that has been clicked as the same as already slected, we just need to switch the oder of the sort
    if( m_SortColumn == section )
    {
        m_ReverseOrder = !m_ReverseOrder;
    }else
    {
        m_ReverseOrder = false;
        m_SortColumn = static_cast<SortColumn>( section );
    }

    //Now redo the sort
    sortEntries();
}

QSharedPointer<DirectoryListing> RemoteFileTableModel::getRootDirectoryListing() const
{
    return m_DirectoryListing;
}
