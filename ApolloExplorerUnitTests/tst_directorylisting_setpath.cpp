#include <QtTest>

// add necessary includes here
#include "directorylisting.h"

class DirectoryListing_SetPath : public QObject
{
    Q_OBJECT

public:
    DirectoryListing_SetPath();
    ~DirectoryListing_SetPath();

private slots:
    void initTestCase();
    void cleanupTestCase();
    void test_case1();
    void test_case2();
    void test_case3();
    void test_case4();
    void test_case5();

};

DirectoryListing_SetPath::DirectoryListing_SetPath()
{

}

DirectoryListing_SetPath::~DirectoryListing_SetPath()
{

}

void DirectoryListing_SetPath::initTestCase()
{

}

void DirectoryListing_SetPath::cleanupTestCase()
{

}

void DirectoryListing_SetPath::test_case1()
{
    DirectoryListing listing;
    listing.setPath( "test:dir" );
    QVERIFY( listing.Path() == "test:dir" );
    QVERIFY( listing.Parent() == "test:" );
    QVERIFY( listing.Name() == "dir" );
}

void DirectoryListing_SetPath::test_case2()
{
    DirectoryListing listing;
    listing.setPath( "test" );
    QVERIFY( listing.Path() == "test:" );
    QVERIFY( listing.Parent() == "" );
    QVERIFY( listing.Name() == "test" );
}

void DirectoryListing_SetPath::test_case3()
{
    DirectoryListing listing;
    listing.setPath( "test:dir1/dir2/" );
    QVERIFY( listing.Path() == "test:dir1/dir2" );
    QVERIFY( listing.Parent() == "test:dir1/" );
    QVERIFY( listing.Name() == "dir2" );
}

void DirectoryListing_SetPath::test_case4()
{
    DirectoryListing listing;
    listing.setPath( "test:dir1/dir2/" );
    listing.setName( "newName" );
    QVERIFY( listing.Path() == "test:dir1/newName" );
    QVERIFY( listing.Parent() == "test:dir1/" );
    QVERIFY( listing.Name() == "newName" );
}

void DirectoryListing_SetPath::test_case5()
{
    DirectoryListing listing;
    listing.setPath("test:dir1/dir2/" );
    listing.setParent( "new:other" );
    QVERIFY( listing.Path() == "new:other/dir2" );
    QVERIFY( listing.Parent() == "new:other/" );
    QVERIFY( listing.Name() == "dir2" );
}
QTEST_APPLESS_MAIN(DirectoryListing_SetPath)

#include "tst_directorylisting_setpath.moc"
