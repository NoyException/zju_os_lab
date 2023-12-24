export
CROSS_=riscv64-linux-gnu-
GCC=${CROSS_}gcc
LD=${CROSS_}ld
OBJCOPY=${CROSS_}objcopy
SCHEDULE ?= SJF

ISA=rv64imafd_zifencei
ABI=lp64

INCLUDE = -I $(shell pwd)/include -I $(shell pwd)/arch/riscv/include
CF = -march=$(ISA) -mabi=$(ABI) -mcmodel=medany -fno-builtin -ffunction-sections -fdata-sections -nostartfiles -nostdlib -nostdinc -static -lgcc -Wl,--nmagic -Wl,--gc-sections -g -D$(SCHEDULE)
CFLAG = ${CF} ${INCLUDE}

.PHONY:all run debug clean flash noflash qemu
all: clean
	${MAKE} -C user all
	${MAKE} -C lib all
	${MAKE} -C init all
	${MAKE} -C fs all
	${MAKE} -C arch/riscv all
	@echo -e '\n'Build Finished OK

# run: all
# 	@echo Launch the qemu ......
# 	@qemu-system-riscv64 -nographic -machine virt -kernel vmlinux -bios default 

# debug: all
# 	@echo Launch the qemu for debug ......
# 	@qemu-system-riscv64 -nographic -machine virt -kernel vmlinux -bios default -S -s
debug: all
	@echo Launch the qemu ......
	@qemu-system-riscv64 \
		-machine virt \
		-nographic \
		-bios default \
		-kernel vmlinux \
		-global virtio-mmio.force-legacy=false \
		-drive file=disk.img,if=none,format=raw,id=hd0 \
		-device virtio-blk-device,drive=hd0 \
		-S -s



# flash: all
# 	@echo Launch the qemu ......
# 	@../qemu-8.1.2/build/qemu-system-riscv64 -nographic -drive file=flash.img,if=pflash,format=raw,unit=1 -machine virt -kernel vmlinux -bios default 

qemu:
	@make -j192 -C ../qemu-8.1.2/

run: all
	@echo Launch the qemu ......
	@qemu-system-riscv64 \
		-machine virt \
		-nographic \
		-bios default \
		-kernel vmlinux \
		-global virtio-mmio.force-legacy=false \
		-drive file=disk.img,if=none,format=raw,id=hd0 \
		-device virtio-blk-device,drive=hd0

trace: all qemu
	@echo Launch the qemu ......
	@../qemu-8.1.2/build/qemu-system-riscv64 \
		-machine virt \
		-nographic \
		-bios default \
		-kernel vmlinux \
		-global virtio-mmio.force-legacy=false \
		-drive file=disk.img,if=none,format=raw,id=hd0 \
		-device virtio-blk-device,drive=hd0

# -machine dumpdtb=virtio.dtb

# flash: all
# 	qemu-system-riscv64 -nographic -blockdev node-name=pflash1,driver=file,filename=flash.img -machine virt -kernel vmlinux -bios default

noflash: all
	@echo Launch the qemu ......
	@../qemu-8.1.2/build/qemu-system-riscv64 -nographic -drive file=flash.img,if=pflash,format=raw,unit=1 -machine virt -kernel vmlinux -bios default 

clean:
	${MAKE} -C lib clean
	${MAKE} -C user clean
	${MAKE} -C fs clean
	${MAKE} -C init clean
	${MAKE} -C arch/riscv clean
	$(shell test -f vmlinux && rm vmlinux)
	$(shell test -f System.map && rm System.map)
	@echo -e '\n'Clean Finished

compile_commands.json: clean
	bear -- make
	sed -i 's/_zifencei//g' compile_commands.json
