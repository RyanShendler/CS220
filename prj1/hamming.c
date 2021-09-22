#include "hamming.h"

#include <assert.h>

/**
  All bitIndex'es are numbered starting at the LSB which is given index 1

  ** denotes exponentiation; note that 2**n == (1 << n)
*/

/** Return bit at bitIndex from word. */
static inline unsigned
get_bit(HammingWord word, int bitIndex)
{
  assert(bitIndex > 0);
  //@TODO
  return ((word & (1 << (bitIndex-1))) >> (bitIndex-1));
}

/** Return word with bit at bitIndex in word set to bitValue. */
static inline HammingWord
set_bit(HammingWord word, int bitIndex, unsigned bitValue)
{
  assert(bitIndex > 0);
  assert(bitValue == 0 || bitValue == 1);
  //@TODO
  if(bitValue == 1u) return (word |= (1ul << (bitIndex-1)));
  else return (word &= ~(1ul << (bitIndex-1)));
}

/** Given a Hamming code with nParityBits, return 2**nParityBits - 1,
 *  i.e. the max # of bits in an encoded word (# data bits + # parity
 *  bits).
 */
static inline unsigned
get_n_encoded_bits(unsigned nParityBits)
{
  //@TODO
  return ((1u << nParityBits)-1u);
}

/** Return non-zero if bitIndex indexes a bit which will be used for a
 *  Hamming parity bit; i.e. the bit representation of bitIndex
 *  contains only a single 1.
 */
static inline int
is_parity_position(int bitIndex)
{
  assert(bitIndex > 0);
  //@TODO
  //only powers of 2 have a single 1 in them
  if((bitIndex & (bitIndex-1)) == 0) return 1; //a power of 2 bitwise anded with the number directly below it always gives 0
  else return 0;
}

/** Return the parity over the data bits in word specified by the
 *  parity bit bitIndex.  The word contains a total of nBits bits.
 *  Equivalently, return parity over all data bits whose bit-index has
 *  a 1 in the same position as in bitIndex.
 */
static int
compute_parity(HammingWord word, int bitIndex, unsigned nBits)
{
  assert(bitIndex > 0);
  //@TODO
  int parity;
  int first = 1;
  for(int i = 1; i <= nBits; i++){
	  if((i != bitIndex) && ((bitIndex & i) != 0) && (first == 1)){
		  parity = get_bit(word, i);
		  first = 0;
	  } else if((i != bitIndex) && ((bitIndex & i) != 0)){
		  parity = parity ^ get_bit(word, i);
	  } 
  }
  return parity;
}

/** Encode data using nParityBits Hamming code parity bits.
 *  Assumes data is within range of values which can be encoded using
 *  nParityBits.
 */
HammingWord
hamming_encode(HammingWord data, unsigned nParityBits)
{
  //@TODO
  HammingWord encoded = 0;
  int numBits = (int) get_n_encoded_bits(nParityBits);
  int dataIndex = 1;
  for(int i = 1; i <= numBits; i++){
	  if(is_parity_position(i) == 0){
		encoded = set_bit(encoded, i, get_bit(data, dataIndex));
		dataIndex++;
	  }
  }
  for(int i = 1; i <= numBits; i++){
	  if(is_parity_position(i) == 1){
		  unsigned parity =  (unsigned) compute_parity(encoded, i, (unsigned) numBits);
		  encoded = set_bit(encoded, i, parity);
	  }
  }
  return encoded;
}

/** Decode encoded using nParityBits Hamming code parity bits.
 *  Set *hasError if an error was corrected.
 *  Assumes that data is within range of values which can be decoded
 *  using nParityBits.
 */
HammingWord
hamming_decode(HammingWord encoded, unsigned nParityBits, int *hasError)
{
  //@TODO
  HammingWord decoded = 0;
  int syndrome = 0;
  int numBits = (int) get_n_encoded_bits(nParityBits);
  for(int i = 1; i <= numBits; i++){
	if(is_parity_position(i) == 1){
		unsigned parity = (unsigned) compute_parity(encoded, i, (unsigned) numBits);
		if(parity != get_bit(encoded, i)){
		       	syndrome = (syndrome | i);
		}
	}
  }
  if(syndrome != 0){
  	unsigned value = get_bit(encoded, syndrome);
	if (value == 1u){
		encoded = set_bit(encoded, syndrome, 0u);
	} else {
		encoded = set_bit(encoded, syndrome, 1u);
	}
	*hasError = 1;
  }
  int decoded_bit = 1;
  for(int i = 1; i <= numBits; i++){
	  if(is_parity_position(i) == 0){
		  decoded = set_bit(decoded, decoded_bit, get_bit(encoded, i));
		  decoded_bit++;
	  }
  }
  	
  return decoded;
}
