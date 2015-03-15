// made in 2015 by GreaseMonkey - Public Domain
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#ifdef TARGET_GBA
#define assert(...)
#else
#include <assert.h>
#endif

#ifndef F3M_FREQ
#define F3M_FREQ 32768
//define F3M_FREQ 16384
#endif
#ifndef F3M_BUFLEN
#define F3M_BUFLEN 546
//define F3M_BUFLEN 273
#endif
#ifndef F3M_CHNS
#define F3M_CHNS 1
#endif

#ifdef TARGET_GBA

#if 0
void __assert_func(const char *a, int b, const char *c, const char *d)
{
	for(;;);
}
#endif

// for -O3
#if 0
void *memset(void *b, int c, size_t len)
{
	size_t i;
	unsigned char rc = (unsigned char)c;
	uint8_t *bb = (uint8_t *)b;
	uint32_t cw = ((uint32_t)rc) * 0x01010101;

	for(i = 0; i < len-3; i += 4)
		*(uint32_t *)(bb+i) = cw;
	for(; i < len; i++)
		bb[i] = rc;
}
#endif
#endif

typedef struct ins
{
	uint8_t typ;
	uint8_t fname[12];
	uint8_t dat_para_h;
	uint16_t dat_para;
	uint32_t len, lpbeg, lpend;
	uint8_t vol, rsv1, pack, flags;
	uint32_t c4freq;
	uint8_t rsv2[12];
	uint8_t name[28];
	uint8_t magic[4];
} __attribute__((__packed__)) ins_s;

typedef struct mod
{
	uint8_t name[28];
	uint8_t magic[4];
	uint16_t ord_num, ins_num, pat_num;
	uint16_t flags, ver, smptyp;
	uint8_t magic_scrm[4];
	uint8_t gvol, ispeed, itempo, mvol;
	uint8_t uclick, defpanFC;
	uint8_t unused1[8];
	uint16_t special;
	uint8_t cset[32];
	uint8_t extra[];
}__attribute__((__packed__)) mod_s;

typedef struct vchn
{
	const uint8_t *data;
	int32_t len;
	int32_t len_loop;

	int32_t freq;
	int32_t offs;
	uint16_t suboffs;

	int32_t period;
	int32_t gxx_period;

	int8_t vol;

	uint8_t eft, efp, lefp;
	uint8_t lins;
	uint8_t mem_gxx, mem_hxx, mem_oxx;
} vchn_s;

typedef struct player
{
	const mod_s *mod;
	const void *modbase;
	const uint16_t *ins_para;
	const uint16_t *pat_para;
	const uint8_t *ord_list;

	int32_t speed, tempo;
	int32_t ctick, tempo_samples, tempo_wait;
	int32_t cord, cpat, crow;
	const uint8_t *patptr;

	int ccount;

	vchn_s vchn[16];
} player_s;

const uint32_t period_amiclk = 8363*1712-400;
const uint16_t period_table[12] = {1712,1616,1524,1440,1356,1280,1208,1140,1076,1016,960,907};

#ifdef F3M_ENABLE_DYNALOAD
mod_s *f3m_mod_dynaload_filename(const char *fname)
{
	int buf_max = 256;
	int buf_len = 0;

	FILE *fp = fopen(fname, "rb");
	assert(fp != NULL);
	char *buf = malloc(buf_max);

	for(;;)
	{
		if(buf_len >= buf_max)
		{
			buf_max = (buf_max*5/4+2);

			if(buf_len >= buf_max)
				buf_max = buf_len + 10;

			buf = realloc(buf, buf_max);
		}

		int v = fgetc(fp);
		if(v < 0) break;
		buf[buf_len++] = v;
	}

	fclose(fp);
	buf = realloc(buf, buf_len);
	return (mod_s *)buf;
}

void f3m_mod_free(mod_s *mod)
{
	free(mod);
}
#endif

static int32_t f3m_calc_tempo_samples(int32_t tempo)
{
	return (F3M_FREQ*10)/(tempo*4);
}

static int32_t f3m_calc_period_freq(int32_t period)
{
	int32_t freq = period_amiclk / period;
#if F3M_FREQ == 32768
	freq <<= 1;
#else
#if F3M_FREQ == 16384
	freq <<= 2;
#else
	freq = (freq << 10) / (F3M_FREQ >> 6);
#endif
#endif
	return freq;
}

void f3m_player_init(player_s *player, mod_s *mod)
{
	int i;

	player->mod = mod;
	player->modbase = (const void *)mod;
	player->ord_list = (const uint8_t *)(((const char *)(mod+1)));
	player->ins_para = (const uint16_t *)(((const char *)(mod+1)) + mod->ord_num);
	player->pat_para = (const uint16_t *)(((const char *)(mod+1)) + mod->ord_num + mod->ins_num*2);

	player->speed = mod->ispeed;
	player->tempo = mod->itempo;
	player->ctick = player->speed;
	player->tempo_samples = f3m_calc_tempo_samples(player->tempo);
	player->tempo_wait = 0;

	player->cord = 0-1;
	player->cpat = 0;
	player->crow = 64;
	player->patptr = NULL;

	for(i = 0; i < 16; i++)
	{
		vchn_s *vchn = &(player->vchn[i]);

		vchn->data = NULL;
		vchn->len = 0;
		vchn->len_loop = 0;

		vchn->freq = 0;
		vchn->offs = 0;
		vchn->suboffs = 0;

		vchn->period = 0;
		vchn->vol = 0;

		vchn->eft = 0;
		vchn->efp = 0;
		vchn->lefp = 0;
		vchn->lins = 0;

		vchn->mem_gxx = 0;
		vchn->mem_hxx = 0;
		vchn->mem_oxx = 0;
	}
}

static void f3m_player_eff_slide_vol(player_s *player, vchn_s *vchn, int isfirst)
{
	(void)player; // "player" is only there to check the fast slide flag (TODO!)

	uint8_t lefp = vchn->lefp;
	int samt = 0;

	if((lefp & 0xF0) == 0x00)
	{
		if((!isfirst) || lefp == 0x0F) samt = -(lefp & 0x0F);
	} else if((lefp & 0x0F) == 0x00) {
		if((!isfirst) || lefp == 0xF0) samt = lefp >> 4;
	} else if((lefp & 0x0F) == 0x0F) {
		if(isfirst) samt = lefp >> 4;
	} else if((lefp & 0xF0) == 0xF0) {
		if(isfirst) samt = -(lefp & 0x0F);
	}

	if(samt > 0)
	{
		vchn->vol += samt;
		if(vchn->vol > 63) vchn->vol = 63;
	} else if(samt < 0) {
		if(vchn->vol < (uint8_t)-samt) vchn->vol = 0;
		else vchn->vol += samt;
	}
}

static void f3m_player_eff_slide_period(vchn_s *vchn, int amt)
{
	vchn->period += amt;
	vchn->freq = f3m_calc_period_freq(vchn->period);
}

void f3m_effect_nop(player_s *player, vchn_s *vchn, int tick, int pefp, int lefp)
{
	(void)player; (void)vchn; (void)tick; (void)pefp; (void)lefp;
}

void f3m_effect_Axx(player_s *player, vchn_s *vchn, int tick, int pefp, int lefp)
{
	(void)player; (void)vchn; (void)tick; (void)pefp; (void)lefp;

	if(tick == 0 && pefp >= 1)
		player->speed = pefp;
}

void f3m_effect_Cxx(player_s *player, vchn_s *vchn, int tick, int pefp, int lefp)
{
	(void)player; (void)vchn; (void)tick; (void)pefp; (void)lefp;

	if(tick == 0)
	{
		// TODO: actually look up the jump value
		player->crow = 64; 
	}
}

void f3m_effect_Dxx(player_s *player, vchn_s *vchn, int tick, int pefp, int lefp)
{
	(void)player; (void)vchn; (void)tick; (void)pefp; (void)lefp;

	f3m_player_eff_slide_vol(player, vchn, tick == 0);
}

void f3m_effect_Exx(player_s *player, vchn_s *vchn, int tick, int pefp, int lefp)
{
	(void)player; (void)vchn; (void)tick; (void)pefp; (void)lefp;

	if(tick == 0)
	{
		if(lefp >= 0xF0)
		{
			f3m_player_eff_slide_period(vchn, ((lefp & 0x0F)<<2));
		} else if(lefp >= 0xE0) {
			f3m_player_eff_slide_period(vchn, (lefp & 0x0F));
		}
	} else {
		if(lefp < 0xE0)
		{
			f3m_player_eff_slide_period(vchn, (lefp<<2));
		}
	}
}

void f3m_effect_Fxx(player_s *player, vchn_s *vchn, int tick, int pefp, int lefp)
{
	(void)player; (void)vchn; (void)tick; (void)pefp; (void)lefp;

	if(tick == 0)
	{
		if(lefp >= 0xF0)
		{
			f3m_player_eff_slide_period(vchn, -((lefp & 0x0F)<<2));
		} else if(lefp >= 0xE0) {
			f3m_player_eff_slide_period(vchn, -(lefp & 0x0F));
		}
	} else {
		if(lefp < 0xE0)
		{
			f3m_player_eff_slide_period(vchn, -(lefp<<2));
		}
	}
}

void f3m_effect_Gxx(player_s *player, vchn_s *vchn, int tick, int pefp, int lefp)
{
	(void)player; (void)vchn; (void)tick; (void)pefp; (void)lefp;

	if(tick == 0)
	{
		lefp = (pefp != 0 ? pefp : vchn->mem_gxx);
		vchn->mem_gxx = lefp;
	} else {
		lefp = vchn->mem_gxx;

		if(vchn->period < vchn->gxx_period)
		{
			vchn->period += lefp<<2;
			if(vchn->period > vchn->gxx_period)
				vchn->period = vchn->gxx_period;
			vchn->freq = f3m_calc_period_freq(vchn->period);

		} else if(vchn->period > vchn->gxx_period) {
			vchn->period -= lefp<<2;
			if(vchn->period < vchn->gxx_period)
				vchn->period = vchn->gxx_period;
			vchn->freq = f3m_calc_period_freq(vchn->period);
		}
	}
}

void f3m_effect_Kxx(player_s *player, vchn_s *vchn, int tick, int pefp, int lefp)
{
	(void)player; (void)vchn; (void)tick; (void)pefp; (void)lefp;

	if(tick != 0)
		f3m_player_eff_slide_vol(player, vchn, tick == 0);
}

void f3m_effect_Lxx(player_s *player, vchn_s *vchn, int tick, int pefp, int lefp)
{
	(void)player; (void)vchn; (void)tick; (void)pefp; (void)lefp;

	if(tick != 0)
	{
		f3m_effect_Gxx(player, vchn, tick, pefp, lefp);
		f3m_effect_Dxx(player, vchn, tick, pefp, lefp);
	}
}

void f3m_effect_Oxx(player_s *player, vchn_s *vchn, int tick, int pefp, int lefp)
{
	(void)player; (void)vchn; (void)tick; (void)pefp; (void)lefp;

	if(tick == 0)
	{
		//if(retrig && vchn->data != NULL && (lefp<<8) < vchn->len)

		// TODO: get info as to whether note retriggered or not
		// (if it's even relevant?)
		lefp = (pefp != 0 ? pefp : vchn->mem_oxx);
		if(vchn->data != NULL && (lefp<<8) < vchn->len)
			vchn->offs = lefp<<8;
	}
}

void f3m_effect_Sxx(player_s *player, vchn_s *vchn, int tick, int pefp, int lefp)
{
	(void)player; (void)vchn; (void)tick; (void)pefp; (void)lefp;

	switch(lefp>>4)
	{
		case 0xC:
			// TODO confirm SC0 behaviour
			if(tick != 0 && (lefp&0x0F) == tick)
			{
				vchn->data = NULL;
				vchn->vol = 0;
			}
			break;
	}
}

void (*(f3m_effect_tab[32]))(player_s *player, vchn_s *vchn, int tick, int pefp, int lefp) = {
	f3m_effect_nop, f3m_effect_Axx, f3m_effect_nop, f3m_effect_Cxx,
	f3m_effect_Dxx, f3m_effect_Exx, f3m_effect_Fxx, f3m_effect_Gxx,
	f3m_effect_nop, f3m_effect_nop, f3m_effect_nop, f3m_effect_Kxx,
	f3m_effect_Lxx, f3m_effect_nop, f3m_effect_nop, f3m_effect_Oxx,

	f3m_effect_nop, f3m_effect_nop, f3m_effect_nop, f3m_effect_Sxx,
	f3m_effect_nop, f3m_effect_nop, f3m_effect_nop, f3m_effect_nop,
	f3m_effect_nop, f3m_effect_nop, f3m_effect_nop, f3m_effect_nop,
	f3m_effect_nop, f3m_effect_nop, f3m_effect_nop, f3m_effect_nop,
};

static void f3m_player_play_newnote(player_s *player)
{
	int i;

	// Advance row
	player->crow++;
	if(player->crow >= 64)
	{
		player->crow = 0;

		// Advance order
		player->cord++;
		if(player->cord >= player->mod->ord_num || player->ord_list[player->cord] == 0xFF)
			player->cord = 0;

		player->cpat = player->ord_list[player->cord];
		assert(player->cpat < 200);
		assert(player->cpat < player->mod->pat_num);

		// Get new pattern pointer
		player->patptr = player->modbase + (((uint32_t)(player->pat_para[player->cpat]))*16);
		player->patptr += 2;
	}

	// Clear vchn pattern data
	for(i = 0; i < 16; i++)
	{
		vchn_s *vchn = &(player->vchn[i]);
		vchn->eft = 0x00;
		vchn->efp = 0x00;
	}

	// Read pattern data
	if(player->patptr == NULL)
		return;

	const uint8_t *p = player->patptr;

	for(;;)
	{
		uint8_t cv = *(p++);
		if(cv == 0) break;
		vchn_s *vchn = &(player->vchn[cv&15]); // TODO proper channel map check?

		uint8_t pnote = 0xFD;
		uint8_t pins = 0x00;
		uint8_t pvol = 0xFF;
		uint8_t peft = 0x00;
		uint8_t pefp = 0x00;

		int retrig = 0;

		if((cv & 0x20) != 0)
		{
			pnote = *(p++);
			pins = *(p++);
		}

		if((cv & 0x40) != 0)
		{
			pvol = *(p++);
		}

		if((cv & 0x80) != 0)
		{
			peft = *(p++);
			pefp = *(p++);
		}

		// TODO: DO THIS PROPERLY
		if(pnote == 0xFE)
		{
			vchn->data = NULL;

		} else if(pnote < 0x80 && (pins != 0 || vchn->lins != 0)) {
			int iidx = (pins == 0 ? vchn->lins : pins);
			vchn->lins = iidx;
			const ins_s *ins = player->modbase + (((uint32_t)(player->ins_para[iidx-1]))*16);
			uint32_t para = (((uint32_t)(ins->dat_para_h))<<16)|((uint32_t)(ins->dat_para));

			vchn->vol = (pvol != 0xFF ? pvol
				: pins != 0 ? ins->vol
				: vchn->vol);
			if(vchn->vol > 63) vchn->vol = 63; // lesser-known quirk

			vchn->gxx_period = ((8363 * 16 * period_table[pnote&15]) / ins->c4freq)
				>> (pnote>>4);
			// TODO: verify if this is the case wrt note-end
			if(vchn->data == NULL || (peft != ('G'-'A'+1) && peft != ('L'-'A'+1)))
			{
				vchn->period = vchn->gxx_period;
				vchn->freq = f3m_calc_period_freq(vchn->period);
				vchn->offs = 0;
				retrig = 1;
			}

			vchn->data = player->modbase + para*16;
			vchn->len = (((ins->flags & 0x01) != 0) && ins->lpend < ins->len
				? ins->lpend
				: ins->len);
			vchn->len_loop = (((ins->flags & 0x01) != 0) && ins->lpbeg < ins->len
				? vchn->len - ins->lpbeg
				: 0);
		}

		if(pvol < 0x80)
		{
			vchn->vol = pvol;
			if(vchn->vol > 63) vchn->vol = 63;
		}

		vchn->eft = peft;
		vchn->efp = pefp;
		if(pefp != 0)
			vchn->lefp = pefp;

		uint8_t lefp = vchn->lefp;
		f3m_effect_tab[peft&31](player, vchn, 0, pefp, lefp);
	}

	player->patptr = p;
}

void f3m_player_play_newtick(player_s *player)
{
	int i;

	player->ctick++;
	if(player->ctick >= player->speed)
	{
		player->ctick = 0;
		f3m_player_play_newnote(player);
	} else {
		for(i = 0; i < 16; i++)
		{
			vchn_s *vchn = &(player->vchn[i]);

			uint8_t peft = vchn->eft;
			uint8_t pefp = vchn->efp;
			uint8_t lefp = vchn->lefp;

			f3m_effect_tab[peft&31](player, vchn, player->ctick, pefp, lefp);
		}

	}
}

void f3m_player_play(player_s *player, int32_t *mbuf, uint8_t *obuf)
{
	int i, j;

	const int blen = F3M_BUFLEN;

#ifdef TARGET_GBA_DEBUG
	VPAL0[0] = 0x7FFF;
#endif

	// Check if we have a new tick
	while(player->tempo_wait < 0)
	{
		f3m_player_play_newtick(player);
		player->tempo_wait += player->tempo_samples;
	}

	player->tempo_wait -= blen;

	// Clear mixing buffer
	for(i = 0; i < blen*F3M_CHNS; i++)
		mbuf[i] = 0;
	
	// Do each channel
	for(i = 0; i < 16; i++)
	{
		vchn_s *vchn = &(player->vchn[i]);

		// Channel enabled?
		if(vchn->data == NULL)
			continue;

		// Output sample
		int32_t *out = mbuf;
		const uint8_t *d = vchn->data;
		int32_t offs = vchn->offs;
		int32_t suboffs = vchn->suboffs;
		int32_t len = vchn->len;
#if F3M_CHNS == 2
		int32_t lvol = vchn->vol;
		int32_t rvol = vchn->vol;
#else
		int32_t vol = vchn->vol;
#endif
		int32_t freq = vchn->freq;
		for(j = 0; j < blen;)
		{
			// Work out the time to the end of the sample
			int32_t rem_len = (len - offs);
			int32_t endsmp = blen;

			// Quick calculation to determine if we need to do a slow calculation
			if(rem_len < (((blen-j)*freq + 0x20000)>>16))
			{
				// We can't just sprint through,
				// so cut it off at the right point.

				// NOTE: ARM7TDMI (and anything pre-ARMv7)
				// lacks a divide instruction!
				// Thus, it calls a function instead.
				//
				// With that said, it's probably cheaper
				// than doing the length check every sample.
				// (Plus we don't do it every update.)

				int32_t rem_sublen = (rem_len<<16) - suboffs;
				endsmp = (rem_sublen+freq-1)/freq;
				endsmp += j;
			}

			// TODO: find optimal threshold
			if(freq >= 0x8000)
			{
				for(; j < endsmp; j++)
				{
#if F3M_CHNS == 2
					out[j*2+0] += (int32_t)(lvol*((int32_t)(d[offs])-0x80));
					out[j*2+1] += (int32_t)(rvol*((int32_t)(d[offs])-0x80));
#else
#if F3M_CHNS == 1
					out[j] += (int32_t)(vol*(((int32_t)d[offs])-0x80));
#else
#error "F3M_CHNS must be 1 or 2"
#endif
#endif
					suboffs += freq;
					offs += suboffs>>16;
					suboffs &= 0xFFFF;
				}
			} else { 
				int32_t loffs = offs-1;
				int32_t lval = 0;
				for(; j < endsmp; j++)
				{
#if F3M_CHNS == 2
					// TODO: port optimisations to stereo
					out[j*2+0] += (int32_t)(lvol*((int32_t)(d[offs])-0x80));
					out[j*2+1] += (int32_t)(rvol*((int32_t)(d[offs])-0x80));
#else
#if F3M_CHNS == 1
					if(loffs != offs)
					{
						lval = (int32_t)(vol*(((int32_t)d[offs])-0x80));
						loffs = offs;

					}
					out[j] += lval;
#else
#error "F3M_CHNS must be 1 or 2"
#endif
#endif
					suboffs += freq;
					offs += suboffs>>16;
					suboffs &= 0xFFFF;
				}
			}

			// Know of any clever ways to speed this up?
			// Memory timings (16/32):
			// - 1/1 Fast RAM
			// - 1/2 VRAM
			// - 3/6 Slow RAM
			// - 3/6 GamePak ROM - SEQUENTIAL
			// - 5/8 GamePak ROM - RANDOM ACCESS
			// Memory timings according to GBATEK:
			if(offs >= len)
			{
				if(vchn->len_loop == 0)
				{
					vchn->data = NULL;
					break;
				}

				offs -= vchn->len_loop;
			}
		}

		vchn->offs = offs;
		vchn->suboffs = suboffs;

	}

	for(i = 0; i < blen*F3M_CHNS; i++)
	{
		int32_t base = ((mbuf[i]+0x80)>>8)+0x80;
		if(base < 0x00) base = 0x00;
		if(base > 0xFF) base = 0xFF;
#ifdef TARGET_GBA
		obuf[i] = base ^ 0x80;
#else
		obuf[i] = base;
#endif
	}

#ifdef TARGET_GBA_DEBUG
	VPAL0[0] = 0x0000;
#endif
}

