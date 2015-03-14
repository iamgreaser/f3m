// made in 2015 by GreaseMonkey - Public Domain
#include <stdint.h>

#include "gba.h"

#define TARGET_GBA
#define TARGET_GBA_DEBUG
#include "f3m.c"

static int32_t mbuf[F3M_BUFLEN*F3M_CHNS];
static uint8_t obuf[3][F3M_BUFLEN*F3M_CHNS];
static player_s player;

static void isr_handler(void)
{
	//asm("mov pc, lr;\n");
	uint16_t isr_state = IF;

	if(isr_state & 0x0001)
	{
		IF = 0x0001;
		ISR_flags |= 0x0001;
		// Do vblank stuff
	}

	if(isr_state & 0x0010)
	{
		IF = 0x0010;
		ISR_flags |= 0x0010;
	}

	//asm("mov r0, #0x08000000 ; bx r0 ;\n");
}

static void wait_timer()
{
	while(!(ISR_flags & 0x0010)) {}
	ISR_flags &= ~0x0010;
}

void _start(void)
{
	int i;

	//WAITCNT = 0x431B;

	DISPCNT = 0x404;
	
	IME = 0;
	ISR_funct = isr_handler;
	IE = 0x0001;
	DISPSTAT |= 0x0008;
	IME = 1;

	// Set up audio

	mod_s *mod = *(mod_s **)0x080000C0;
	//uint32_t *src_s3m = *(uint32_t **)0x080000C0;
	//uint32_t *dst_s3m = (uint32_t *)0x02000000;
	//uint32_t *dst_s3m = (uint32_t *)0x03001800;
	//for(i = 0; i < (24*1024)/4; i++)
	//	dst_s3m[i] = src_s3m[i];
	//mod_s *mod = (mod_s *)dst_s3m;
	//mod_s *mod = (mod_s *)src_s3m;
	f3m_player_init(&player, mod);
	f3m_player_play(&player, mbuf, obuf[0]);
	f3m_player_play(&player, mbuf, obuf[1]);

	// Init sound properly
	SOUNDCNT_X = (1<<7);
	SOUNDCNT_L = (7<<0) | (7<<4);
	SOUNDCNT_H = (2<<0) | (1<<2) | (1<<11) | (3<<8);

	TM0CNT_H = 0;
	TM1CNT_H = 0;

	DMA1SAD = obuf[0];
	DMA1DAD = &FIFO_A;
	DMA1CNT_H = 0xB600;

	TM1CNT_L = 0x10000-64;
	TM1CNT_H = 0x00C4;

	TM0CNT_L = 0x10000-((1<<24)/F3M_FREQ);
	TM0CNT_H = 0x0080;

	for(;;)
	{
		wait_timer();
		f3m_player_play(&player, mbuf, obuf[0]);
		// copy a bit to the end
		((uint32_t *)(obuf[2]))[0] = ((uint32_t *)(obuf[0]))[0];
		((uint32_t *)(obuf[2]))[1] = ((uint32_t *)(obuf[0]))[1];
		((uint32_t *)(obuf[2]))[2] = ((uint32_t *)(obuf[0]))[2];
		((uint32_t *)(obuf[2]))[3] = ((uint32_t *)(obuf[0]))[3];
		wait_timer();
		DMA1CNT_H = 0x0000;
		DMA1SAD = obuf[0];
		DMA1DAD = &FIFO_A;
		DMA1CNT_H = 0xB600;
		f3m_player_play(&player, mbuf, obuf[1]);
	}

	for(;;) {}
}

