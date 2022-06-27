#!/bin/bash

VERSION=$1
VERSION=${VERSION:-'1.0.8'}
BUILDDIR="apolloexplorer-${VERSION}"
TARFILE="apolloexplorer_${VERSION}.orig.tar.gz"

#Preparation
echo "Preparing ${BUILDDIR}"
cp -ra ApolloExplorerPC ${BUILDDIR}
tar czf ${TARFILE} ${BUILDDIR}

#build
echo "Building ${BUILDDIR}"
cd ${BUILDDIR}
debuild -us -uc
cd ..

#clean up
echo "Cleaning up"
rm -rf ${BUILDDIR}
rm -f ${TARFILE}
