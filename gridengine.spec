# This spec flie is intended to build gridengine rpms on RedHat or
# Fedora, but has only been tested under RHEL 5.  It installs into
# /opt/sge, intended to support shared installations like the original
# Sun distribution.  It doesn't deal with the configuration of the
# installed binaries (use the vanilla instructions) or with init
# scripts, because it's difficult with anything other than a
# default cell.  Herd (Hadoop) and the GUI installer aren't currently
# supported due to problems dealing wwith their dependencies.

# This was originally derived from the Fedora version, but probably
# doesn't bear too much resemblance to it now.

# Fixmes:
# * Check on/port to SuSE and other relevant distributions.
# * Support shared installs "--with sharedinstall"
# * Try to support herd (Hadoop) and the GUI installer -- either assume
#   izpack/hadoop installed, or maybe download and build them.  Probably
#   use --with guiinst, --with hadoop.  (NB izpack download needs components
#   removed to be redistributable.)
# * Clarify the licence on this file to the extent it's derived from the
#   Fedora one.
# * Patch or hook for installation scripts to default appropriately for
#   packaged installation.

# Use "rpmbuild --without java" to omit all Java bits
%bcond_without java

%define sge_home /opt/sge
%define sge_lib %{sge_home}/lib
# Binaries actually go in arch-dependent subdir of this
%define sge_bin %{sge_home}/bin
%define sge_mandir %{sge_home}/man
%define sge_libexecdir %{sge_home}/utilbin
%define sge_javadir %sge_lib
%define sge_include %{sge_home}/include
%define sge_docdir %{sge_home}/doc
%define _docdir %{sge_docdir}
# admin user maybe to create
%define username sgeadmin

Name:    gridengine
Version: 8.0.0e
Release: 1%{?dist}
Summary: Grid Engine - Distributed Resource Manager

Group:   Applications/System
# per 3rd_party_licscopyrights
License: (BSD and LGPLv2+ and MIT and SISSL) and GPLv2+ and GFDLv3+ and others
URL:     https://arc.liv.ac.uk/trac/SGE
Source:  https://arc.liv.ac.uk/downloads/SGE/releases/%{version}/sge-%{version}.tar.gz
Prefix: %{sge_home}

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires: /bin/csh, openssl-devel, db4-devel, ncurses-devel, pam-devel
BuildRequires: libXmu-devel, libXpm-devel, hwloc-devel >= 1.1
# This used to test %{?rhel}, but that's not defined on RHEL 5, and
# I don't know whether _host_vendor distinguishes Fedora and RHEL.
%if 0%{?fedora}
BuildRequires: lesstif-devel
%else
BuildRequires: openmotif-devel
%endif
%if %{with java}
BuildRequires: java-devel >= 1.6.0, javacc, ant-junit, ant-nodeps
#BuildRequires: hadoop-0.20
%endif
BuildRequires: elfutils-libelf-devel, net-tools, man, gzip
%if 0%{?fedora}
BuildRequires: fedora-usermgmt-devel
%endif
Requires: binutils, ncurses, shadow-utils
# for makewhatis
Requires: man

%description
Grid Engine (often known as SGE) is a distributed resource manager,
typically deployed to manage batch jobs on comptational clusters (like
Torque/Maui), but also capable of managing interactive jobs and looser
collections of resources, such as desktop PCs (like Condor).

The computational resources may be heterogeneous (including different
operating systems) with specified properties.  Jobs are matched to
available resources according to the properties they request.

These are the files shared by both the qmaster and execd daemons,
required to run either the server or clients.

https://arc.liv.ac.uk/trac/SGE

%package devel
Summary: Gridengine development files
Group: Development/Libraries
License: BSD and LGPLv2+ and MIT and SISSL
Requires: %{name} = %{version}-%{release}

%description devel
Grid Engine development headers and libraries.

%package qmon
Summary: Gridengine qmon monitor
Group: Development/Libraries
License: BSD and LGPLv2+ and MIT and SISSL
Requires: %{name} = %{version}-%{release}
Requires: xorg-x11-fonts-ISO8859-1-100dpi xorg-x11-fonts-ISO8859-1-75dpi

%description qmon
The qmon graphical graphical interface to Grid Engine.

%package execd
Summary: Gridengine execd program
Group: Development/Libraries
License: BSD and LGPLv2+ and MIT and SISSL
Requires: %{name} = %{version}-%{release}
Requires(postun): %{name} = %{version}-%{release}
Requires(preun): %{name} = %{version}-%{release}

%description execd
Programs needed to run a Grid Engine execution host.

%package qmaster
Summary: Gridengine qmaster programs
Group: Development/Libraries
License: BSD and LGPLv2+ and MIT and SISSL
Requires: %{name} = %{version}-%{release}
Requires: db4-utils
# Not automatically derived:
%if %{with java}
Requires: java >= 1.6.0
%endif
Requires(postun): %{name} = %{version}-%{release}
Requires(preun): %{name} = %{version}-%{release}

%description qmaster
Programs needed to run a Grid Engine master host.

%package drmaa4ruby
Summary: Ruby binding for DRMAA library
Group: Development/Libraries
License: SISSL

%description drmaa4ruby
This binding is presuambly not Grid Engine-specific.

%prep

%setup -q -n sge-%{version}

%build
cd source
# cat >> aimk.private <<EOF
# EOF
cat > build_private.properties <<\EOF
javacc.home=%{_javadir}
libs.junit_4.classpath=%{_javadir}/junit.jar
hadoop.dir=/usr/lib/hadoop-0.20
hadoop.version=0.20.2-cdh3u0
file.reference.hadoop-0.20.1-core.jar=${hadoop.dir}/hadoop-core-${hadoop.version}.jar
file.reference.hadoop-0.20.1-tools.jar=${hadoop.dir}/hadoop-tools-${hadoop.version}.jar
EOF
# Ensure dlopening the right libssl library version at runtime
ssl_ver=$(objdump -p %{_libdir}/libssl.so | awk '/SONAME/ {print substr($2,10)}')

# -O2/-O3 gives warnings about type puns.  It's not clear whether
# they're serious, but -fno-strict-aliasing just in case.
export SGE_INPUT_CFLAGS="$RPM_OPT_FLAGS -fno-strict-aliasing"
[ -n "$RPM_BUILD_NCPUS" ] && parallel_flags="-parallel $RPM_BUILD_NCPUS"
%if %{without java}
JAVA_BUILD_OPTIONS="-no-java -no-jni"
%endif
csh -f ./aimk -only-depend $JAVA_BUILD_OPTIONS
scripts/zerodepend
./aimk $JAVA_BUILD_OPTIONS depend
./aimk -DLIBSSL_VER='\"'$ssl_ver'\"' -no-gui-inst -system-libs -pam -no-herd $parallel_flags $JAVA_BUILD_OPTIONS
./aimk -man $JAVA_BUILD_OPTIONS
%if %{with java}
# "-no-gui-inst -no-herd -javadoc" still produces all the javadocs
ant drmaa.javadoc juti.javadoc jgdi.javadoc jjsv.javadoc
%endif

%install 
rm -rf $RPM_BUILD_ROOT
SGE_ROOT=$RPM_BUILD_ROOT%{sge_home}
export SGE_ROOT
mkdir -p $SGE_ROOT
cd source
echo instguiinst=false >distinst.private
gearch=`dist/util/arch`
echo 'y'| scripts/distinst -nobdb -noopenssl -local -allall -noexit ${gearch}
( cd $RPM_BUILD_ROOT/%{sge_home}
  mv 3rd_party/3rd_party_licscopyrights doc
  rm -r hadoop catman 3rd_party dtrace
  rm -r examples/jobsbin
  rm man/man8/SGE_Helper_Service.exe.8
  rm -r util/sgeSMF
  rm util/resources/loadsensors/interix-loadsensor.sh # uses ksh
  for l in lib/*/libdrmaa.so.1.0; do
    ( cd $(dirname $l); ln -sf libdrmaa.so.1.0 libdrmaa.so )
  done
  gzip man/man*/*
)
cat ../README - > $RPM_BUILD_ROOT/%{sge_home}/doc/README <<+

Note that, unlike the Fedora rpm, this version doesn't try to configure
the system or provide its own init scripts, and installs into /opt/sge,
which is appropriate for a cluster shared installation, consistent with
the old Sun binaries.
+

%clean
rm -rf $RPM_BUILD_ROOT

%pre
%{_sbindir}/groupadd -r %username 2>/dev/null || :
%{_sbindir}/useradd -d / -s /sbin/nologin \
   -g %username -M -r -c 'Grid Engine admin' %username 2>/dev/null || :

%post
if [ -f /usr/sbin/mandb ]; then
  # Later Fedora
  mandb %{sge_home}/man
else
  makewhatis %{sge_home}/man
fi

%files
%defattr(-,root,root,-)
# Ensure we can make sgeadmin-owned cell directory
%attr(775,root,%{username}) %{sge_home}
%exclude %{sge_bin}/*/qacct
%exclude %{sge_bin}/*/qmon
%exclude %{sge_bin}/*/sge_*
%if %{with java}
%exclude %{sge_docdir}/javadocs
%endif
%exclude %{sge_home}/examples/drmaa
%exclude %{sge_mandir}/man1/qmon.1.gz
%exclude %{sge_mandir}/man8/sge_qmaster.8.gz
%exclude %{sge_mandir}/man8/sge_execd.8.gz
%exclude %{sge_mandir}/man8/sge_*shepherd.8.gz
%exclude %{sge_mandir}/man8/sge_shadowd.8.gz
%exclude %{sge_mandir}/man8/pam*8.gz
%exclude %{sge_mandir}/man3
%exclude %{sge_home}/utilbin/*/rshd
%exclude %{sge_lib}/*/pam*
%exclude %{sge_home}/pvm/src
%exclude %{sge_bin}/process-scheduler-log
%exclude %{sge_bin}/qsched
%exclude %{sge_bin}/jobstats
%exclude %{sge_home}/util/resources/drmaa4ruby
%{sge_bin}
%{sge_lib}
%{sge_home}/ckpt
%doc %{sge_docdir}
%{sge_home}/inst_sge
%{sge_home}/mpi
%{sge_home}/pvm
%{sge_home}/util
%{sge_home}/utilbin
# rlogin and rsh need to be suid to work, but there is no need for them
# as we have ssh and PAM
%attr{4755,root,root} %{sge_home}/utilbin/*/testsuidroot
#%attr{4755,root,root} %{sge_home}/utilbin/*/authuser
# Avoid this for safety, assuming no MS Windows hosts
#%attr{4755,root,root} %{sge_home}/utilbin/*/sgepasswd
%{sge_mandir}/man1/*.1.gz
%{sge_mandir}/man5/*.5.gz
%{sge_mandir}/man8/*.8.gz
%{sge_home}/examples
%{sge_lib}/*/libdrmaa.so*

%files devel
%defattr(-,root,root,-)
%{sge_include}
%{sge_home}/pvm/src
%{sge_mandir}/man3/*.3.gz
%if %{with java}
%doc %{sge_docdir}/javadocs
%endif
%{sge_home}/examples/drmaa

%files qmon
%defattr(-,root,root,-)
%{sge_bin}/*/qmon
%{sge_home}/qmon
%{sge_mandir}/man1/qmon.1.gz

%files execd
%defattr(-,root,root,-)
%{sge_bin}/*/sge_execd
%{sge_bin}/*/sge_shepherd
%{sge_bin}/*/sge_coshepherd
%{sge_home}/install_execd
%{sge_mandir}/man8/sge_execd.8.gz
%{sge_mandir}/man8/sge_*shepherd.8.gz
%{sge_mandir}/man8/pam*8.gz
%{sge_home}/utilbin/*/rshd
%{sge_lib}/*/pam*

%files qmaster
%defattr(-,root,root,-)
%{sge_bin}/*/qacct
%{sge_bin}/*/sge_qmaster
%{sge_bin}/*/sge_shadowd
%{sge_bin}/process-scheduler-log
%{sge_bin}/qsched
%{sge_bin}/jobstats
%{sge_home}/install_qmaster
%{sge_mandir}/man8/sge_qmaster.8.gz
%{sge_mandir}/man8/sge_shadowd.8.gz

%files drmaa4ruby
%defattr(-,root,root,-)
%{sge_home}/util/resources/drmaa4ruby

%changelog
* Thu Apr 12 2012 Dave Love <d.love@liverpool.ac.uk> - 8.0.0e
- Modify for changes in licence info and distinst.
- Consider setuid binaries.

* Wed Feb  1 2012 Dave Love <d.love@liverpool.ac.uk> - 8.0.0e
- Fix --without java

* Sat Oct  8 2011 Dave Love <d.love@liverpool.ac.uk> - 8.0.0pre_c-1
- Depend on hwloc.

* Mon Oct  3 2011 Dave Love <d.love@liverpool.ac.uk> - 8.0.0pre_c-1
- Update version; don't use Packager; fix Source.
- Require man.
- New package drmaa4ruby.
- Move qsched to qmaster package.

* Thu Aug 18 2011 Dave Love <d.love@liverpool.ac.uk> 8.0.0pre_b
- Add Requires: xorg-x11-fonts-ISO8859-1-100dpi xorg-x11-fonts-ISO8859-1-75dpi
  to qmon package [from the EPEL 6 package (Orion Poplawski)].
- Fix libdrmaa symlink [from Florian La Roche].

* Sat Jun 18 2011 Dave Love <d.love@liverpool.ac.uk> 8.0.0a
- Add --without java (adapted from Jesse Becker <hawson@gmail.com>).
- Use csh -f.  Add Prefix.  Use -fno-strict-aliasing.

* Thu May 25 2011 Dave Love <d.love@liverpool.ac.uk> - 8.0.0a-1
- Heavily re-written from Orion Poplawski's Fedora original for shared
  installation under /opt and not doing any configuration.  Different
  enough that it's probably not worh presevring the changelog.
