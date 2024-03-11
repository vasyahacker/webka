NAME = test-router
all:
	@echo 'Usage:'
	@echo ' make vmdisk		- create disk for vm'
	@echo ' doas make bsdrd	- create custom bsd.rd'
	@echo ' doas make run-init	- start vm, auto install os and configure soft'
	@echo ' doas make run-bg	- start vm in background mode'
	@echo ' doas make run-fg	- start vm and connet to vm serial console'
	@echo ' make vmconf		- generate vm.conf for vmd'

bsdrd:
	cp /bsd.rd . || ftp https://mirror2.openbsd.org.ru/pub/OpenBSD/7.4/${MACHINE_ARCH}/bsd.rd
	gunzip bsd.rd -o bsdrd
	rdsetroot -dx bsdrd ramdisk
	mkdir -p mnt
	vnconfig ramdisk >vnd
	mount -w /dev/$$(<vnd)a mnt
	chmod 777 mnt
	vi auto_install.conf
	cp auto_install.conf mnt/
	vi disklabel.auto
	cp disklabel.auto mnt/
	cp rc.firsttime mnt/
	patch mnt/install.sub <firsttime.patch
	umount mnt
	vnconfig -u $$(<vnd)
	rm vnd
	rdsetroot bsdrd ramdisk
	gzip bsdrd -o bsd.rd
	chmod 666 bsd.rd
	rm -rf mnt ramdisk
run-init:
	vmctl start -c -m 256M -L -b ./bsd.rd -d ${NAME}.qcow2 "${NAME}"

run-bg:
	vmctl start -m 128M -L -d ${NAME}.qcow2 "${NAME}"

run-fg:
	vmctl start -c -m 128M -L -d ${NAME}.qcow2 "${NAME}"

vmdisk:
	vmctl create -s 1.5G ${NAME}.qcow2

vmconf:
	@echo "vm \"${NAME}\" {\n\tdisable\n\tmemory 128M\n\tdisk \"${.CURDIR}/${NAME}.qcow2\"\n\tlocal interface\n\towner ${USER}\n}">vm.conf
	cat vm.conf
	@echo "add include \"${.CURDIR}/vm.conf\" to /etc/vm.conf and restart vmd"
	@echo "Than vmctl start|stop [-c] ${NAME}"
clean:
	rm -rf mnt disk.img ramdisk bsdrd

