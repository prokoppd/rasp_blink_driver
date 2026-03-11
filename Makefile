obj-m += rasp-blink.o

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD)

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules_install

clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c
	rm -f Module.markers Module.symvers modules.order
	rm -rf .tmp_versions Modules.symvers

cppcheck:
	cppcheck \
		--checkers-report=report.txt \
		--enable=all \
		--inconclusive \
		--force \
		--inline-suppr \
		-D__KERNEL__ \
		-DMODULE \
		-D__GNUC__=15 \
		-D__GNUC_MINOR__=2 \
		-D__GNUC_PATCHLEVEL__=1 \
		--suppress=missingIncludeSystem \
		qspi_interface.c
