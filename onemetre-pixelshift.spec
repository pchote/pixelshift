Name:      onemetre-pixelshift
Version:   1.0.1
Release:   0
Url:       https://github.com/warwick-one-metre/pixelshift
Summary:   Autoguiding helper for Warwick one-metre telescope.
License:   GPL-3.0
Group:     Unspecified
BuildArch: x86_64
Requires: cfitsio

%description
Part of the observatory software for the Warwick one-meter telescope.

A tool for calculating sub-pixel translational shifts between images and applying guide offsets to the Warwick one-metre telescope.

%build
mkdir -p %{buildroot}%{_bindir}

%{__install} %{_sourcedir}/pixelshift %{buildroot}%{_bindir}

%files
%defattr(0755,root,root,-)
%{_bindir}/pixelshift

%changelog
