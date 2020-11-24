#include <assert.h>
#include "vlc.h"
#include "common.h"

int init_vlc(struct vlc *vlc)
{
	assert(vlc != NULL);
	
	vlc->code = 0;
	vlc->size = 0;

	return RET_SUCCESS;
}

int vlc_add_bit(struct vlc *vlc, uint16_t bit)
{
	assert(vlc != NULL);

	vlc->code <<= 1;
	vlc->code |= bit & 1;
	vlc->size++;

	return RET_SUCCESS;
}
