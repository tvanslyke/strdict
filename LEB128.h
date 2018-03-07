#ifndef LEB128_H
#define LEB128_H

#include <stdint.h>

typedef struct leb128_encoding 
{
	unsigned char encoding[9];
	unsigned char len;
} Leb128Encoding;


Leb128Encoding leb128_encode(uint_least64_t value)
{
	Leb128Encoding enc;
	unsigned char* byte = enc.encoding;
	do {
		*byte = (value & 0x7f); // lower 7 bits
		value >>= 7;
		// set high order bit if value is not zero yet (more bytes to come)
		*byte++ |= (!!value) << 7;
	} while(value);
	// set the length field
	enc.len = byte - enc.encoding;
	return enc;
}

uint_least64_t leb128_decode(const unsigned char* data, size_t* count)
{
	uint_least64_t value = 0;
	unsigned char shift = 0;
	do {
		assert(shift < sizeof(value) * CHAR_BIT);
		value |= ((uint_least64_t)(0x7f & *data)) << shift;
		shift += 7;
	} while(*data++ & 0x80);
	*count = (shift / 7);
	return value;
}


#endif /* LEB128_H */
