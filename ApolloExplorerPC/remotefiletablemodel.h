#ifndef REMOTEFILETABLEMODEL_H
#define REMOTEFILETABLEMODEL_H

#include "directorylisting.h"

#include <QAbstractTableModel>
#include <QObject>
#include <QVector>
#include <QMutexLocker>

class RemoteFileTableModel : public QAbstractTableModel
{
    Q_OBJECT

    typedef enum
    {
        SORT_NAME = 0,
        SORT_TYPE = 1,
        SORT_SIZE = 2
    } SortColumn;

public:
    explicit RemoteFileTableModel( QSharedPointer<DirectoryListing> directoryListing, QObject *parent = nullptr );

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    virtual Qt::ItemFlags flags( const QModelIndex& index) const override;

    void updateListing( QSharedPointer<DirectoryListing> newListing );
    QSharedPointer<DirectoryListing> getDirectoryListingForIndex( const QModelIndex &index ) const;
    void showInfoFiles( bool enable );
    void sortEntries();
    void getHeaderSelection( int &column, bool &reversed );

    QSharedPointer<DirectoryListing> getRootDirectoryListing() const;

public slots:
    void onHeaderSectionClicked( int section );

private:
    QMutex *m_Mutex;
    QStringList m_HeaderNames;
    QMap<QModelIndex,QSharedPointer<DirectoryListing>> m_Index;
    QSharedPointer<DirectoryListing> m_DirectoryListing;
    QVector<QSharedPointer<DirectoryListing>> m_FileList;
    SortColumn m_SortColumn;
    bool m_ReverseOrder;
    bool m_ShowInfoFiles;
};

#endif // REMOTEFILETABLEMODEL_H
