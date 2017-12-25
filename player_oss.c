// made in 2015 by GreaseMonkey - Public Domain
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#include <assert.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

#define F3M_ENABLE_DYNALOAD
#define F3M_FREQ   48000
#define F3M_BUFLEN (48000/500)
#include "f3m.c"

static int32_t mbuf[F3M_BUFLEN*F3M_CHNS];
static uint8_t obuf[F3M_BUFLEN*F3M_CHNS];

int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	int dsp = open("/dev/dsp", O_WRONLY);
	int freq = F3M_FREQ;
	int fmt = AFMT_U8;
	int chns = F3M_CHNS;

	if(dsp == -1)
	{
		perror("open /dev/dsp");
		return 1;
	}

	if(ioctl(dsp, SNDCTL_DSP_SPEED, &freq) < 0)
	{
		perror("ioctl(SNDCTL_DSP_SPEED)");
		return 1;
	}

	if(ioctl(dsp, SNDCTL_DSP_SETFMT, &fmt) < 0)
	{
		perror("ioctl(SNDCTL_DSP_SETFMT)");
		return 1;
	}

	if(ioctl(dsp, SNDCTL_DSP_CHANNELS, &chns) < 0)
	{
		perror("ioctl(SNDCTL_DSP_CHANNELS)");
		return 1;
	}

	printf("freq=%i chns=%i fmt=%08X\n", freq, chns, fmt);

	assert(argc > 1);
	mod_s *mod = f3m_mod_dynaload_filename(argv[1]);
	player_s player;
	f3m_player_init(&player, mod);

	for(;;)
	{
		f3m_player_play(&player, mbuf, obuf);
		write(dsp, obuf, F3M_BUFLEN*F3M_CHNS);
	}

	f3m_mod_free(mod);

	return 0;
}

