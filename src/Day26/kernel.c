// gcc -m32 -c kernel.c -o kernel.o -ffreestanding -nostdlib
void kernel_main(void) {
    const char *str = "Long Mode Activated!";
    volatile char *video = (volatile char *)0xB8000;

    for (int i = 0; str[i] != '\0'; i++) {
        video[i * 2] = str[i];
        video[i * 2 + 1] = 0x0F;
    }

    while(1);
}