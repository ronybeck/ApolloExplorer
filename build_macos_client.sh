#!/bin/bash

printf '\33c\e[3J'
echo ""
echo -e "\033[1m\033[37m########## \033[31mApollo\033[1;30mExplorer MacOS Client - Release 1.4 \033[37m###########\033[0m\033[36m"
echo ""

echo -e "\033[1m\033[37m0. Checking Prerequisites\033[0;30m"

sw_vers | grep "macOS" >log.txt 2>>log.txt
if [ $? -ne 0 ]; then
    echo -e "\033[1m\033[31mThis script can only be used on macOS\033[0;30m"
    exit 1
fi

qmake >>log.txt 2>>log.txt
if [ $? -ne 0 ]; then
    echo -e "\033[1m\033[31mQt qmake command not found\033[0;30m"
    printf 'Do you want to install Qt 6 from homebrew? (y/n):'
    read answer
    if [ "$answer" != "${answer#[Yy]}" ] ;then 
        echo -e "Installing Qt 6 from homebrew (be patient)\033[0;30m"
        brew install -qy qt@6 >>log.txt 2>>log.txt
    else 
        echo -e "Please install Qt 6 (homebrew, Qt online installer or build from source)"
        echo -e "Make sure qmake is in your \$PATH variable\033[0m"
        exit 1
    fi
fi

echo -e "\033[1m\033[37m1. Clean House\033[0m"
rm -f log*.txt
rm -r -f .qmake.stash >>log.txt 2>>log.txt
make clean distclean >>log.txt 2>>log.txt
rm -r -f ApolloExplorer-macOS >>log.txt 2>>log.txt
rm -r -f ApolloExplorer-macOS.dmg >>log.txt 2>>log.txt
cd ApolloExplorerPC
rm -r -f ApolloExplorer.zip >>log.txt 2>>log.txt
cd ..

echo -e "\033[1m\033[37m2. Create macOS Project\033[0m\033[30m"

echo ""
printf 'Do you want to create macOS Universal bundle (Intel + Silicon)?:\n\033[1mIMPORTANT:\033[22m This requires a Qt 6 build with both x86_64 and arm64 support (y/n) '
read answer
if [ "$answer" != "${answer#[Yy]}" ] ;then
    qmake -recursive QMAKE_APPLE_DEVICE_ARCHS="x86_64 arm64" MACOSX_DEPLOYMENT_TARGET="12.7.6" >>log.txt 2>>log.txt
else 
    qmake -recursive MACOSX_DEPLOYMENT_TARGET="12.7.6" >>log.txt 2>>log.txt
fi

echo ""
echo -e "\033[1m\033[37m3. Make macOS Project\033[0m"
make -j16 >>log.txt 2>>log.txt

grep -i "error" log.txt
if [ $? -eq 0 ]; then
    echo -e "\033[1m\033[Error(s) found, check log.txt\033[0m"
    exit 1
fi

echo -e "\033[1m\033[37m4. Add Icons macOS Project\033[0m"
mkdir -p ApolloExplorerPC/ApolloExplorer.app/Contents/Resources/ >>log.txt 2>>log.txt
cp ApolloExplorerPC/icons/ApolloExplorer.icns ApolloExplorerPC/ApolloExplorer.app/Contents/Resources/ >>log.txt 2>>log.txt
codesign --force --timestamp --options=runtime --sign ${APPLETEAMID} ApolloExplorerPC/ApolloExplorer.app/Contents/Resources/ApolloExplorer.icns >>log.txt 2>>log.txt

echo -e "\033[1m\033[37m5. QT Deploy MacOS with static Libs + Hardening + Signing\033[0m" 
macdeployqt ApolloExplorerPC/ApolloExplorer.app -verbose=2 -sign-for-notarization=${APPLETEAMID} >>log.txt 2>>log.txt

echo ""
printf '\033[0;30mProceeed with Signing, Notarization and Packaging?\nRequires Apple Developer Team-ID in ${APPLETEAMID}\nChoose No unless you understand the question (y/n)? '
read answer
if [ "$answer" != "${answer#[Yy]}" ] ;then 
    cd ApolloExplorerPC
    echo ""
    echo -e "\033[1m\033[37mA. ZIP ApolloExplorer.app Bundle (keep Symlinks intact)\033[0m"
    zip -r -y ApolloExplorer.zip ApolloExplorer.app >>log.txt 2>>log.txt
    echo -e "\033[1m\033[37mB. Notarize ApolloExplorer.app Bundle with Apple Developer ID (${APPLETEAMID})\033[0;30m\n"
    xcrun notarytool submit ApolloExplorer.zip --keychain-profile "AC_PASSWORD" --wait   
    echo ""
    echo -e "\033[1m\033[37mC. Staple ApolloExplorer.app Bundle with Notarization Ticket\033[0;30m\n"
    xcrun stapler staple ApolloExplorer.app  
    echo -e "\n\033[1m\033[37mD. Cleanup & Finish\033[0m\n"
    rm -r -f ApolloExplorer.zip
    cd ..
else 
    echo ""
fi

echo -e "\033[1m\033[37m6. Install macOS Project\033[0m"
mkdir -p ApolloExplorer-macOS >>log.txt 2>>log.txt
mv ApolloExplorerPC/ApolloExplorer.app ApolloExplorer-macOS/ >>log.txt 2>>log.txt
mv acp/acp ApolloExplorer-macOS/ >>log.txt 2>>log.txt

echo -e "\033[1m\033[37m7. Clean macOS Project\033[0m"
rm -r -f .qmake.stash >>log.txt 2>>log.txt
make distclean >>log.txt 2>>log.txt
echo -e "\033[0m"

printf '\033[0;30mProceeed with creating Apple DMG for distribution? Choose No unless you understand the question (y/n)? '
read answer
if [ "$answer" != "${answer#[Yy]}" ] ;then 
    echo -e "\n\033[1m\033[37m6. Create Apple DMG for distribution (Please Wait ...)\033[0m"
    brew install create-dmg >>log.txt 2>>log.txt
    create-dmg >>log.txt 2>>log.txt \
    --volicon "ApolloExplorerPC/icons/ApolloExplorer.icns" \
    --icon "ApolloExplorer.app" 200 140 \
    --icon "acp" 200 300 \
    --hide-extension "ApolloExplorer.app" \
    --app-drop-link 600 220 \
    --background "images/ApolloExplorer-macOS.png" \
    --window-size 800 476 \
    --icon-size 128 \
    --overwrite \
    "ApolloExplorer-macOS.dmg" \
    "ApolloExplorer-macOS"
fi

echo ""
