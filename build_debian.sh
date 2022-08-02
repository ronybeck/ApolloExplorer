#!/bin/bash

VERSION=$1
VERSION=${VERSION:-'1.0.9'}
BUILDDIR="apolloexplorer-${VERSION}"
TARFILE="apolloexplorer_${VERSION}.orig.tar.gz"

#cleanup any leftovers from last time
echo "Cleaning the slate"
rm -rf ${BUILDDIR} ${TARFILE}

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
