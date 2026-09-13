#include <QtTest>

#include "directorylisting.h"

class DirectoryListing_SetPath : public QObject
{
    Q_OBJECT

private slots:
    void driveDirectoryPath();
    void driveRootPath();
    void nestedDirectoryPath();
    void setNameReformsPath();
    void setParentReformsPath();

};

void DirectoryListing_SetPath::driveDirectoryPath()
{
    DirectoryListing listing;
    listing.setPath(QStringLiteral("test:dir"));
    QCOMPARE(listing.Path(), QStringLiteral("test:dir"));
    QCOMPARE(listing.Parent(), QStringLiteral("test:"));
    QCOMPARE(listing.Name(), QStringLiteral("dir"));
}

void DirectoryListing_SetPath::driveRootPath()
{
    DirectoryListing listing;
    listing.setPath(QStringLiteral("test"));
    QCOMPARE(listing.Path(), QStringLiteral("test:"));
    QCOMPARE(listing.Parent(), QString());
    QCOMPARE(listing.Name(), QStringLiteral("test"));
}

void DirectoryListing_SetPath::nestedDirectoryPath()
{
    DirectoryListing listing;
    listing.setPath(QStringLiteral("test:dir1/dir2/"));
    QCOMPARE(listing.Path(), QStringLiteral("test:dir1/dir2"));
    QCOMPARE(listing.Parent(), QStringLiteral("test:dir1/"));
    QCOMPARE(listing.Name(), QStringLiteral("dir2"));
}

void DirectoryListing_SetPath::setNameReformsPath()
{
    DirectoryListing listing;
    listing.setPath(QStringLiteral("test:dir1/dir2/"));
    listing.setName(QStringLiteral("newName"));
    QCOMPARE(listing.Path(), QStringLiteral("test:dir1/newName"));
    QCOMPARE(listing.Parent(), QStringLiteral("test:dir1/"));
    QCOMPARE(listing.Name(), QStringLiteral("newName"));
}

void DirectoryListing_SetPath::setParentReformsPath()
{
    DirectoryListing listing;
    listing.setPath(QStringLiteral("test:dir1/dir2/"));
    listing.setParent(QStringLiteral("new:other"));
    QCOMPARE(listing.Path(), QStringLiteral("new:other/dir2"));
    QCOMPARE(listing.Parent(), QStringLiteral("new:other/"));
    QCOMPARE(listing.Name(), QStringLiteral("dir2"));
}

QTEST_APPLESS_MAIN(DirectoryListing_SetPath)

#include "tst_directorylisting_setpath.moc"
