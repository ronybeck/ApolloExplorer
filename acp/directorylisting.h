#ifndef DIRECTORYLISTING_H
#define DIRECTORYLISTING_H

#include <QObject>
#include <QSharedPointer>
#include <QVector>

#include "protocolTypes.h"

class DirectoryListing : public QObject
{
    Q_OBJECT

public:
    explicit DirectoryListing(QObject *parent = nullptr);
    ~DirectoryListing();

    //Updating entries
    void populate( ProtocolMessageDirectoryList_t *newListing );
    void removeEntry( QSharedPointer<DirectoryListing> );
    void removeEntry( QString name );
    void addEntry( QSharedPointer<DirectoryListing> );

    //Querying entries
    QSharedPointer<DirectoryListing> findEntry( QString name, bool recursive = false );

    //Entry properties
    const QString &Name() const;
    void setName(const QString &newName);
    const QString &Path() const;
    void setPath(const QString &newPath);
    quint32 Size() const;
    void setSize(quint32 newSize);
    quint32 Type() const;
    void setType(quint32 newType);
    bool isDirectory();
    const QVector<QSharedPointer<DirectoryListing> > &Entries() const;

    //DirectoryListing& operator=(const DirectoryListing &other );
    bool operator>(const DirectoryListing &other );
    bool operator<(const DirectoryListing &other );

    const QString &Parent() const;
    void setParent(const QString &newParent);

private:
    void reformPath();

signals:

private:
    QString m_Name;
    QString m_Path;
    QString m_Parent;
    quint32 m_Size;
    quint32 m_Type;
    QVector<QSharedPointer<DirectoryListing>> m_Entries;
};

#endif // DIRECTORYLISTING_H
