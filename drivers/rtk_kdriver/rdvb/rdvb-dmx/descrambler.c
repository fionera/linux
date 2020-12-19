#include <asm/uaccess.h>
#include <dmx-ext.h>
#include <tp/tp_drv_api.h>
#include <rdvb-common/misc.h>
#include "descrambler.h"
int rdvb_dmx_descram_init(struct descrambler *dscmb, unsigned int idx)
{
	dscmb->idx = idx;
	dscmb->type = DMX_EXT_DSCRMB_TYPE_NONE;
	memset(dscmb->key_even_iv, 0xFF, KEY_SIZE_MAX);
	memset(dscmb->key_odd_iv, 0XFF, KEY_SIZE_MAX);
	return 0;
}
int rdvb_dmx_descram_type_set(struct descrambler *dscmb, enum dmx_ext_dscrmb_type type)
{
	TPK_DESCRAMBLE_MODE_T mode = TPK_TSP_LEVEL_DESCRAMBLE;
	TPK_DESCRAMBLE_ALGORITHM_T alg = {};
	dmx_notice(dscmb->idx, "%s_%d:  type:%d\n", __func__, __LINE__, type);
	switch (type) {
	case DMX_EXT_DSCRMB_TYPE_NONE:
		mode = TPK_NO_DESCRAMBLE;
		break;
	case DMX_EXT_DSCRMB_TYPE_BCAS:
		alg.algo = TPK_DESCRAMBLE_ALGO_MULTI2_OFB;
		alg.round = 32 ;
		break;
	case DMX_EXT_DSCRMB_TYPE_CI_AES:
		alg.algo = TPK_DESCRAMBLE_ALGO_AES_128_CBC_CLEAR;
		break;
	case DMX_EXT_DSCRMB_TYPE_CI_DES:
		alg.algo = TPK_DESCRAMBLE_ALGO_DES_ECB_CLEAR;
		break;
	case DMX_EXT_DSCRMB_TYPE_PVR:
		alg.algo = TPK_DESCRAMBLE_ALGO_AES_128_CBC_CLEAR;
		break;
	default:
		dmx_err(dscmb->idx, "%s_%d :ERROR type:%d\n", __func__, __LINE__, type);
		return -1;
	}
	if (RHAL_SetDescrambleAlgorithm(dscmb->idx, alg) != TPK_SUCCESS) {
		dmx_err(dscmb->idx, "%s_%d :ERROR type:%d\n", __func__, __LINE__, type);
		return -1;
	}
	if (RHAL_DescrambleControl(dscmb->idx, mode) != TPK_SUCCESS) {
		dmx_err(dscmb->idx, "%s_%d :ERROR type:%d\n", __func__, __LINE__, type);
		return -1;
	}
	dscmb->type = type;
	return 0;
}
int rdvb_dmx_descram_type_get(struct descrambler *dscmb, enum dmx_ext_dscrmb_type *type)
{
	*type = dscmb->type;
	return 0;
}
int rdvb_dmx_descram_key_set(struct descrambler *dscmb, struct dmx_ext_dscrmb_key *key)
{
	unsigned char key_index = dscmb->idx;
	unsigned char key_id = 0;
	unsigned char *key_iv = (void *)-1;
	unsigned char *pkey = (void *)-1;
	u8 __key[128];
	if (key->key_size == 0) {
		dmx_err(dscmb->idx, "ERROR: key_index:%d, keytype:%d\n", key_index, key->key_type);
		return -1;
	}
	if (copy_from_user(__key, key->key, key->key_size)) {
		dmx_err(dscmb->idx, "ERROR: key_index:%d, keytype:%d\n", key_index, key->key_type);
		return -1;
	}
	dmx_mask_print(DMX_DESCRAMBLER_DBG,dscmb->idx,
			"key_index:%d, keytype:%d keysize:%d, \nkey[%*ph]\n",
				key_index, key->key_type, key->key_size, key->key_size, __key);
	switch (key->key_type) {
	case DMX_EXT_DSCRMB_KEY_TYPE_EVEN:
		memcpy(dscmb->key_even, __key, key->key_size);
		dmx_mask_print(DEMUX_NOTICE_PRINT,dscmb->idx,
				"key_index:%d, keytype:%d(even) keysize:%d, key[%*ph]\n",
					key_index, key->key_type, key->key_size, 4, dscmb->key_even);
		key_id = TP_SETCW_KEYID_EVEN_KEY;
		key_iv = dscmb->key_even_iv;
		pkey = dscmb->key_even;
		break;
	case DMX_EXT_DSCRMB_KEY_TYPE_ODD:
		memcpy(dscmb->key_odd, __key, key->key_size);
		dmx_mask_print(DEMUX_NOTICE_PRINT,dscmb->idx,
				"key_index:%d, keytype:%d(odd) keysize:%d, key[%*ph]\n",
					key_index, key->key_type, key->key_size, 4, dscmb->key_odd);
		key_id = TP_SETCW_KEYID_ODD_KEY;
		key_iv = dscmb->key_odd_iv;
		pkey = dscmb->key_odd;
		break;
	case DMX_EXT_DSCRMB_KEY_TYPE_EVEN_ODD:
		//need to check
		memcpy(dscmb->key_odd, __key, key->key_size/2);
		memcpy(dscmb->key_even, &__key[key->key_size/2], key->key_size/2);
		dmx_mask_print(DEMUX_NOTICE_PRINT,dscmb->idx,
				"key_index:%d, keytype:%d(odd_even) keysize:%d, key[%*ph] key[%*ph]\n",
					key_index, key->key_type, key->key_size, 4, dscmb->key_odd, 4, dscmb->key_even);
		key_id = TP_SETCW_KEYID_ODD_KEY;
		key_iv = dscmb->key_odd_iv;
		pkey = dscmb->key_odd;
		break;
	case DMX_EXT_DSCRMB_KEY_TYPE_EVENIV:
		memcpy(dscmb->key_even_iv, __key, key->key_size);
		key_id = TP_SETCW_KEYID_EVEN_KEY;
		key_iv = dscmb->key_even_iv;
		pkey = dscmb->key_even;
		break;
	case DMX_EXT_DSCRMB_KEY_TYPE_ODDIV:
		memcpy(dscmb->key_odd_iv, __key, key->key_size);
		key_id = TP_SETCW_KEYID_ODD_KEY;
		key_iv = dscmb->key_odd_iv;
		pkey = dscmb->key_odd;;
		break;
	case DMX_EXT_DSCRMB_KEY_TYPE_CBC:
		memcpy(dscmb->key_even_iv, __key, key->key_size);
		memcpy(dscmb->key_odd_iv, __key, key->key_size);
		return 0;
	case DMX_EXT_DSCRMB_KEY_TYPE_SYSKEY:
		key_id = TP_SETCW_KEYID_MULTI2_SYS_KEY;
		pkey = __key;
		break;
	case DMX_EXT_DSCRMB_KEY_TYPE_PVRKEY:
	default:
		dmx_err(dscmb->idx, "ERROR: key_index:%d, keytype:%d\n", key_index, key->key_type);
		return -1;
	}
	if (RHAL_SetCW(dscmb->idx, key_index, key_id, pkey, key_iv) != TPK_SUCCESS) {
		dmx_err(dscmb->idx, "ERROR: key_index:%d, keytype:%d\n", key_index, key->key_type);
		return -1;
	}
	if (key->key_type == DMX_EXT_DSCRMB_KEY_TYPE_EVEN_ODD) {
		key_id = TP_SETCW_KEYID_EVEN_KEY;
		key_iv = dscmb->key_even_iv;
		pkey = dscmb->key_even;
		if (RHAL_SetCW(dscmb->idx, key_index, key_id, pkey, key_iv) != TPK_SUCCESS) {
			dmx_err(dscmb->idx, "ERROR: key_index:%d, keytype:%d\n", key_index, key->key_type);
			return -1;
		}
	}
	return 0;
}

