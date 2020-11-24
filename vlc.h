#ifndef JPEG_VLC_H
#define JPEG_VLC_H

#include <stddef.h>
#include <stdint.h>

/* represents Huffman code
 */
struct vlc {
	uint16_t code;
	size_t size;
};

int init_vlc(struct vlc *vlc);

int vlc_add_bit(struct vlc *vlc, uint16_t bit);

#endif
