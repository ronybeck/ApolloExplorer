Name:           apolloexplorer
Version:        %{app_version}
Release:        1%{?dist}
Summary:        File explorer client for the Apollo Amiga accelerator

License:        MIT
URL:            https://github.com/ronybeck/ApolloExplorer
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qttools-devel
BuildRequires:  gcc-c++
BuildRequires:  make
Requires:       qt6-qtbase

%description
ApolloExplorer is a file sharing utility for getting files to and from
your Amiga from the comfort of your PC. A server component runs in the
background on the Amiga (Apollo V4) and allows this client to connect,
browse local disks and transfer files over the network.

This package installs the ApolloExplorer GUI client, the acp command
line copy tool and the ash remote shell tool.

%prep
%autosetup -n %{name}-%{version}

%build
qmake6 -recursive
%make_build

%install
rm -rf %{buildroot}
install -Dm0755 ApolloExplorerPC/ApolloExplorer %{buildroot}%{_bindir}/ApolloExplorer
install -Dm0755 acp/acp %{buildroot}%{_bindir}/acp
install -Dm0755 ash/ash %{buildroot}%{_bindir}/ash
install -Dm0644 packaging/linux/apolloexplorer.desktop %{buildroot}%{_datadir}/applications/apolloexplorer.desktop
install -Dm0644 ApolloExplorerPC/icons/Apollo_Explorer_icon.png %{buildroot}%{_datadir}/pixmaps/apolloexplorer.png

%files
%license LICENSE
%{_bindir}/ApolloExplorer
%{_bindir}/acp
%{_bindir}/ash
%{_datadir}/applications/apolloexplorer.desktop
%{_datadir}/pixmaps/apolloexplorer.png

%changelog
* Mon Jan 01 2024 Rony Beck <ronybeck@themenz.biz> - 1.4.0-1
- Initial packaging for Fedora
