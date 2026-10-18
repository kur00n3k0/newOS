# Toolchain: uses the x86_64-elf cross toolchain if installed, else the host one.
ifneq ($(shell which x86_64-elf-gcc 2>/dev/null),)
CC := x86_64-elf-gcc
LD := x86_64-elf-ld
else
CC := gcc
LD := ld -m elf_x86_64
endif

CFLAGS := -c -I src/intf -ffreestanding -fno-pie -fno-stack-protector \
          -mno-red-zone -mno-sse -mno-mmx -fno-tree-loop-distribute-patterns -fno-asynchronous-unwind-tables -Wall -Wextra -O2

kernel_source_files := $(shell find src/impl/kernel -name '*.c')
kernel_object_files := $(patsubst src/impl/kernel/%.c, build/kernel/%.o, $(kernel_source_files))

x86_64_c_source_files := $(shell find src/impl/x86_64 -name '*.c')
x86_64_c_object_files := $(patsubst src/impl/x86_64/%.c, build/x86_64/%.o, $(x86_64_c_source_files))

x86_64_asm_source_files := $(shell find src/impl/x86_64 -name '*.asm')
x86_64_asm_object_files := $(patsubst src/impl/x86_64/%.asm, build/x86_64/%.o, $(x86_64_asm_source_files))

x86_64_object_files := $(x86_64_c_object_files) $(x86_64_asm_object_files)

$(kernel_object_files): build/kernel/%.o : src/impl/kernel/%.c
	mkdir -p $(dir $@) && \
	$(CC) $(CFLAGS) $< -o $@

$(x86_64_c_object_files): build/x86_64/%.o : src/impl/x86_64/%.c
	mkdir -p $(dir $@) && \
	$(CC) $(CFLAGS) $< -o $@

$(x86_64_asm_object_files): build/x86_64/%.o : src/impl/x86_64/%.asm
	mkdir -p $(dir $@) && \
	nasm -f elf64 $< -o $@

.PHONY: build-x86_64 run clean
build-x86_64: $(kernel_object_files) $(x86_64_object_files)
	mkdir -p dist/x86_64 && \
	$(LD) -n -o dist/x86_64/kernel.bin -T targets/x86_64/linker.ld $(kernel_object_files) $(x86_64_object_files) && \
	cp dist/x86_64/kernel.bin targets/x86_64/iso/boot/kernel.bin && \
	grub-mkrescue -o dist/x86_64/kernel.iso targets/x86_64/iso

run: build-x86_64
	qemu-system-x86_64 -cdrom dist/x86_64/kernel.iso

clean:
	rm -rf build dist targets/x86_64/iso/boot/kernel.bin
