#ifndef __RDVB_DESCRAMBLER_H_
#define __RDVB_DESCRAMBLER_H_

#include <dmx-ext.h>
#define KEY_SIZE_MAX 64
struct descrambler{
	int idx;
	enum dmx_ext_dscrmb_type type;
	unsigned char key_even_iv[KEY_SIZE_MAX];
	unsigned char key_odd_iv[KEY_SIZE_MAX];
	unsigned char key_even[KEY_SIZE_MAX];
	unsigned char key_odd[KEY_SIZE_MAX];
};
int rdvb_dmx_descram_init(struct descrambler *dscmb, unsigned int idx);
int rdvb_dmx_descram_type_get(struct descrambler *dscmb, enum dmx_ext_dscrmb_type *type);
int rdvb_dmx_descram_type_set(struct descrambler *dscmb, enum dmx_ext_dscrmb_type type);
int rdvb_dmx_descram_key_set(struct descrambler *dscmb, struct dmx_ext_dscrmb_key *key);
#endif