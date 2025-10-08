MEMORY {
	/* Old config with bootloader:
	   boot (rx) : ORIGIN = 0x8000000, LENGTH = 4K
	   cfg (rx) : ORIGIN = ORIGIN(boot) + LENGTH(boot), LENGTH = 1K
	   rom (rx) : ORIGIN = ORIGIN(cfg) + LENGTH(cfg), LENGTH = 64K - LENGTH(boot) - LENGTH(cfg) */
	
	/* New config: Code starts at flash base, cfg after code */
	boot (rx) : ORIGIN = 0x8000000, LENGTH = 0
	rom (rx)  : ORIGIN = 0x8000000, LENGTH = 59K
	cfg (rx)  : ORIGIN = 0x800ec00, LENGTH = 1K
	ram (rwx) : ORIGIN = 0x20000000, LENGTH = 8K
}
