#sbs-git:slp/pkgs/xorg/server/xorg-server xorg-server 1.9.3 c6fa517cfcb450647614f9214de654870513e6ff
Name:	xorg-server
Summary:    X.Org X11 X server
Version: 1.9.3
Release:    19
#ExclusiveArch:  %arm
Group:      System/X11
License:    MIT
URL:        http://www.x.org
Source0:    %{name}-%{version}.tar.gz
Source100:  10-kbd.conf
Source101:  10-mouse.conf
Source102:  64-xorg-xkb.rules
Source200:  videoabiver
Source201:  inputabiver
Source202:  serverminver
Source203:  xserver-xorg-core.bug.script
Source1001: packaging/xorg-server.manifest 
Requires:   libdrm2 >= 2.4.0
BuildRequires:  pkgconfig(xorg-macros)
BuildRequires:  pkgconfig(fontutil)
BuildRequires:  pkgconfig(xtrans)
BuildRequires:  pkgconfig(bigreqsproto)
BuildRequires:  pkgconfig(compositeproto)
BuildRequires:  pkgconfig(xproto)
BuildRequires:  pkgconfig(damageproto)
BuildRequires:  pkgconfig(fixesproto)
BuildRequires:  pkgconfig(kbproto)
BuildRequires:  pkgconfig(xineramaproto)
BuildRequires:  pkgconfig(randrproto)
BuildRequires:  pkgconfig(recordproto)
BuildRequires:  pkgconfig(renderproto)
BuildRequires:  pkgconfig(resourceproto)
BuildRequires:  pkgconfig(scrnsaverproto)
BuildRequires:  pkgconfig(videoproto)
BuildRequires:  pkgconfig(xcmiscproto)
BuildRequires:  pkgconfig(xextproto)
BuildRequires:  pkgconfig(dri2proto)
BuildRequires:  pkgconfig(inputproto)
BuildRequires:  pkgconfig(fontsproto)
BuildRequires:  pkgconfig(videoproto)
BuildRequires:  pkgconfig(xf86vidmodeproto)
BuildRequires:  pkgconfig(gestureproto)
BuildRequires:  pkgconfig(xdmcp)
BuildRequires:  pkgconfig(xfont)
BuildRequires:  pkgconfig(xkbfile)
BuildRequires:  pkgconfig(pixman-1)
BuildRequires:  pkgconfig(xv)
BuildRequires:  pkgconfig(libudev)
BuildRequires:  pkgconfig(libdrm)
BuildRequires:  pkgconfig(pciaccess)
BuildRequires:  libgcrypt-devel


%description
Description: %{summary}


%package -n xserver-xorg-devel
Summary:    SDK for X server driver module development
Group:      System/X11
Requires:   %{name} = %{version}-%{release}
Requires:   pixman-devel
Requires:   libpciaccess-devel

%description -n xserver-xorg-devel
The SDK package provides the developmental files which are necessary for
developing X server driver modules, and for compiling driver modules
outside of the standard X11 source code tree.  Developers writing video
drivers, input drivers, or other X modules should install this package.


%package -n xserver-common
Summary:    Xorg server common files
Group:      System/X11
Requires:   %{name} = %{version}-%{release}

%description -n xserver-common
Common files shared among all X servers.

%package -n xserver-xorg-core
Summary:    Xorg X server
Group:      System/X11
Requires:   xserver-common = %{version}-%{release}

%description -n xserver-xorg-core
X.org X11 is an open source implementation of the X Window System.  It
provides the basic low level functionality which full fledged
graphical user interfaces (GUIs) such as GNOME and KDE are designed
upon.

%package -n xserver-xorg-tools
Summary:    Xorg X server
Group:      System/X11
Requires:   xserver-common = %{version}-%{release}

%description -n xserver-xorg-tools
xserver-xorg-tools

%prep
%setup -q



%build
cp %{SOURCE1001} .

%reconfigure \
	--disable-strict-compilation \
	--disable-static \
	--disable-debug \
	--disable-unit-tests \
	--disable-sparkle \
	--disable-builddocs \
	--disable-install-libxf86config \
	--disable-aiglx \
	--disable-glx-tls \
	--enable-registry \
	--enable-gesture \
	--enable-composite \
	--enable-shm \
	--enable-xres \
	--enable-record \
	--enable-xv \
	--enable-xvmc \
	--disable-dga \
	--disable-screensaver \
	--enable-xdmcp \
	--enable-xdm-auth-1 \
	--disable-glx \
	--disable-dri --enable-dri2 \
	--enable-xinerama \
	--enable-xf86vidmode \
	--enable-xace \
	--disable-xselinux \
	--disable-xcsecurity \
	--disable-xcalibrate \
	--disable-tslib \
	--disable-dbe \
	--disable-xf86bigfont \
	--enable-dpms \
	--disable-config-dbus \
	--enable-config-udev \
	--disable-config-hal \
	--enable-xfree86-utils \
	--disable-xaa \
	--disable-vgahw \
	--disable-vbe \
	--with-int10=x86emu \
	--disable-windowswm \
	--enable-libdrm \
	--enable-xorg \
	--disable-dmx \
	--disable-xvfb \
	--disable-xnest \
	--disable-xquartz \
	--disable-xwin \
	--disable-kdrive \
	--disable-xephyr \
	--disable-xfake \
	--disable-xfbdev \
	--disable-kdrive-kbd \
	--disable-kdrive-mouse \
	--disable-kdrive-evdev \
	--disable-doc \
	--disable-devel-doc \
	--without-dtrace \
	--with-extra-module-dir="/usr/lib/xorg/extra-modules" \
	--with-os-vendor="SLP(Samsung Linux Platform)" \
	--with-xkb-path=/opt/etc/X11/xkb \
	--with-xkb-output=/opt/etc/X11/xkb \
	--with-default-font-path="built-ins" \
	--disable-install-setuid \
	--with-sha1=libgcrypt \
	--enable-gestures \
	CFLAGS="\
		-Wall -g \
		-D_F_UDEV_DEBUG_ \
		-D_F_NO_FLOATINGDEVICE_ERROR_ \
		-D_F_NO_CATCH_SIGNAL_ \
		-D_F_BG_NONE_ROOT_ \
		-D_F_ENABLE_XI2_SENDEVENT_ \
		-D_F_CHECK_NULL_CLIENT_ \
		-D_F_DYNAMIC_MIEQ_ \
		-D_F_NO_GRABTIME_UPDATE_ \
		-D_F_COMP_OVL_PATCH \
		-D_F_GESTURE_EXTENSION_ \
 		" \
	CPPFLAGS=""

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

rm -f %{buildroot}/usr/lib/xorg/modules/multimedia/*
rm -f %{buildroot}/usr/lib/xorg/modules/libvbe.so
rm -f %{buildroot}/usr/lib/xorg/modules/libvgahw.so
rm -f %{buildroot}/usr/lib/xorg/modules/libwfb.so
rm -f %{buildroot}/usr/lib/xorg/modules/libxaa.so
rm -f %{buildroot}/usr/lib/xorg/modules/libwfb.so
rm -f %{buildroot}/usr/lib/xorg/modules/libxf8_16bpp.so
rm -f %{buildroot}/opt/etc/X11/xkb/README.compiled

#mkdir -p %{buildroot}/usr/share/X11/xorg.conf.d
#cp -a %{SOURCE100} %{buildroot}/usr/share/X11/xorg.conf.d
#cp -a %{SOURCE101} %{buildroot}/usr/share/X11/xorg.conf.d

#mkdir -p %{buildroot}/usr/share/xserver-xorg
#cp -a %{SOURCE200} %{SOURCE201} %{SOURCE202} %{buildroot}/usr/share/xserver-xorg

#mkdir -p %{buildroot}/usr/share/bug/xserver-xorg-core
#cp -a %{SOURCE203} %{buildroot}/usr/share/bug/xserver-xorg-core/script

#mkdir -p %{buildroot}/lib/udev/rules.d
#cp -a %{SOURCE102} %{buildroot}/lib/udev/rules.d



%remove_docs

%files
%manifest xorg-server.manifest
%defattr(-,root,root,-)
# this section/file is intentionally left blank


%files -n xserver-xorg-devel
%manifest xorg-server.manifest
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/xorg-server.pc
%dir %{_includedir}/xorg
%{_includedir}/xorg/*.h
%{_datadir}/aclocal/xorg-server.m4
%{_datadir}/X11/xorg.conf.d/10-evdev.conf

%files -n xserver-common
%manifest xorg-server.manifest
%defattr(-,root,root,-)
%{_libdir}/xorg/protocol.txt

%files -n xserver-xorg-tools
%manifest xorg-server.manifest
%defattr(-,root,root,-)

%files -n xserver-xorg-core
%manifest xorg-server.manifest
%defattr(-,root,root,-)
%{_bindir}/X
%{_bindir}/Xorg
%{_bindir}/gtf
%{_bindir}/cvt
%dir %{_libdir}/xorg
%dir %{_libdir}/xorg/modules
%dir %{_libdir}/xorg/modules/extensions
%{_libdir}/xorg/modules/extensions/libdri2.so
%{_libdir}/xorg/modules/extensions/libextmod.so
%{_libdir}/xorg/modules/extensions/librecord.so
%dir %{_libdir}/xorg/modules/multimedia
%{_libdir}/xorg/modules/*.so

