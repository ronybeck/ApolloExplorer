$Bold       = "$([char]27)[1m" 
$BoldReset  = "$([char]27)[22m"

$Red        = "$([char]27)[31m"
$Green      = "$([char]27)[32m"
$Yellow     = "$([char]27)[33m"
$Blue       = "$([char]27)[34m"
$Purple     = "$([char]27)[35m"
$LightBlue  = "$([char]27)[36m"
$White      = "$([char]27)[37m"
$Grey       = "$([char]27)[30m"

Clear-Host
Write-Host ""
Write-Host "${Bold}${White}########## ${Red}Apollo${Grey}Explorer Windows Client - Release 1.4 ${White}###########"
Write-Host ""
Write-Host "${Bold}${White}0. Checking Prerequisites${Grey}${BoldReset}"

# TODO: Check if we run on Windows
$UserPath = Resolve-Path ~
$QT6InstallPath = Join-Path $UserPath "\Qt6"
$QT6CommandPath =  Join-Path $QT6InstallPath "\6.11.1\mingw_64\bin"  
$env:Path += ";"
$env:Path += $QT6CommandPath
$QT6ToolsPath = Join-Path $QT6InstallPath "\Tools\mingw1310_64\bin"  
$env:Path += ";"
$env:Path += $QT6ToolsPath

try {
    Get-Command "qmake" -ErrorAction Stop | Out-Null
    Write-Host "* QT6 FrameWork found"
} catch {
    $Answer = Read-Host "* No QT6 FrameWork found, do you want to install QT6? (y/n) "
    $Answer = $Answer.Trim().ToLower()
    if($Answer -eq "n") {exit}
    Write-Host ""
    Write-Host ""
    Write-Host "* Installing QT6 FrameWork (qt6.11.1-essentials-dev) to $QT6InstallPath"
    Invoke-WebRequest https://download.qt.io/official_releases/online_installers/qt-online-installer-windows-x64-online.exe -O qt-online-installer-windows-x64-online.exe | Out-Null
    .\qt-online-installer-windows-x64-online.exe --root $QT6InstallPath --accept-licenses --default-answer --confirm-command install qt6.11.1-essentials-dev | Out-Null
    try {
        Get-Command "qmake" -ErrorAction Stop | Out-Null
        Write-Host "* QT6 FrameWork succesfully installed"
        Remove-Item "qt-online-installer-windows-x64-online.exe"
    } catch {
        Write-Host "* ERROR Installing QT6"
    }
}

Write-Host "${Bold}${White}1. Clean House${Grey}${BoldReset}"
mingw32-make.exe distclean 1>log_succes.txt 2>log_errors.txt 3>log_warnings.txt 
Remove-Item -Recurse -Force .\acp\debug\ 1>>log_succes.txt 2>>log_errors.txt 3>>log_warnings.txt
Remove-Item -Recurse -Force .\acp\release\ 1>>log_succes.txt 2>>log_errors.txt 3>>log_warnings.txt
Remove-Item -Recurse -Force .\ApolloExplorerPC\debug\ 1>>log_succes.txt 2>>log_errors.txt 3>>log_warnings.txt
Remove-Item -Recurse -Force .\ApolloExplorerPC\release\ 1>>log_succes.txt 2>>log_errors.txt 3>>log_warnings.txt
Remove-Item -Recurse -Force .\AmigaIconReader\debug\ 1>>log_succes.txt 2>>log_errors.txt 3>>log_warnings.txt
Remove-Item -Recurse -Force .\AmigaIconReader\release\ 1>>log_succes.txt 2>>log_errors.txt 3>>log_warnings.txt
Remove-Item -Recurse -Force .\ApolloExplorer-Windows 1>>log_succes.txt 2>>log_errors.txt 3>>log_warnings.txt
Remove-Item .\ApolloExplorerPC\ApolloExplorer_resource.rc  1>>log_succes.txt 2>>log_errors.txt 3>>log_warnings.txt

Write-Host "${Bold}${White}2. Create Windows Project${Grey}${BoldReset}"
qmake -recursive -config release  1>>log_succes.txt 2>>log_errors.txt 3>>log_warnings.txt

Write-Host "${Bold}${White}3. Make Windows Project${Grey}${BoldReset}"
mingw32-make.exe -j8 1>>log_succes.txt 2>>log_errors.txt 3>>log_warnings.txt

$ErrorCheck = Select-String -Path log_errors.txt -Pattern "\berror\b"

if ($null -ne $ErrorCheck)
{
    Write-Host "${Bold}${Red}Error(s) found, check log_errors.txt${Grey}${BoldReset}"
} else {
    Write-Host "${Bold}${White}4. Install Windows Project${Grey}${BoldReset}"
    mkdir ApolloExplorer-Windows >>log_succes.txt 2>>log_errors.txt 3>>log_warnings.txt
    Move-Item .\acp\release\acp.exe .\ApolloExplorer-Windows\acp.exe -Force 1>>log_succes.txt 2>>log_errors.txt 3>>log_warnings.txt
    Move-Item .\ApolloExplorerPC\release\ApolloExplorer.exe .\ApolloExplorer-Windows\ApolloExplorer.exe -Force 1>>log_succes.txt 2>>log_errors.txt 3>>log_warnings.txt
    windeployqt.exe --release .\ApolloExplorer-Windows\ApolloExplorer.exe 1>>log_succes.txt 2>>log_errors.txt 3>>log_warnings.txt
}

Write-Host "${Bold}${White}5. Clean Windows Project${Grey}${BoldReset}"
mingw32-make.exe distclean 1>>log_succes.txt 2>>log_errors.txt 3>>log_warnings.txt
Remove-Item -Recurse -Force .\acp\debug\ 1>>log_succes.txt 2>>log_errors.txt 3>>log_warnings.txt
Remove-Item -Recurse -Force .\acp\release\ 1>>log_succes.txt 2>>log_errors.txt 3>>log_warnings.txt
Remove-Item -Recurse -Force .\ApolloExplorerPC\debug\ 1>>log_succes.txt 2>>log_errors.txt 3>>log_warnings.txt
Remove-Item -Recurse -Force .\ApolloExplorerPC\release\ 1>>log_succes.txt 2>>log_errors.txt 3>>log_warnings.txt
Remove-Item -Recurse -Force .\AmigaIconReader\debug\ 1>>log_succes.txt 2>>log_errors.txt 3>>log_warnings.txt
Remove-Item -Recurse -Force .\AmigaIconReader\release\ 1>>log_succes.txt 2>>log_errors.txt 3>>log_warnings.txt
Remove-Item .\ApolloExplorerPC\ApolloExplorer_resource.rc  1>>log_succes.txt 2>>log_errors.txt 3>>log_warnings.txt  


