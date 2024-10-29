include common.mk

KERN = kernel
USER = user
KERNEL_ELF = kernel-qemu
CPUNUM = 2
FS_IMG = none

.PHONY: clean $(KERN) $(USER)

$(KERN):
	$(MAKE) build --directory=$@

$(USER):
	$(MAKE) init --directory=$@

# QEMU相关配置
QEMU     =  qemu-system-riscv64
QEMUOPTS =  -machine virt -bios none -kernel $(KERNEL_ELF) 
QEMUOPTS += -m 128M -smp $(CPUNUM) -nographic

# 调试
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)

build: $(KERN)

# qemu运行
qemu: clean $(USER) $(KERN)
	$(QEMU) $(QEMUOPTS)

.gdbinit-gui: .gdbinit.tmpl-riscv-gui
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

qemu-gdb-gui: clean $(KERN) $(USER) .gdbinit-gui
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

qemu-gdb: clean $(KERN) $(USER) .gdbinit
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

clean:
	$(MAKE) --directory=$(KERN) clean
	rm -f $(KERNEL_ELF) .gdbinit .gdbinit-gui
