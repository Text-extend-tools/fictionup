Name:           fictionup
Version:        0.2.1
Release:        1%{?dist}
Summary:        markdown to FB2 converter

License:        GPL-3.0+
URL:            http://cdslow.org.ru/en/fictionup
Source0:        %{name}-%{version}.tar.gz

%if 0%{?suse_version}
    %global app_group Productivity/Publishing/Other
%else
    %global app_group Applications/Publishing
%endif

BuildRequires:  libyaml-devel
BuildRequires:  zlib-devel
BuildRequires:  cmake

Group:          %{app_group}

BuildRoot:      %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

%description
fictionup is a command line tool for converting markdown formatted document to FB2 XML file.

%prep
%setup -q

%build
%if 0%{?suse_version} >= 1310
    %cmake
%else
    %if 0%{?cmake:1}
        %cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo .
    %else
        CFLAGS="${CFLAGS:-%optflags}"
        export CFLAGS
        cmake \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_VERBOSE_MAKEFILE=ON \
            -DCMAKE_INSTALL_PREFIX:PATH=%{_prefix} \
            -DCMAKE_INSTALL_LIBDIR:PATH=%{_libdir} \
            -DINCLUDE_INSTALL_DIR:PATH=%{_includedir} \
            -DLIB_INSTALL_DIR:PATH=%{_libdir} \
            -DSYSCONF_INSTALL_DIR:PATH=%{_sysconfdir} \
            -DSHARE_INSTALL_PREFIX:PATH=%{_datadir} \
            -DCMAKE_SKIP_RPATH:BOOL=ON \
            -DBUILD_SHARED_LIBS:BOOL=ON \
            .
    %endif
%endif
make %{?_smp_mflags}

%install
%if 0%{?suse_version} >= 1310
    cd build
%endif

%if 0%{?make_install:1}
    %make_install
%else
    make install DESTDIR=%{buildroot}
%endif

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(0755, root, root)
%{_bindir}/%{name}
%defattr(0644, root, root)
%{_datadir}/%{name}
%doc %{_datadir}/man/man1/%{name}.1.gz
%doc %{_datadir}/doc/%{name}

%changelog
* Wed Sep 11 2019 Vadim Druzhin <cdslow@mail.ru> - 0.2.1-1
- Add workaround for Gentoo patched zlib.

* Sun May  5 2019 Vadim Druzhin <cdslow@mail.ru> - 0.2-1
- Add 'Series:' meta-information tag;
- Support markdown formatting in the annotation;
- Add ZIP output.

* Thu Dec 14 2017 Vadim Druzhin <cdslow@mail.ru> - 0.1-1
- First release.
