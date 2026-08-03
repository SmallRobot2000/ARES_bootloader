riscv64-linux-gnu-as -march=rv32i -o test.o test.S
riscv64-linux-gnu-ld -m elf32lriscv -Ttext=0x90200000 test.o -o test.elf
riscv64-linux-gnu-objcopy -O binary test.elf image.bin
