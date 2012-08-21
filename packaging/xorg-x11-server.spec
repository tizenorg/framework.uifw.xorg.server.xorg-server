Name:	    xorg-x11-server
Summary:    X.Org X11 X server
Version:    1.12.2
Release:    1
Group:      System/X11
License:    MIT
URL:        http://www.x.org
Source0:    %{name}-%{version}.tar.gz
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
BuildRequires:  xorg-x11-proto-gesture
BuildRequires:  pkgconfig(xdmcp)
BuildRequires:  pkgconfig(xfont)
BuildRequires:  pkgconfig(xkbfile)
BuildRequires:  pkgconfig(pixman-1)
BuildRequires:  pkgconfig(xv)
BuildRequires:  pkgconfig(libudev)
BuildRequires:  pkgconfig(libdrm)
BuildRequires:  libpciaccess-devel
BuildRequires:  libgcrypt-devel


%description
Description: %{summary}


%package  common
Summary:  Xorg server common files
Group:    System/X11
Requires: pixman >= 0.21.8
Requires: xkeyboard-config xkbcomp
Provides: xserver-common

%description common
Common files shared among all X servers.


%package Xorg
Summary:    Xorg X server
Group:      System/X11
Requires:   xorg-x11-server-common = %{version}-%{release}
Provides:   xserver-xorg-core

%description Xorg
X.org X11 is an open source implementation of the X Window System.  It
provides the basic low level functionality which full fledged
graphical user interfaces (GUIs) such as GNOME and KDE are designed
upon.


%package devel
Summary:    SDK for X server driver module development
Group:      System/X11
Requires:   %{name}-Xorg = %{version}-%{release}
Requires:   pixman-devel
Requires:   libpciaccess-devel
Provides:   xserver-xorg-dev

%description devel
The SDK package provides the developmental files which are necessary for
developing X server driver modules, and for compiling driver modules
outside of the standard X11 source code tree.  Developers writing video
drivers, input drivers, or other X modules should install this package.


#%package source
#Summary: Xserver source code required to build VNC server (Xvnc)
#Group: Development/Libraries
#BuildArch: noarch
#
#%description source
#Xserver source code needed to build VNC server (Xvnc)


%prep
%setup -q


%build

./autogen.sh
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
	--with-xkb-path=/usr/etc/X11/xkb \
	--with-xkb-output=/usr/etc/X11/xkb \
	--with-default-font-path="built-ins" \
	--disable-install-setuid \
	--with-sha1=libgcrypt \
	--enable-gestures \
	CFLAGS="${CFLAGS} \
		-Wall -g \
		-D_F_UDEV_DEBUG_ \
		-D_F_NO_GRABTIME_UPDATE_ \
		-D_F_NO_CATCH_SIGNAL_ \
		-D_F_CHECK_NULL_CLIENT_ \
		-D_F_COMP_OVL_PATCH \
		-D_F_PUT_ON_PIXMAP_ \
		-D_F_IGNORE_MOVE_SPRITE_FOR_FLOATING_POINTER_ \
                -D_F_NOT_ALWAYS_CREATE_FRONTBUFFER_ \
		-D_F_GESTURE_EXTENSION_ \
 		" \
	CPPFLAGS="${CPPFLAGS} "

#excluded macros
#		-D_F_DYNAMIC_MIEQ_ \
#		-D_F_NO_FLOATINGDEVICE_ERROR_ \
#		-D_F_ENABLE_XI2_SENDEVENT_ \
#		-D_F_BG_NONE_ROOT_ \

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

rm -f %{buildroot}/usr/etc/X11/xkb/README.compiled
rm -f %{buildroot}/usr/share/X11/xorg.conf.d/10-evdev.conf
rm -rf %{buildroot}/usr/share/man/*

#mkdir -p %{buildroot}/usr/share/X11/xorg.conf.d
#cp -a %{SOURCE100} %{buildroot}/usr/share/X11/xorg.conf.d
#cp -a %{SOURCE101} %{buildroot}/usr/share/X11/xorg.conf.d

#mkdir -p %{buildroot}/usr/share/xserver-xorg
#cp -a %{SOURCE200} %{SOURCE201} %{SOURCE202} %{buildroot}/usr/share/xserver-xorg

#mkdir -p %{buildroot}/usr/share/bug/xserver-xorg-core
#cp -a %{SOURCE203} %{buildroot}/usr/share/bug/xserver-xorg-core/script

#mkdir -p %{buildroot}/lib/udev/rules.d
#cp -a %{SOURCE102} %{buildroot}/lib/udev/rules.d

# Make the source package
#%define xserver_source_dir %{_datadir}/xorg-x11-server-source
#%define inst_srcdir %{buildroot}/%{xserver_source_dir}
#mkdir -p %{inst_srcdir}/{Xext,xkb,GL,hw/{xquartz/bundle,xfree86/common}}
#mkdir -p %{inst_srcdir}/{hw/dmx/doc,man,doc,hw/dmx/doxygen}
#cp {,%{inst_srcdir}/}hw/xquartz/bundle/cpprules.in
#cp {,%{inst_srcdir}/}man/Xserver.man
#cp {,%{inst_srcdir}/}doc/smartsched
#cp {,%{inst_srcdir}/}hw/dmx/doxygen/doxygen.conf.in
#cp {,%{inst_srcdir}/}xserver.ent.in
#cp xkb/README.compiled %{inst_srcdir}/xkb
#cp hw/xfree86/xorgconf.cpp %{inst_srcdir}/hw/xfree86


%clean
rm -rf $RPM_BUILD_ROOT

%remove_docs


%files common
%defattr(-,root,root,-)
%{_libdir}/xorg/protocol.txt

%files Xorg
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

%files devel
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/xorg-server.pc
%dir %{_includedir}/xorg
%{_includedir}/xorg/*.h
%{_datadir}/aclocal/xorg-server.m4

#%files source
#%defattr(-, root, root, -)
#%{xserver_source_dir}
