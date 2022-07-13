%bcond_without srpm

# Do out-of-source-tree builds
# Only required in Fedora < f33 as this is the new default
%if 0%{?fedora} < 33
%undefine __cmake_in_source_build
%endif

# pq require C++17 - only required for Fedora <= 33
%if 0%{?fedora} <= 33
%global optflags %{optflags} -std=gnu++17
%endif

Name:       gms-utils
Version:    0.5.7
Release:    1%{?dist}
Summary:    Collection of command line utilities
URL:        https://github.com/gsauthof/utility
License:    GPLv3+
Source:     https://example.org/gms-utils.tar.gz

BuildRequires:   cmake
BuildRequires:   gcc-c++

# Required by the check target
BuildRequires:   python3-pytest
BuildRequires:   python3-distro
BuildRequires:   python3-psutil
# i.e. because of pgrep
BuildRequires:   procps-ng
# i.e. because of gcore
BuildRequires:   gdb
BuildRequires:   python3-dnf
# i.e. for test suite
BuildRequires:   lz4

%if %{__isa_bits} == 64
BuildRequires: glibc-devel(%{__isa_name}-32)
%endif

# for check-cert
Requires: gnutls-utils
Requires: python3-dns
# for matrixto
Requires: python3-matrix-nio
# workaround missing matrix-nio depedency: https://bugzilla.redhat.com/show_bug.cgi?id=1925689
Requires: python3-crypto
# for user_installed
Requires: python3-dnf


%description
Collection of command line utilities.

%prep
%if %{with srpm}
%autosetup -n gms-utils
%endif

%build
%cmake
%cmake_build

%install
%cmake_install
mkdir -p %{buildroot}/usr/local/bin
# resolve conflict with glibc-common
mv %{buildroot}/usr/bin/pldd %{buildroot}/usr/local/bin/
# resolve conflict with obs-build
mv %{buildroot}/usr/bin/unrpm %{buildroot}/usr/local/bin/

%check
# for ctests there is also the ctest macro
%cmake_build --target check


%files
/usr/bin/addrof
/usr/bin/adjtimex
/usr/bin/arsort
/usr/bin/ascii
/usr/bin/check-cert
/usr/bin/check-dnsbl
/usr/bin/chromium-extensions
/usr/bin/cpufreq
/usr/bin/dcat
/usr/bin/detect-size
/usr/bin/devof
/usr/bin/disas
/usr/bin/exec
/usr/bin/firefox-addons
/usr/bin/gs-ext
/usr/bin/inhibit
/usr/bin/isempty
/usr/bin/latest-kernel-running
/usr/bin/lockf
/usr/bin/lsata
/usr/bin/macgen
/usr/bin/matrixto
/usr/bin/oldprocs
/usr/bin/pargs
/usr/bin/pdfmerge
/usr/local/bin/pldd
/usr/bin/pq
/usr/bin/pwhatch
/usr/bin/remove
/usr/bin/reset-tmux
/usr/bin/ripdvd
/usr/bin/searchb
/usr/bin/silence
/usr/bin/swap
/usr/local/bin/unrpm
/usr/bin/user-installed
/usr/bin/wipedev
%doc README.md


%changelog
* Sun Jan 31 2021 Georg Sauthoff <mail@gms.tf> - 0.5.4-1
- add matrixto, inhibit
* Fri Jan  1 2021 Georg Sauthoff <mail@gms.tf> - 0.5.3-1
- add wipedev
* Sun Dec 13 2020 Georg Sauthoff <mail@gms.tf> - 0.5.2-1
- add pq
* Sat Sep 19 2020 Georg Sauthoff <mail@gms.tf> - 0.5.1-1
- bump version
* Sun Sep 06 2020 Georg Sauthoff <mail@gms.tf> - 0.5.0-3
- fix check-cert depedency
* Fri Aug 21 2020 Georg Sauthoff <mail@gms.tf> - 0.5.0-2
- fix check-dnsbl depedency
* Wed Aug 12 2020 Georg Sauthoff <mail@gms.tf> - 0.5.0-1
- initial packaging
