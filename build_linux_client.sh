#!/bin/bash

printf '\33c\e[3J'
echo ""
echo -e "\033[1m\033[37m########## \033[31mApollo\033[1;30mExplorer Linux Client - Release 1.4 \033[37m###########\033[0m\033[36m"
echo ""

echo -e "\033[1m\033[37m0. Checking Prerequisites\033[0m"
cat /etc/os-release | grep -i "debian" >log.txt 2>>log.txt
if [ $? -ne 0 ]; then
    echo -e "\033[1m\033[31mThis script is only tested on Debian-based Linux distributions (Debian, Ubuntu, etc)\033[0;30m"
    printf 'Do you want to continue anyway? (y/n):'
    read answer
    if [ "$answer" != "${answer#[Nn]}" ] ;then 
        exit 1
    fi
fi

sudo apt install -qy build-essential >>log.txt 2>>log.txt
sudo apt remove -qy qt5-base-dev qt5-tools-dev qt5-tools >>log.txt 2>>log.txt
sudo apt install -qy qt6-*dev* >>log.txt 2>>log.txt

echo -e "\033[1m\033[37m1. Clean House\033[0m"
rm -r -f .qmake.stash >log.txt 2>>log.txt

make distclean >>log.txt 2>>log.txt
rm -r -f ApolloExplorerPC/ApolloExplorer.zip >>log.txt 2>>log.txt
rm -r -f ApolloExplorerPC/ApolloExplorer.dmg >>log.txt 2>>log.txt
rm -r -f ApolloExplorer-Linux >>log.txt 2>>log.txt

echo -e "\033[1m\033[37m2. QT6 Create Linux Project\033[0m"
qmake6 -recursive >>log.txt 2>>log.txt

echo -e "\033[1m\033[37m3. Make Linux Project\033[0m"
make -j16 >>log.txt 2>>log.txt

grep -i "error" log.txt
if [ $? -eq 0 ]; then
    echo -e "\033[1m\033[31mError(s) found, check log.txt\033[0m"
    exit 1
fi

echo -e "\033[1m\033[37m4. Install Linux Project\033[0m"

mkdir -p ApolloExplorer-Linux
mv ApolloExplorerPC/ApolloExplorer ApolloExplorer-Linux/ApolloExplorer
cp ApolloExplorerPC/ApolloExplorer.desktop ApolloExplorer-Linux/ApolloExplorer.desktop
mv acp/acp ApolloExplorer-Linux/acp

echo -e "\033[1m\033[37m5. Clean Project\033[0m"
make distclean >>log.txt 2>>log.txt
echo -e "\033[0m"