// Glue so devkitPro's gba_crt0.s links against Ubuntu's stock newlib (instead of devkitARM's).
// crt0 stores the end of EWRAM here; only devkitARM's sbrk reads it. We don't use malloc.
char *fake_heap_end;
char *fake_heap_start;
