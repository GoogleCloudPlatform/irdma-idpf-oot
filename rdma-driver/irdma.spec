%if 0%{?_IRDMA_SPREFIX_:1}
%define name %{_IRDMA_SPREFIX_}
%else
%define name irdma
%endif

Name: %{name}
Summary: Intel(R) Ethernet Connection E800 Series Linux iRDMA Driver

%if 0%{?_IRDMA_VER_:1}
%define version %{_IRDMA_VER_}
%else
%define version 2.9.0
%endif
%if 0%{?_IRDMA_REL_:1}
%define release %{_IRDMA_REL_}
%else
%define release 1
%endif
Version: %{version}
Release: %{release}
Source: %{name}-%{version}.tar.gz
Vendor: Google LLC
License: GPLv2 and Redistributable, no modification permitted
ExclusiveOS: linux
Group: System Environment/Kernel
Provides: %{name}
URL: http://support.intel.com
BuildRoot: %{_tmppath}/%{name}-%{version}-root

%global debug_package %{nil}
%global __strip /bin/true
%define find() %(for f in %*; do if [ -e $f ]; then echo $f; break; fi; done)
Requires: kernel, findutils, gawk, bash, dkms

# Check for existence of kernel_module_package_buildreqs ...
%if 0%{?!kernel_module_package_buildreqs:1}
# ... and provide a suitable definition if it is not defined
%define kernel_module_package_buildreqs kernel-devel
%endif
BuildRequires: %kernel_module_package_buildreqs

%description
This package contains the Intel(R) Ethernet Connection E800 Series Linux iRDMA Driver.

%prep
%setup

%build
%install

# Copy over source code
mkdir -p %{buildroot}/usr/src/%{name}-%{version}
cp -r * %{buildroot}/usr/src/%{name}-%{version}

# Setup depmod.d override
mkdir -p %{buildroot}/etc/depmod.d/
echo "override irdma * extra" >> %{buildroot}/etc/depmod.d/irdma.conf

cd %{buildroot}

echo "/usr/src/%{name}-%{version}/*" \
	>>%{_builddir}/%{name}-%{version}/file.list
echo "/etc/depmod.d/irdma.conf" \
	>>%{_builddir}/%{name}-%{version}/file.list
export _ksrc=%{_usrsrc}/kernels/%{kernel_ver}
cd %{buildroot}

%clean
rm -rf %{buildroot}

%files -f file.list
%defattr(-,root,root)
%doc file.list
%post
rmmod i40iw 2> /dev/null
rm -f /lib/modules/`uname -r`/kernel/drivers/infiniband/hw/i40iw/i40iw.ko 2> /dev/null
rm -f /lib/modules/`uname -r`/updates/drivers/infiniband/hw/i40iw/i40iw.ko 2> /dev/null
rm -f /lib/modules/`uname -r`/kernel/drivers/infiniband/hw/i40iw/i40iw.ko.xz 2> /dev/null
rm -f /lib/modules/`uname -r`/updates/drivers/infiniband/hw/i40iw/i40iw.ko.xz 2> /dev/null

echo "Creating /etc/modprobe.d/irdma.conf file ..."
mkdir -p "/etc/modprobe.d/"
if [ -e "/etc/modprobe.d/irdma.conf" ]; then
	if [ "" = "$(grep 'blacklist i40iw' /etc/modprobe.d/irdma.conf)" ]; then
		echo "blacklist i40iw"  >>  "/etc/modprobe.d/irdma.conf"
		echo "alias i40iw irdma" >> "/etc/modprobe.d/irdma.conf"
	fi
else
	echo "blacklist i40iw"  >  "/etc/modprobe.d/irdma.conf"
	echo "alias i40iw irdma" >> "/etc/modprobe.d/irdma.conf"
fi

dkms add -m %{name} -v %{version} -q --rpm_safe_upgrade || :
dkms build -m %{name} -v %{version} -q || :
dkms install -m %{name} -v %{version} -q --force || :

uname -r | grep BOOT || /sbin/depmod -a > /dev/null 2>&1 || true

if which dracut >/dev/null 2>&1; then
	echo "Updating initramfs with dracut..."
	if dracut --force ; then
		echo "Successfully updated initramfs."
	else
		echo "Failed to update initramfs."
		echo "You must update your initramfs image for changes to take place."
		exit -1
	fi
elif which mkinitrd >/dev/null 2>&1; then
	echo "Updating initrd with mkinitrd..."
	if mkinitrd; then
		echo "Successfully updated initrd."
	else
		echo "Failed to update initrd."
		echo "You must update your initrd image for changes to take place."
		exit -1
	fi
else
	echo "Unable to determine utility to update initrd image."
	echo "You must update your initrd manually for changes to take place."
	exit -1
fi

%preun
dkms remove -m %{name} -v %{version} -q --all --rpm_safe_upgrade || :

%postun

uname -r | grep BOOT || /sbin/depmod -a > /dev/null 2>&1 || true
if which dracut >/dev/null 2>&1; then
	echo "Updating initramfs with dracut..."
	if dracut --force ; then
		echo "Successfully updated initramfs."
	else
		echo "Failed to update initramfs."
		echo "You must update your initramfs image for changes to take place."
		exit -1
	fi
elif which mkinitrd >/dev/null 2>&1; then
	echo "Updating initrd with mkinitrd..."
	if mkinitrd; then
		echo "Successfully updated initrd."
	else
		echo "Failed to update initrd."
		echo "You must update your initrd image for changes to take place."
		exit -1
	fi
else
	echo "Unable to determine utility to update initrd image."
	echo "You must update your initrd manually for changes to take place."
	exit -1
fi
