

#include "mii.h"

// https://github.com/ivanizag/izapple2/blob/master/cardMouse.go
// https://hackaday.io/project/19925-aiie-an-embedded-apple-e-emulator/log/188017-entry-23-here-mousie-mousie-mousie
// https://github.com/ct6502/apple2ts/blob/main/src/emulator/mouse.ts

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mii.h"
#include "mii_bank.h"


enum {
	CLAMP_MIN_LO    = 0x478,
	CLAMP_MIN_HI    = 0x578,
	CLAMP_MAX_LO    = 0x4F8,
	CLAMP_MAX_HI    = 0x5F8,

	// RAM Locations
	// (Add $Cn where n is slot to these)
	MOUSE_X_LO      = 0x03B8,
	MOUSE_X_HI      = 0x04B8,
	MOUSE_Y_LO      = 0x0438,
	MOUSE_Y_HI      = 0x0538,
	MOUSE_STATUS    = 0x06B8,
	MOUSE_MODE      = 0x0738,
};

enum {
	mouseEnabled          = 1,
	mouseIntMoveEnabled   = 2,
	mouseIntButtonEnabled = 4,
	mouseIntVBlankEnabled = 8,
};

typedef struct mii_card_mouse_t {
	struct mii_slot_t *	slot;
	uint8_t 			slot_offset;
	uint8_t				mode; // cached mode byte
	struct {
		uint16_t 			x, y;
		bool 				button;
	}					last;
} mii_card_mouse_t;

static int
_mii_mouse_init(
		mii_t * mii,
		struct mii_slot_t *slot )
{
	mii_card_mouse_t *c = calloc(1, sizeof(*c));
	c->slot = slot;
	slot->drv_priv = c;

	printf("%s loading in slot %d\n", __func__, slot->id + 1);

	c->slot_offset = slot->id + 1 + 0xc0;
	uint8_t data[256] = {};

	// Identification as a mouse card
	// From Technical Note Misc #8, "Pascal 1.1 Firmware Protocol ID Bytes":
	data[0x05] = 0x38;
	data[0x07] = 0x18;
	data[0x0b] = 0x01;
	data[0x0c] = 0x20;
	// From "AppleMouse // User's Manual", Appendix B:
	//data[0x0c] = 0x20
	data[0xfb] = 0xd6;

	// Set 8 entrypoints to sofstwitches 2 to 1f
	for (int i = 0; i < 14; i++) {
		uint8_t base = 0x60 + 0x05 * i;
		data[0x12+i] = base;
		data[base+0] = 0x8D; // STA $C0x2
		data[base+1] = 0x82 + i + ((slot->id + 1) << 4);
		data[base+2] = 0xC0;
		data[base+3] = 0x18; // CLC ;no error
		data[base+4] = 0x60; // RTS
	}

	uint16_t addr = 0xc100 + (slot->id * 0x100);
	mii_bank_write(
			&mii->bank[MII_BANK_CARD_ROM],
			addr, data, 256);

	return 0;
}

static uint8_t
_mii_mouse_access(
		mii_t * mii,
		struct mii_slot_t *slot,
		uint16_t addr,
		uint8_t byte,
		bool write)
{
	mii_card_mouse_t *c = slot->drv_priv;

	int psw = addr & 0x0F;
	mii_bank_t * main = &mii->bank[MII_BANK_MAIN];
	switch (psw) {
		case 2: {
			if (write) {
				byte &= 0xf;
				mii_bank_poke(main, MOUSE_MODE + c->slot_offset, byte);
				printf("%s: mouse mode %02x\n", __func__, byte);
				mii->mouse.enabled = byte & mouseEnabled;
				printf("Mouse %s\n", mii->mouse.enabled ? "enabled" : "disabled");
				printf(" Interupt: %s\n", byte & mouseIntMoveEnabled ? "enabled" : "disabled");
				printf(" Button:   %s\n", byte & mouseIntButtonEnabled ? "enabled" : "disabled");
				printf(" VBlank:   %s\n", byte & mouseIntVBlankEnabled ? "enabled" : "disabled");
				c->mode = byte;
				mii->video.vbl_irq = !!(byte & mouseIntVBlankEnabled);
			}
		}	break;
		case 4: {// read mouse
			if (!mii->mouse.enabled)
				break;
			uint8_t status = 0;
			if (mii->mouse.button)
				status |= 1 << 7;
			if (c->last.button) {
				status |= 1 << 6;
			}
			if ((mii->mouse.x != c->last.x) || (mii->mouse.y != c->last.y)) {
				status |= 1 << 5;
			}
			mii_bank_poke(main, MOUSE_X_HI + c->slot_offset, mii->mouse.x >> 8);
			mii_bank_poke(main, MOUSE_Y_HI + c->slot_offset, mii->mouse.y >> 8);
			mii_bank_poke(main, MOUSE_X_LO + c->slot_offset, mii->mouse.x);
			mii_bank_poke(main, MOUSE_Y_LO + c->slot_offset, mii->mouse.y);
			mii_bank_poke(main, MOUSE_STATUS + c->slot_offset, status);
		//	mii_bank_poke(main, MOUSE_MODE + c->slot_offset, 0xf); // already in place
			c->last.x = mii->mouse.x;
			c->last.y = mii->mouse.y;
			c->last.button = mii->mouse.button;
		}	break;
		case 5: // clear mouse
			break;
		case 7: // set mouse
			if (byte == 0) {
				mii->mouse.min_x = mii_bank_peek(main, CLAMP_MIN_LO) |
									(mii_bank_peek(main, CLAMP_MIN_HI) << 8);
				mii->mouse.max_x = mii_bank_peek(main, CLAMP_MAX_LO) |
									(mii_bank_peek(main, CLAMP_MAX_HI) << 8);
			} else if (byte == 1) {
				mii->mouse.min_y = mii_bank_peek(main, CLAMP_MIN_LO) |
									(mii_bank_peek(main, CLAMP_MIN_HI) << 8);
				mii->mouse.max_y = mii_bank_peek(main, CLAMP_MAX_LO) |
									(mii_bank_peek(main, CLAMP_MAX_HI) << 8);
			}
			printf("Mouse clamp to %d,%d - %d,%d\n",
					mii->mouse.min_x, mii->mouse.min_y,
					mii->mouse.max_x, mii->mouse.max_y);
			break;
		case 8: // home mouse
			mii->mouse.x = mii->mouse.min_x;
			mii->mouse.y = mii->mouse.min_y;
			break;
		case 0xc: // init mouse
			mii->mouse.min_x = mii->mouse.min_y = 0;
			mii->mouse.max_x = mii->mouse.max_y = 1023;
			mii->mouse.enabled = 0;
			mii_bank_poke(main, MOUSE_MODE + c->slot_offset, 0x0);
			break;
		default:
			printf("%s PC:%04x addr %04x %02x wr:%d\n", __func__,
					mii->cpu.PC, addr, byte, write);
			break;
	}
	return 0;
}

static mii_slot_drv_t _driver = {
	.name = "mouse",
	.desc = "Mouse card",
	.init = _mii_mouse_init,
	.access = _mii_mouse_access,
};
MI_DRIVER_REGISTER(_driver);
