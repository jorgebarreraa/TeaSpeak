/*
 * Stub for arc4random_addrandom - This function is deprecated and not available
 * in modern libc implementations. Libevent may call it when built against BoringSSL.
 * This stub prevents linker errors.
 */

void arc4random_addrandom(unsigned char *dat, int datlen) {
    /* No-op stub - arc4random doesn't need additional entropy in modern implementations */
    (void)dat;
    (void)datlen;
}
