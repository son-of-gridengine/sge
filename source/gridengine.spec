# This is the Fedora gridengine spec file.  It's not currently useful
# with this source, but that's to be fixed with high priority.

# The licence on this file itself needs to be clarified -- it's not
# clear whether it comes under the SISSL or something else.

%bcond_without  fedora
%define uid 43
%define username sgeadmin
%define homedir %{_datadir}/gridengine
%define gecos Grid Engine

Name:    gridengine
Version: 6.2u5
Release: 5%{?dist}
Summary: Grid Engine - Distributed Computing Management software

Group:   Applications/System
# Only the file %{_libexecdir}/gridengine/bin/*/qmake is
# under GPLv2+, which is not used or linked by other parts
# of gridengine.
# The file %{_libexecdir}/gridengine/bin/*/qtcsh is
# under BSD with advertising, 
# which is not used or linked by other parts of gridengine.
License: (BSD and LGPLv2+ and MIT and SISSL) and GPLv2+ and BSD with advertising
URL:     http://gridengine.sunsource.net/
#This is make with maketarball <TAG>
Source0: ge-V62u5_TAG-src.tar.bz2
Source1: gridengine-ppc.tar.gz
Source2: conf_defaults
Source3: sge.csh
Source4: sge.sh
Source5: sge_execd
Source6: sgemaster
Source7: bootstrap
Source8: Licenses
Source9: gridengine.sysconfig
Source10: http://gridengine.sunsource.net/nonav/issues/showattachment.cgi/165/libcore.c
Source11: README
Source12: maketarball
# Link ssl libraries dynamically so dependencies are pulled in
# http://gridengine.sunsource.net/issues/show_bug.cgi?id=2845
Patch1: gridengine-6.2beta-ssl.patch
# Make inst_common.sh not look for qmon or sge_execd since they might not be installed
Patch2: gridengine-6.2beta-inst.patch
# Don't need to make rc files in inst_common.sh
# Partially http://gridengine.sunsource.net/issues/show_bug.cgi?id=2780
Patch3: gridengine-6.2u5-rctemplates.patch
# Fixup sge_ca to use system openssl and java paths
Patch4: gridengine-6.2u2_1-sge_ca.patch
# Fixup jni paths
Patch5: gridengine-6.2-jni.patch
# Use system db_ utils
Patch6: gridengine-6.2-db.patch
# aimk sets -Werror, but there are lots of warnings
# http://gridengine.sunsource.net/issues/show_bug.cgi?id=2112
Patch7: gridengine-6.2u2_1-Werror.patch
# Need to patch -Dant.library.dir=/usr/share/ant/lib to ant processes
# http://gridengine.sunsource.net/issues/show_bug.cgi?id=3171
Patch8: gridengine-6.2u4-ant.patch
# Rename getline() to sge_getline()
# http://gridengine.sunsource.net/issues/show_bug.cgi?id=2980
Patch9: gridengine-6.2u2_1-getline.patch
# Support lesstif - http://gridengine.sunsource.net/issues/show_bug.cgi?id=2310
Patch15: gridengine-6.2beta2-lesstif.patch
# Make inst_sge exit with status 1 if usage is incorrect
# http://gridengine.sunsource.net/issues/show_bug.cgi?id=2586
Patch19: gridengine-6.2u4-auto.patch
# Don't use rpaths
Patch22: gridengine-6.2u2_1-rpath.patch
# Fix linking with proper libs
# http://gridengine.sunsource.net/issues/show_bug.cgi?id=2588
Patch25: gridengine-6.2u2_1-libs.patch
# Handle ignoring return codes
Patch26: gridengine-6.2beta2-error.patch
# Workaround for openssl-1.0 API change
Patch27: gridengine-6.2u3-openssl.patch
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
ExcludeArch: ppc64

BuildRequires: /bin/csh, openssl-devel, db4-devel, ncurses-devel, pam-devel
BuildRequires: libXmu-devel, libXpm-devel
%if 0%{?rhel}
BuildRequires: openmotif-devel
%else
BuildRequires: lesstif-devel
%endif
BuildRequires: java-devel >= 1.6.0, javacc, ant-junit, ant-nodeps
BuildRequires: elfutils-libelf-devel, net-tools
BuildRequires: fedora-usermgmt-devel
Requires: binutils
Requires: ncurses
Requires(posttrans): /usr/sbin/alternatives
Requires(preun): /usr/sbin/alternatives
%{?FE_USERADD_REQ}


%description
In a typical network that does not have distributed resource management
software, workstations and servers are used from 5% to 20% of the time.
Even technical servers are generally less than fully utilized. This
means that there are a lot of cycles that can be used productively if
only users know where they are, can capture them, and put them to work.

Grid Engine finds a pool of idle resources and harnesses it
productively, so an organization gets as much as five to ten times the
usable power out of systems on the network. That can increase utilization
to as much as 98%.

Grid Engine software aggregates available compute resources and
delivers compute power as a network service.

These are the local files shared by both the qmaster and execd
daemons. You must install this package in order to use any one of them.


%package devel
Summary: Gridengine development files
Group: Development/Libraries
License: BSD and LGPLv2+ and MIT and SISSL
Requires: %{name} = %{version}-%{release}

%description devel
gridengine development headers and libraries.


%package qmon
Summary: Gridengine qmon monitor
Group: Development/Libraries
License: BSD and LGPLv2+ and MIT and SISSL
Requires: %{name} = %{version}-%{release}

%description qmon
The qmon graphical grid engine monitor


%package execd
Summary: Gridengine execd program
Group: Development/Libraries
License: BSD and LGPLv2+ and MIT and SISSL
Requires: %{name} = %{version}-%{release}
Requires(post): /sbin/chkconfig
Requires(postun): /sbin/service
Requires(postun): %{name} = %{version}-%{release}
Requires(preun): /sbin/chkconfig
Requires(preun): /sbin/service
Requires(preun): %{name} = %{version}-%{release}

%description execd
Programs needed to run a grid engine execution host


%package qmaster
Summary: Gridengine qmaster programs
Group: Development/Libraries
License: BSD and LGPLv2+ and MIT and SISSL
Requires: %{name} = %{version}-%{release}
Requires: db4-utils
Requires(post): /sbin/chkconfig
Requires(postun): /sbin/service
Requires(postun): %{name} = %{version}-%{release}
Requires(preun): /sbin/chkconfig
Requires(preun): /sbin/service
Requires(preun): %{name} = %{version}-%{release}

%description qmaster
Programs needed to run a grid engine qmaster host


%prep
%setup -q -n gridengine -a 1
#Copy Licenses and README file into build directory
cp %SOURCE8 %SOURCE11 .
#Remove unneeded shbangs
sed -i -e '/^#! *\/bin\/sh/d' source/dist/util/install_modules/*.sh
%patch1 -p1 -b .ssl
%patch2 -p1 -b .inst
%patch3 -p1 -b .rctemplates
%patch4 -p1 -b .sge_ca
%patch5 -p1 -b .jni
%patch6 -p1 -b .db
%patch7 -p1 -b .Werror
%patch8 -p1 -b .ant
%patch9 -p1 -b .getline
%if !0%{?rhel}
%patch15 -p1 -b .lesstif
%endif
%patch19 -p1 -b .auto
%patch22 -p1 -b .rpath
%patch25 -p1 -b .libs
%patch26 -p1 -b .error
%patch27 -p1 -b .openssl
sed -i.arch -e 's,/\$DSTARCH,,g' source/scripts/distinst
#Don't ship rctemplates
rm -rf source/dist/util/rctemplates
#Don't ship windows .bat scripts
find source -name \*.bat | xargs rm
#Fix permissions
chmod +x source/dist/util/dl?.*sh 
chmod -x source/dist/util/dl.*sh 
find source -name \*.c | xargs chmod -x
#Fix flags for qmake build
find source/3rdparty/qmake source/3rdparty/qtcsh -name Makefile | 
  xargs sed -i -e "/^CFLAGS *=/s/=/= $RPM_OPT_FLAGS/"
#dlopen the runtime libssl library
soname=$(objdump -p %{_libdir}/libssl.so | awk '/SONAME/ {print $2}')
sed -i -e s/libssl\.so/$soname/ source/libs/comm/cl_ssl_framework.c
#Fix xterm path
sed -i -e s,X11/xterm,xterm,g doc/htmlman/htmlman5/sge_conf.html \
                              doc/man/man5/sge_conf.5 \
                              source/dist/util/arch_variables \
                              source/libs/sgeobj/sge_conf.c


%build
export JAVA_HOME=%{java_home}
cd source
#Setup paths
cat > aimk.private <<EOF
set OPENSSL_HOME = /usr
set BERKELEYDB_HOME = /usr
set BDB_INCLUDE_SUBDIR = ../include
set BDB_LIB_SUBDIR = ../%{_lib}
set KRB_HOME = /usr
set MAN2HTMLPATH = /usr/bin
set GROFFPATH = /usr/bin
set SWIG = /usr/bin/swig
set PERL = /usr/bin/perl
set TCLSH = /usr/bin/tclsh
set JUNIT_JAR = /usr/share/java/junit.jar
set CORE_HOME = `pwd`
EOF
cat > build_private.properties <<EOF
javacc.home=%{_javadir}
libs.junit.classpath=%{_javadir}/junit.jar
izpack.lib=/usr/share/java/izpack
hadoop.javac.source=1.6
hadoop.javac.target=1.6
hadoop.home=/usr/share/java
EOF

#Build libcore.so
gcc $RPM_OPT_FLAGS -D_GNU_SOURCE -fPIC -c %SOURCE10 -o libcore.o
gcc $RPM_OPT_FLAGS -shared -o libcore.so libcore.o -lpthread
export SGE_INPUT_CFLAGS="$RPM_OPT_FLAGS"
touch aimk
./aimk -only-depend
scripts/zerodepend
./aimk depend
#TODO - Need IzPack for GUI Installer
./aimk -no-gui-inst
./aimk -man 
#Not build by default - going to need hadoop
#ant herd


%install 
rm -rf $RPM_BUILD_ROOT

#Set the gridengine arch
gearch=`%{_builddir}/gridengine/source/dist/util/arch`
# set up the target installation directory
export SGE_ROOT=$RPM_BUILD_ROOT%{_datadir}/gridengine
mkdir -p $SGE_ROOT
cd source
echo 'y'| scripts/distinst -nobdb -noopenssl -local -allall -noexit ${gearch}

#Create default install configuration
cp dist/util/install_modules/inst_template.conf \
   $RPM_BUILD_ROOT%{_datadir}/gridengine/my_configuration.conf
cat %SOURCE2 | while read line
do
  key=${line/=*/}
  value=${line/*=/}
  sed -i -e "/^${key}=/s,=.*,=$value," $RPM_BUILD_ROOT%{_datadir}/gridengine/my_configuration.conf
done

install -p -m755 `scripts/compilearch -c ${gearch}`/qevent $RPM_BUILD_ROOT%{_datadir}/gridengine/bin

# man - do before the alternatives rename below
mkdir -p $RPM_BUILD_ROOT%{_mandir}
mv $RPM_BUILD_ROOT%{_datadir}/gridengine/man/man* $RPM_BUILD_ROOT%{_mandir}

# Move things to the right location, making links back
# bin
mv $RPM_BUILD_ROOT%{_datadir}/gridengine/bin $RPM_BUILD_ROOT%{_prefix}
rmdir $RPM_BUILD_ROOT%{_bindir}/${gearch}
mkdir -p $RPM_BUILD_ROOT%{_datadir}/gridengine/bin/${gearch}
# Rename common queuing binaries and manpages for use with alternatives
for bin in qalter qdel qhold qmake qrls qselect qstat qsub
do
    if [ -L $RPM_BUILD_ROOT%{_bindir}/$bin ]
    then
        target=`readlink $RPM_BUILD_ROOT%{_bindir}/$bin`
        rm $RPM_BUILD_ROOT%{_bindir}/$bin
        ln -s ${target}-ge $RPM_BUILD_ROOT%{_bindir}/${bin}-ge
    else
        mv $RPM_BUILD_ROOT%{_bindir}/$bin $RPM_BUILD_ROOT%{_bindir}/${bin}-ge
    fi
    mv $RPM_BUILD_ROOT%{_mandir}/man1/${bin}.1 $RPM_BUILD_ROOT%{_mandir}/man1/${bin}-ge.1
done
for bin in `find $RPM_BUILD_ROOT%{_bindir} -type f -o -type l`
do
    ln -s ../../../../bin/`basename $bin` \
          $RPM_BUILD_ROOT%{_datadir}/gridengine/bin/${gearch}/`basename $bin -ge`
done

# utilbin
mkdir -p $RPM_BUILD_ROOT%{_libexecdir}/gridengine/utilbin
mv $RPM_BUILD_ROOT%{_datadir}/gridengine/utilbin/* \
   $RPM_BUILD_ROOT%{_libexecdir}/gridengine/utilbin
ln -s ../../../libexec/gridengine/utilbin \
      $RPM_BUILD_ROOT%{_datadir}/gridengine/utilbin/${gearch}
# We also need some db utils
ln -s ../../../bin/db_dump ../../../bin/db_load \
      $RPM_BUILD_ROOT%{_datadir}/gridengine/utilbin/

# lib
mkdir -p $RPM_BUILD_ROOT%{_prefix}
mv $RPM_BUILD_ROOT%{_datadir}/gridengine/lib $RPM_BUILD_ROOT%{_libdir}
ln -s ../../%{_lib}/gridengine $RPM_BUILD_ROOT%{_datadir}/gridengine/lib
# libcore.so
install -p -m755 libcore.so $RPM_BUILD_ROOT%{_libdir}
# Move the JNI libraries
mkdir -p $RPM_BUILD_ROOT%{_libdir}/%{name}
for jni in jgdi juti
do
  mv $RPM_BUILD_ROOT%{_libdir}/${jni}.jar $RPM_BUILD_ROOT%{_libdir}/lib${jni}.so \
     $RPM_BUILD_ROOT%{_libdir}/%{name}/
done
mkdir -p $RPM_BUILD_ROOT%{_javadir}
mv $RPM_BUILD_ROOT%{_libdir}/drmaa.jar $RPM_BUILD_ROOT%{_javadir}/drmaa.jar
mv $RPM_BUILD_ROOT%{_libdir}/JSV.jar $RPM_BUILD_ROOT%{_javadir}/JSV.jar

# include
mv $RPM_BUILD_ROOT%{_datadir}/gridengine/include $RPM_BUILD_ROOT%{_includedir}

# app-defaults
mkdir -p $RPM_BUILD_ROOT%{_datadir}/X11/app-defaults
mv $RPM_BUILD_ROOT%{_datadir}/gridengine/qmon/*.ad \
   $RPM_BUILD_ROOT%{_datadir}/X11/app-defaults

# The default cell directories
mkdir -p $RPM_BUILD_ROOT%{_datadir}/gridengine/default/common

# The default qmaster, spool, and spooldb directories
mkdir -p $RPM_BUILD_ROOT%{_localstatedir}/spool/gridengine/default/qmaster
mkdir -p $RPM_BUILD_ROOT%{_localstatedir}/spool/gridengine/default/spool/{admin_hosts,cqueues,job_scripts,resource_quotas,zombies,calendars,exec_hosts,pe,submit_hosts,centry,hostgroups,projects,users,ckpt,jobs,qinstances,usersets}
mkdir -p $RPM_BUILD_ROOT%{_localstatedir}/spool/gridengine/default/spooldb

# Bootstrap file
#cp %SOURCE7 $RPM_BUILD_ROOT%{_datadir}/gridengine/default/common/

# These files get created during user setup (install_qmaster)
for f in common/{act_qmaster,bootstrap,qtask,settings.csh,settings.sh,sge_aliases,sgeexecd,sgemaster,sge_request}
do
   touch $RPM_BUILD_ROOT%{_datadir}/gridengine/default/${f}
done
for f in qmaster/job_scripts spooldb/{__db.00{1,2,3,4,5,6},log.0000000001,sge,sge_job}
do
   touch $RPM_BUILD_ROOT%{_localstatedir}/spool/gridengine/default/${f}
done

# Environment (SGE_ROOT)
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/profile.d/
install -p -m644 %SOURCE3 %SOURCE4 $RPM_BUILD_ROOT%{_sysconfdir}/profile.d/

# Startup scripts
mkdir -p $RPM_BUILD_ROOT%{_initrddir}
install -p -m755 %SOURCE5 %SOURCE6 $RPM_BUILD_ROOT%{_initrddir}
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig
install -p -m644 %SOURCE9 $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig/gridengine

#sgeCA
mkdir -p $RPM_BUILD_ROOT%{_localstatedir}/sgeCA

# Don't package catman files
rm -rf $RPM_BUILD_ROOT%{_datadir}/gridengine/catman

# Rename some man pages to avoid conflicts
pushd $RPM_BUILD_ROOT%{_mandir}/man5
for f in *
do
   if [ ${f:0:4} != sge_ ]
   then
      mv $f sge_$f
   fi
done
popd

#Don't need these
rm -rf $RPM_BUILD_ROOT%{_datadir}/gridengine/3rd_party
rm -rf $RPM_BUILD_ROOT%{_datadir}/gridengine/dtrace

#Don't ship example binaries
rm -rf $RPM_BUILD_ROOT%{_datadir}/gridengine/examples/jobsbin
rm -rf $RPM_BUILD_ROOT%{_datadir}/gridengine/examples/worker.*

#Cleanup some patch backups that get shipped
rm $RPM_BUILD_ROOT%{_datadir}/gridengine/util/install_modules/inst_*.sh.*

#TODO - Ship the GUI installer
rm $RPM_BUILD_ROOT%{_datadir}/gridengine/start_gui_installer


%clean
rm -rf $RPM_BUILD_ROOT


%pre
%__fe_groupadd %uid -r %username &>/dev/null || :
%__fe_useradd  %uid -r -s /sbin/nologin -d %homedir -M          \
                    -c '%gecos' -g %username %username &>/dev/null || :

%post -p /sbin/ldconfig

%posttrans
alternatives --install %{_bindir}/qsub qsub %{_bindir}/qsub-ge 10 \
        --slave %{_mandir}/man1/qsub.1.gz qsub-man \
                %{_mandir}/man1/qsub-ge.1.gz \
        --slave %{_bindir}/qalter qalter %{_bindir}/qalter-ge \
        --slave %{_mandir}/man1/qalter.1.gz qalter-man \
                %{_mandir}/man1/qalter-ge.1.gz \
        --slave %{_bindir}/qdel qdel %{_bindir}/qdel-ge \
        --slave %{_mandir}/man1/qdel.1.gz qdel-man \
                %{_mandir}/man1/qdel-ge.1.gz \
        --slave %{_bindir}/qhold qhold %{_bindir}/qhold-ge \
        --slave %{_mandir}/man1/qhold.1.gz qhold-man \
                %{_mandir}/man1/qhold-ge.1.gz \
        --slave %{_bindir}/qrls qrls %{_bindir}/qrls-ge \
        --slave %{_mandir}/man1/qrls.1.gz qrls-man \
                %{_mandir}/man1/qrls-ge.1.gz \
        --slave %{_bindir}/qselect qselect %{_bindir}/qselect-ge \
        --slave %{_mandir}/man1/qselect.1.gz qselect-man \
                %{_mandir}/man1/qselect-ge.1.gz \
        --slave %{_bindir}/qstat qstat %{_bindir}/qstat-ge \
        --slave %{_mandir}/man1/qstat.1.gz qstat-man \
                %{_mandir}/man1/qstat-ge.1.gz

%preun
alternatives --remove qsub %{_bindir}/qsub-ge

%postun -p /sbin/ldconfig


%post execd
/sbin/chkconfig --add sge_execd

%postun execd
if [ "$1" -ge "1" ]; then
        /sbin/service sge_execd condrestart >/dev/null 2>&1 || :
fi

%preun execd
if [ $1 -eq 0 ]; then
    /sbin/service sge_execd stop
    /sbin/chkconfig --del sge_execd
fi


%post qmaster
/sbin/chkconfig --add sgemaster

%postun qmaster
if [ "$1" -ge "1" ]; then
        /sbin/service sgemaster condrestart >/dev/null 2>&1 || :
fi

%preun qmaster
if [ $1 -eq 0 ]; then
    /sbin/service sgemaster stop
    /sbin/chkconfig --del sgemaster
fi


%files
%defattr(-,root,root,-)
%doc Licenses README
%config(noreplace) %{_sysconfdir}/profile.d/sge.*
%config(noreplace) %{_sysconfdir}/sysconfig/gridengine
# Only the file %{_bindir}/qmake-ge is
# under GPLv2+
# Olny the file %{_bindir}/qtcsh is
# under BSD with advertising
%{_bindir}/*
%exclude %{_bindir}/qacct
%exclude %{_bindir}/qmon
%exclude %{_bindir}/sge_execd
%exclude %{_bindir}/sge_qmaster
%{_libdir}/%{name}
%{_libdir}/libcore.so
%{_libdir}/libdrmaa.so.*
%{_libdir}/libspoolb.so
%{_libdir}/libspoolc.so
%{_javadir}/drmaa.jar
%{_javadir}/JSV.jar
%{_libexecdir}/gridengine/
%attr(-,%username,%username) %dir %{_datadir}/gridengine
%{_datadir}/gridengine/bin
%exclude %{_datadir}/gridengine/bin/*/qacct
%exclude %{_datadir}/gridengine/bin/*/qmon
%exclude %{_datadir}/gridengine/bin/*/sge_execd
%exclude %{_datadir}/gridengine/bin/*/sge_qmaster
%{_datadir}/gridengine/ckpt
%attr(-,%username,%username) %ghost %{_datadir}/gridengine/default
%{_datadir}/gridengine/doc
%{_datadir}/gridengine/hadoop
%{_datadir}/gridengine/inst_sge
%{_datadir}/gridengine/lib
%{_datadir}/gridengine/mpi
%{_datadir}/gridengine/my_configuration.conf
%dir %{_datadir}/gridengine/pvm
%{_datadir}/gridengine/pvm/*
%exclude %{_datadir}/gridengine/pvm/src
%{_datadir}/gridengine/util
%dir %{_datadir}/gridengine/utilbin
%{_datadir}/gridengine/utilbin/*
%exclude %{_datadir}/gridengine/utilbin/db_*
%{_mandir}/man1/*.1*
%exclude %{_mandir}/man1/qmon.1*
%{_mandir}/man5/*.5*
%{_mandir}/man8/*.8*
%exclude %{_mandir}/man8/sge_qmaster.8*
%exclude %{_mandir}/man8/sge_execd.8*
%attr(-,%username,%username) %dir %{_localstatedir}/spool/gridengine
%attr(-,%username,%username) %dir %{_localstatedir}/spool/gridengine/default

%files devel
%defattr(-,root,root,-)
%{_includedir}/*
%{_libdir}/libdrmaa.so
%{_datadir}/gridengine/examples
%{_datadir}/gridengine/pvm/src
%{_mandir}/man3/*.3*

%files qmon
%defattr(-,root,root,-)
%{_bindir}/qmon
%{_libdir}/libXltree.so
%{_datadir}/X11/app-defaults/*
%{_datadir}/gridengine/bin/*/qmon
%{_datadir}/gridengine/qmon/
%{_mandir}/man1/qmon.1*

%files execd
%defattr(-,root,root,-)
%{_initrddir}/sge_execd
%{_bindir}/sge_execd
%{_datadir}/gridengine/install_execd
%{_datadir}/gridengine/bin/*/sge_execd
%{_mandir}/man8/sge_execd.8*

%files qmaster
%defattr(-,root,root,-)
%{_initrddir}/sgemaster
%{_bindir}/qacct
%{_bindir}/sge_qmaster
%{_datadir}/gridengine/bin/*/qacct
%{_datadir}/gridengine/bin/*/sge_qmaster
%{_datadir}/gridengine/install_qmaster
%{_datadir}/gridengine/utilbin/db_*
%{_mandir}/man8/sge_qmaster.8*
%attr(-,%username,%username) %dir %{_localstatedir}/sgeCA
%attr(-,%username,%username) %ghost %{_localstatedir}/spool/gridengine/default/qmaster
%attr(-,%username,%username) %ghost %{_localstatedir}/spool/gridengine/default/spool
%attr(-,%username,%username) %ghost %{_localstatedir}/spool/gridengine/default/spooldb


%changelog
* Wed Aug 25 2010 - Orion Poplawski <orion@cora.nwra.com> - 6.2u5-5
- Update instructions to referece ./my_configuration.conf (bug #557628)
- Don't set IFS when reading conf file, breaks lots of stuff
- Set SGE_JVM_LIB_PATH to none by default so we don't setup JMF

* Wed Aug 11 2010 - Orion Poplawski <orion@cora.nwra.com> - 6.2u5-4
- Use upstream my_configuration.conf as template for default one (bugs 557628,566294)
- Set SGE_CELL in sge.sh/sge.csh (bug 620907)

* Mon Jul 12 2010 - Orion Poplawski <orion@cora.nwra.com> - 6.2u5-3
- Exclude ppc64 - no java 1.6.0

* Tue Feb 16 2010 - Orion Poplawski <orion@cora.nwra.com> - 6.2u5-2
- Update ssl patch to add -lcrypt to fix FTBFS bug #565003

* Mon Dec 28 2009 - Orion Poplawski <orion@cora.nwra.com> - 6.2u5-1
- Update to 6.2u5

* Thu Oct 29 2009 - Orion Poplawski <orion@cora.nwra.com> - 6.2u4-1
- Update to 6.2u4
- Drop import patch fixed upstream
- Updated rctemplates and auto patches
- Add patch to call ant with appropriate library directory

* Tue Aug 25 2009 Tomas Mraz <tmraz@redhat.com> - 6.2u3-3
- rebuilt with new openssl

* Fri Jul 24 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 6.2u3-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild

* Fri Jul 14 2009 - Orion Poplawski <orion@cora.nwra.com> - 6.2u3-1
- Update to 6.2u3
- Drop ppc patch fixed upstream
- Update rctemplates patch

* Mon Apr 6 2009 - Orion Poplawski <orion@cora.nwra.com> - 6.2u2_1-1
- Update to 6.2u2_1
- Rebase several patches
- Add patch to rename getline()
- Add patch to compile with correct libs on ppc/ppc64

* Tue Feb 24 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 6.2u1-4
- Rebuilt for https://fedoraproject.org/wiki/Fedora_11_Mass_Rebuild

* Thu Feb 19 2009 - Orion Poplawski <orion@cora.nwra.com> - 6.2u1-3
- Add patch to import specific classes to avoid pulling in other ones

* Thu Jan 15 2009 - Orion Poplawski <orion@cora.nwra.com> - 6.2u1-2
- Rebuild with openssl-0.9.8j

* Thu Dec 18 2008 - Orion Poplawski <orion@cora.nwra.com> - 6.2u1-1
- Update to 6.2u1

* Mon Nov 17 2008 - Orion Poplawski <orion@cora.nwra.com> - 6.2-5
- Leave qmake-ge in %{_bindir}
- Use system db_* utils in bdb_checkpoint script
- Fix xterm path in default setup
- Change java BR to >= 1.6.0 to allow building with other 1.6 javas

* Tue Nov 11 2008 - Orion Poplawski <orion@cora.nwra.com> - 6.2-4
- Add note to README about localhost line in /etc/hosts
- Cleanup setting.sh some, no more MAN stuff
- Add conditional build support for EL

* Wed Nov 5 2008 - Orion Poplawski <orion@cora.nwra.com> - 6.2-3
- Add Requires: ncurses for "clear"
- Add patch from CVS to update install scripts
- Patch sge_ca script not to use system openssl
- Modify code to dlopen runtime ssl library
- Patch install_qmaster to update /etc/sysconfig/gridengine
- Point install scripts to proper JAVA_HOME
- Have sgemaster init script use /etc/sysconfig/gridengine
- Add patch to point to jni library locations
- Make sure install_qmaster sets up default managers/operators

* Fri Sep 26 2008 - Orion Poplawski <orion@cora.nwra.com> - 6.2-2
- No more sge_schedd in 6.2, remove from startup script

* Mon Aug 11 2008 - Orion Poplawski <orion@cora.nwra.com> - 6.2-1
- Update to 6.2 final

* Tue Jul 15 2008 - Orion Poplawski <orion@cora.nwra.com> - 6.2beta2-0.1
- Provide some installation instructions for these RPMs
- Fix up some installation script issues
- Ghost more directories created later

* Thu Jun 5 2008 - Orion Poplawski <orion@cora.nwra.com> - 6.2beta-0
- Update to 6.2beta
- Use aimk.private and build_private.properties instead of patching
  aimk and build.properties
- Cleanup some extra /s in paths
- Shift to Java 1.6.0 because of new requirements in the source
- Move Java JNI libraries to proper location
- Drop several upstreamed patches
- Add patch to avoid linking unneeded libraries

* Fri Apr 11 2008 - Orion Poplawski <orion@cora.nwra.com> - 6.1u4-1
- Update to 6.1u4

* Tue Apr 1 2008 - Orion Poplawski <orion@cora.nwra.com> - 6.1u3-7
- Use alternatives to avoid conflicts with torque (bug #437613)
- Add patch to support stricter csh variable handling

* Fri Feb  8 2008 - Orion Poplawski <orion@cora.nwra.com> - 6.1u3-6
- Fixup subpackage License tags
- Service name change in scriptlets

* Thu Feb  7 2008 - Orion Poplawski <orion@cora.nwra.com> - 6.1u3-5
- Rewrite initscripts
- Remove spurious Requires(post): /sbin/ldconfig
- Add License explanation file and fix License tags

* Mon Feb  4 2008 - Orion Poplawski <orion@cora.nwra.com> - 6.1u3-4
- Drop arch from source
- Fix Requires() for main package
- Move man3 to -devel

* Fri Feb  1 2008 - Orion Poplawski <orion@cora.nwra.com> - 6.1u3-3
- Fix various review comments and rpmlint issues (bug #316141)

* Thu Jan 31 2008 - Orion Poplawski <orion@cora.nwra.com> - 6.1u3-2
- Actually ship sge_execd in the execd subpackage
- Don't complain about missing sge_execd during inst_sge -upd
- Link in db_dump and db_load into utilbin so that update scripts can find them

* Wed Jan  9 2008 - Orion Poplawski <orion@cora.nwra.com> - 6.1u3-1
- Update to 6.1u3
- Split execd into sub-package

* Thu Nov 15 2007 - Orion Poplawski <orion@cora.nwra.com> - 6.1u2-5
- Add BR net-tools for hostname for java build on devel

* Mon Nov 12 2007 - Orion Poplawski <orion@cora.nwra.com> - 6.1u2-4
- Add patch and source for ppc/ppc64 builds

* Fri Nov  1 2007 - Orion Poplawski <orion@cora.nwra.com> - 6.1u2-3
- Add patch to fix qstat xml output

* Thu Oct 18 2007 - Orion Poplawski <orion@cora.nwra.com> - 6.1u2-2
- Cleanup arch handling
- Install qevent

* Tue Oct  2 2007 - Orion Poplawski <orion@cora.nwra.com> - 6.1u2-1
- Fedora packaging
