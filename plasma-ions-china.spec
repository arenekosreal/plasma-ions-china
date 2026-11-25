%global commit 1d5b55ed72c22822db5c8a482f29022457674a00
%global upstream_version 0.1.3
# Source0 SHA256: 1304844f5397adc95591aaa912fa0ee57da294dc6ee5ac8438d5e09f8152c5d3

Name:           plasma-ions-china
Version:        0.1.3
Release:        1%{?dist}
Summary:        Plasma weather ion plugins for Chinese weather services
License:        GPL-3.0-only
URL:            https://github.com/arenekosreal/plasma-ions-china
Source0:        %{url}/archive/%{commit}.tar.gz#/%{name}-%{version}.tar.gz

%if 0%{?fedora}
BuildRequires:  cmake >= 4.0
BuildRequires:  gcc-c++
BuildRequires:  gettext
BuildRequires:  extra-cmake-modules
BuildRequires:  ninja-build
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  kf6-kcoreaddons-devel
BuildRequires:  kf6-ki18n-devel
BuildRequires:  kf6-kunitconversion-devel
BuildRequires:  kdeplasma-addons-devel
%endif

%if 0%{?suse_version}
BuildRequires:  cmake >= 4.0
BuildRequires:  gcc-c++
BuildRequires:  gettext-tools
BuildRequires:  kf6-extra-cmake-modules
BuildRequires:  ninja
BuildRequires:  qt6-base-devel
BuildRequires:  qt6-declarative-devel
BuildRequires:  kf6-kcoreaddons-devel
BuildRequires:  kf6-ki18n-devel
BuildRequires:  kf6-kunitconversion-devel
BuildRequires:  kdeplasma6-addons-devel
%endif

%if 0%{?suse_version}
Requires:       kdeplasma6-addons
%else
Requires:       kdeplasma-addons
%endif

%description
A collection of Plasma weather ions for Chinese users.

This package contains the nmccn ion, which uses JSON data from the National
Meteorological Center of China. The qweather ion requires per-user QWeather
API credentials at build time and is therefore not included in this package.

%prep
%autosetup -n %{name}-%{commit}

%build
%cmake \
    -DPLASMA_IONS_CHINA_VERSION=%{upstream_version} \
    -DPLASMA_IONS_CHINA_ENABLE_MODERN=ON \
    -DPLASMA_IONS_CHINA_ENABLE_LEGACY=OFF \
    -DPLASMA_IONS_CHINA_USE_SYSTEM_HEADERS=ON \
    -DKDE_INSTALL_USE_QT_SYS_PATHS=ON
%cmake_build

%install
%cmake_install

%files
%license LICENSE.txt
%doc README.md
%{_libdir}/qt6/plugins/plasma/weather_ions/
%{_datadir}/locale/*/LC_MESSAGES/plasma_ions_china.mo

%changelog
* Wed Nov 25 2026 Plasma Ions China Packaging <plasma-ions-china@users.noreply.github.com> - 0.1.3-1
- Initial COPR package.
