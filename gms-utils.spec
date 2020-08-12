%bcond_without srpm

# Do out-of-source-tree builds
# Only required in Fedora < f33 as this is the new default
%undefine __cmake_in_source_build

Name:       gms-utils
Version:    0.5.0
Release:    1%{?dist}
Summary:    Collection of command line utilities
URL:        https://github.com/gsauthof/utility
License:    GPLv3+
Source:     https://example.org/gms-utils.tar.gz

BuildRequires:   cmake
BuildRequires:   gcc-c++

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

%check
%ctest

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
/usr/bin/devof
/usr/bin/disas
/usr/bin/exec
/usr/bin/firefox-addons
/usr/bin/gs-ext
/usr/bin/isempty
/usr/bin/latest-kernel-running
/usr/bin/lockf
/usr/bin/lsata
/usr/bin/macgen
/usr/bin/oldprocs
/usr/bin/pargs
/usr/bin/pdfmerge
/usr/bin/pldd
/usr/bin/pwhatch
/usr/bin/remove
/usr/bin/reset-tmux
/usr/bin/ripdvd
/usr/bin/searchb
/usr/bin/silence
/usr/bin/swap
/usr/bin/unrpm
/usr/bin/user-installed
%doc README.md


%changelog
* Wed Aug 12 2020 Georg Sauthoff <mail@gms.tf> - 0.5.0-1
- initial packaging
