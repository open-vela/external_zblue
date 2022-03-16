/*
 * Copyright (c) 2018-2021 mcumgr authors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cborattr/cborattr.h"
#include "tinycbor/cbor.h"
#include "tinycbor/cbor_buf_reader.h"

#include <zephyr.h>
#ifdef CONFIG_MGMT_CBORATTR_MAX_SIZE
#define CBORATTR_MAX_SIZE CONFIG_MGMT_CBORATTR_MAX_SIZE
#else
#define CBORATTR_MAX_SIZE 512
#endif

#ifdef CONFIG_MGMT_CBORATTR_FLOAT_SUPPORT
#define CBORATTR_FLOAT_SUPPORT	0
#else
#define CBORATTR_FLOAT_SUPPORT	1
#endif

/* this maps a CborType to a matching CborAtter Type. The mapping is not
 * one-to-one because of signedness of integers
 * and therefore we need a function to do this trickery
 */
static int
valid_attr_type(CborType ct, enum CborAttrType at)
{
	switch (at) {
	case CborAttrIntegerType:
	case CborAttrUnsignedIntegerType:
		if (ct == CborIntegerType) {
			return 1;
		}
		break;
	case CborAttrByteStringType:
		if (ct == CborByteStringType) {
			return 1;
		}
		break;
	case CborAttrTextStringType:
		if (ct == CborTextStringType) {
			return 1;
		}
		break;
	case CborAttrBooleanType:
		if (ct == CborBooleanType) {
			return 1;
		}
	break;
#if CBORATTR_FLOAT_SUPPORT != 0
	case CborAttrHalfFloatType:
		if (ct == CborHalfFloatType) {
			return 1;
		}
		break;
	case CborAttrFloatType:
		if (ct == CborFloatType) {
			return 1;
		}
		break;
	case CborAttrDoubleType:
		if (ct == CborDoubleType) {
			return 1;
		}
		break;
#endif
	case CborAttrArrayType:
		if (ct == CborArrayType) {
			return 1;
		}
		break;
	case CborAttrObjectType:
		if (ct == CborMapType) {
			return 1;
		}
		break;
	case CborAttrNullType:
		if (ct == CborNullType) {
			return 1;
		}
		break;
	default:
		break;
	}
	return 0;
}

/* This function find the pointer to the memory location to
 * write or read and attribute from the cbor_attr_r structure
 */
static char *
cbor_target_address(const struct cbor_attr_t *cursor,
			const struct cbor_array_t *parent, int offset)
{
	char *targetaddr = NULL;

	if (parent == NULL || parent->element_type != CborAttrStructObjectType) {
		/* ordinary case - use the address in the cursor structure */
		switch (cursor->type) {
		case CborAttrNullType:
			targetaddr = NULL;
			break;
		case CborAttrIntegerType:
			targetaddr = (char *)&cursor->addr.integer[offset];
			break;
		case CborAttrUnsignedIntegerType:
			targetaddr = (char *)&cursor->addr.uinteger[offset];
			break;
#if CBORATTR_FLOAT_SUPPORT != 0
		case CborAttrHalfFloatType:
			targetaddr = (char *)&cursor->addr.halffloat[offset];
			break;
		case CborAttrFloatType:
			targetaddr = (char *)&cursor->addr.fval[offset];
			break;
		case CborAttrDoubleType:
			targetaddr = (char *)&cursor->addr.real[offset];
			break;
#endif
		case CborAttrByteStringType:
			targetaddr = (char *) cursor->addr.bytestring.data;
			break;
		case CborAttrTextStringType:
			targetaddr = cursor->addr.string;
			break;
		case CborAttrBooleanType:
			targetaddr = (char *)&cursor->addr.boolean[offset];
			break;
		default:
			targetaddr = NULL;
			break;
		}
	} else {
		/* tricky case - hacking a member in an array of structures */
		targetaddr =
			parent->arr.objects.base + (offset * parent->arr.objects.stride) +
			cursor->addr.offset;
	}
	return targetaddr;
}

static int
cbor_internal_read_object(CborValue *root_value,
						  const struct cbor_attr_t *attrs,
						  const struct cbor_array_t *parent,
						  int offset)
{
	const struct cbor_attr_t *cursor, *best_match;
	char attrbuf[CBORATTR_MAX_SIZE + 1];
	void *lptr;
	CborValue cur_value;
	CborError err = 0;
	size_t len = 0;
	CborType type = CborInvalidType;

	/* stuff fields with defaults in case they're omitted in the JSON input */
	for (cursor = attrs; cursor->attribute != NULL; cursor++) {
		if (!cursor->nodefault) {
			lptr = cbor_target_address(cursor, parent, offset);
			if (lptr != NULL) {
				switch (cursor->type) {
				case CborAttrIntegerType:
					memcpy(lptr, &cursor->dflt.integer, sizeof(long long));
					break;
				case CborAttrUnsignedIntegerType:
					memcpy(lptr, &cursor->dflt.integer,
						   sizeof(unsigned long long));
					break;
				case CborAttrBooleanType:
					memcpy(lptr, &cursor->dflt.boolean, sizeof(bool));
					break;
#if CBORATTR_FLOAT_SUPPORT != 0
				case CborAttrHalfFloatType:
					memcpy(lptr, &cursor->dflt.halffloat, sizeof(uint16_t));
					break;
				case CborAttrFloatType:
					memcpy(lptr, &cursor->dflt.fval, sizeof(float));
					break;
				case CborAttrDoubleType:
					memcpy(lptr, &cursor->dflt.real, sizeof(double));
					break;
#endif
				default:
					break;
				}
			}
		}
	}

	if (cbor_value_is_map(root_value)) {
		err |= cbor_value_enter_container(root_value, &cur_value);
	} else {
		err |= CborErrorIllegalType;
		return err;
	}

	/* contains key value pairs */
	while (cbor_value_is_valid(&cur_value) && !err) {
		/* get the attribute */
		if (cbor_value_is_text_string(&cur_value)) {
			if (cbor_value_calculate_string_length(&cur_value, &len) == 0) {
				if (len > CBORATTR_MAX_SIZE) {
					err |= CborErrorDataTooLarge;
					break;
				}
				err |= cbor_value_copy_text_string(&cur_value, attrbuf, &len, NULL);
			}

			/* at least get the type of the next value so we can match the
			 * attribute name and type for a perfect match
			 */
			err |= cbor_value_advance(&cur_value);
			if (cbor_value_is_valid(&cur_value)) {
				type = cbor_value_get_type(&cur_value);
			} else {
				err |= CborErrorIllegalType;
				break;
			}
		} else {
			attrbuf[0] = '\0';
			type = cbor_value_get_type(&cur_value);
		}

		/* find this attribute in our list */
		best_match = NULL;
		for (cursor = attrs; cursor->attribute != NULL; cursor++) {
			if (valid_attr_type(type, cursor->type)) {
				if (cursor->attribute == CBORATTR_ATTR_UNNAMED &&
					attrbuf[0] == '\0') {
					best_match = cursor;
				} else if (strlen(cursor->attribute) == len &&
					!memcmp(cursor->attribute, attrbuf, len)) {
					break;
				}
			}
		}
		if (!cursor->attribute && best_match) {
			cursor = best_match;
		}
		/* we found a match */
		if (cursor->attribute != NULL) {
			lptr = cbor_target_address(cursor, parent, offset);
			switch (cursor->type) {
			case CborAttrNullType:
				/* nothing to do */
				break;
			case CborAttrBooleanType:
				err |= cbor_value_get_boolean(&cur_value, lptr);
				break;
			case CborAttrIntegerType:
				err |= cbor_value_get_int64(&cur_value, lptr);
				break;
			case CborAttrUnsignedIntegerType:
				err |= cbor_value_get_uint64(&cur_value, lptr);
				break;
#if CBORATTR_FLOAT_SUPPORT != 0
			case CborAttrHalfFloatType:
				err |= cbor_value_get_half_float(&cur_value, lptr);
				break;
			case CborAttrFloatType:
				err |= cbor_value_get_float(&cur_value, lptr);
				break;
			case CborAttrDoubleType:
				err |= cbor_value_get_double(&cur_value, lptr);
				break;
#endif
			case CborAttrByteStringType: {
				size_t len = cursor->len;

				err |= cbor_value_copy_byte_string(&cur_value, lptr, &len, NULL);
				*cursor->addr.bytestring.len = len;
				break;
			}
			case CborAttrTextStringType: {
				size_t len = cursor->len;

				err |= cbor_value_copy_text_string(&cur_value, lptr, &len, NULL);
				break;
			}
			case CborAttrArrayType:
				err |= cbor_read_array(&cur_value, &cursor->addr.array);
				continue;
			case CborAttrObjectType:
				err |= cbor_internal_read_object(&cur_value, cursor->addr.obj,
								 NULL, 0);
				continue;
			default:
				err |= CborErrorIllegalType;
			}
		}
		err = cbor_value_advance(&cur_value);
	}
	if (!err) {
		/* that should be it for this container */
		err |= cbor_value_leave_container(root_value, &cur_value);
	}
	return err;
}

int
cbor_read_array(struct CborValue *value, const struct cbor_array_t *arr)
{
	CborError err = 0;
	struct CborValue elem;
	int off, arrcount;
	size_t len;
	void *lptr;
	char *tp;

	err = cbor_value_enter_container(value, &elem);
	if (err) {
		return err;
	}
	arrcount = 0;
	tp = arr->arr.strings.store;
	for (off = 0; off < arr->maxlen; off++) {
		switch (arr->element_type) {
		case CborAttrBooleanType:
			lptr = &arr->arr.booleans.store[off];
			err |= cbor_value_get_boolean(&elem, lptr);
			break;
		case CborAttrIntegerType:
			lptr = &arr->arr.integers.store[off];
			err |= cbor_value_get_int64(&elem, lptr);
			break;
		case CborAttrUnsignedIntegerType:
			lptr = &arr->arr.uintegers.store[off];
			err |= cbor_value_get_uint64(&elem, lptr);
			break;
#if CBORATTR_FLOAT_SUPPORT != 0
		case CborAttrHalfFloatType:
			lptr = &arr->arr.halffloats.store[off];
			err |= cbor_value_get_half_float(&elem, lptr);
			break;
		case CborAttrFloatType:
		case CborAttrDoubleType:
			lptr = &arr->arr.reals.store[off];
			err |= cbor_value_get_double(&elem, lptr);
			break;
#endif
		case CborAttrTextStringType:
			len = arr->arr.strings.storelen - (tp - arr->arr.strings.store);
			err |= cbor_value_copy_text_string(&elem, tp, &len, NULL);
			arr->arr.strings.ptrs[off] = tp;
			tp += len + 1;
			break;
		case CborAttrStructObjectType:
			err |= cbor_internal_read_object(&elem, arr->arr.objects.subtype, arr, off);
			break;
		default:
			err |= CborErrorIllegalType;
			break;
		}
		arrcount++;
		if (arr->element_type != CborAttrStructObjectType) {
			err |= cbor_value_advance(&elem);
		}
		if (!cbor_value_is_valid(&elem)) {
			break;
		}
	}
	if (arr->count) {
		*arr->count = arrcount;
	}
	while (!cbor_value_at_end(&elem)) {
		err |= CborErrorDataTooLarge;
		cbor_value_advance(&elem);
	}
	err |= cbor_value_leave_container(value, &elem);
	return err;
}

int
cbor_read_object(struct CborValue *value, const struct cbor_attr_t *attrs)
{
	int st;

	st = cbor_internal_read_object(value, attrs, NULL, 0);
	return st;
}

/*
 * Read in cbor key/values from flat buffer pointed by data, and fill them
 * into attrs.
 *
 * @param data		Pointer to beginning of cbor encoded data
 * @param len		Number of bytes in the buffer
 * @param attrs		Array of cbor objects to look for.
 *
 * @return		0 on success; non-zero on failure.
 */
int
cbor_read_flat_attrs(const uint8_t *data, int len,
					 const struct cbor_attr_t *attrs)
{
	struct cbor_buf_reader reader;
	struct CborParser parser;
	struct CborValue value;
	CborError err;

	cbor_buf_reader_init(&reader, data, len);
	err = cbor_parser_init(&reader.r, 0, &parser, &value);
	if (err != CborNoError) {
		return -1;
	}
	return cbor_read_object(&value, attrs);
}
