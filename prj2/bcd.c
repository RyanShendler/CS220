#include "bcd.h"

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>

//returns the number of digits in a binary
int num_Binary_digits(Binary value){
	int num = 0;
	while(value > 0){
		value /= 10;
		num++;
	}
	return num;
}

Bcd get_bcd_digit(Bcd bcd, int index){
	Bcd mask = 15;
	Bcd shift = 4*index;
	return ((bcd & (mask << shift)) >> shift);
}

Bcd set_bcd_digit(Bcd bcd, int index, Bcd value){
	Bcd mask = 15;
	bcd  = (bcd | (mask << (4*index)));
	return (bcd & (value << (4*index)));
}

static Bcd bcd_multiply_digit(Bcd multiplicand, unsigned bcdDigit, BcdError *error){
	Bcd product = 0;
	Bcd carry = 0;
	for(int i = 0; i < MAX_BCD_DIGITS; i++){
		Bcd temp = 0;
		Bcd digit = get_bcd_digit(multiplicand, i);
		if (error != NULL){
			if(digit > 9) *error = BAD_VALUE_ERR;
		}
		temp = (digit * bcdDigit) + carry;
		carry = temp/10;
		product = product | ((temp%10) << (i*4));
		if(i == (MAX_BCD_DIGITS-1)){
			if(error != NULL){
				if(carry != 0) *error = OVERFLOW_ERR;
			}
		}
	}
	return product;
}
/** Return BCD encoding of binary (which has normal binary representation).
 *
 *  Examples: binary_to_bcd(0xc) => 0x12;
 *            binary_to_bcd(0xff) => 0x255
 *
 *  If error is not NULL, sets *error to OVERFLOW_ERR if binary is too
 *  big for the Bcd type, otherwise *error is unchanged.
 */
Bcd
binary_to_bcd(Binary value, BcdError *error)
{
  int numDigits = num_Binary_digits(value);
  Bcd retVal = 0;
  if(error != NULL){
	  if(numDigits > MAX_BCD_DIGITS){
		  *error = OVERFLOW_ERR;
	  }
  }
  for(int i = 0; i < numDigits; i++){
	  Binary digit = value%10;
	  int shift = i*4;
	  retVal  = retVal | (digit << shift);
	  value /= 10;
  }
  
  return retVal;
}

/** Return binary encoding of BCD value bcd.
 *
 *  Examples: bcd_to_binary(0x12) => 0xc;
 *            bcd_to_binary(0x255) => 0xff
 *
 *  If error is not NULL, sets *error to BAD_VALUE_ERR if bcd contains
 *  a bad BCD digit.
 *  Cannot overflow since Binary can represent larger values than Bcd
 */
Binary
bcd_to_binary(Binary bcd, BcdError *error)
{
  Binary retVal = 0;
  int index = 0;
  for(int i = 0; i < MAX_BCD_DIGITS; i++){
	  Binary digit = get_bcd_digit(bcd, i);
	  if(error != NULL){
	  	if(digit >= 10) *error = BAD_VALUE_ERR;
	  }
	  retVal += (digit*(unsigned long long)pow(10.0, (double)i));
	  index++;
  }
  return retVal;
}

/** Return BCD encoding of decimal number corresponding to string s.
 *  Behavior undefined on overflow or if s contains a non-digit
 *  character.  Rougly equivalent to atoi().
 *
 *  If error is not NULL, sets *error to OVERFLOW_ERR if binary is too
 *  big for the Bcd type, otherwise *error is unchanged.
 */
Bcd
str_to_bcd(const char *s, const char **p, BcdError *error)
{
  Bcd retVal = 0;
  int numDigits = 0;
  int setP = 0;
  while(*s != '\0'){
	  char digit = *s;
	  if(digit > '9' || digit < '0'){
		  *p = s;
		  setP = 1;
		  break;
	  } 
	  numDigits++;
	  if(error != NULL){
		  if(numDigits > MAX_BCD_DIGITS) *error = OVERFLOW_ERR;
	  }
	  char value =  digit - '0';
	  retVal = retVal << 4;
	  retVal = retVal | value;
	  s++;

  }
  if(setP != 1) *p = s;
  return retVal;
}

/** Convert bcd to a NUL-terminated string in buf[] without any
 *  non-significant leading zeros.  Never write more than bufSize
 *  characters into buf.  The return value is the number of characters
 *  written (excluding the NUL character used to terminate strings).
 *
 *  If error is not NULL, sets *error to BAD_VALUE_ERR is bcd contains
 *  a BCD digit which is greater than 9, OVERFLOW_ERR if bufSize bytes
 *  is less than BCD_BUF_SIZE, otherwise *error is unchanged.
 */
int
bcd_to_str(Bcd bcd, char buf[], size_t bufSize, BcdError *error)
{
  int numChar = 0;
  if(error != NULL){
  	if(bufSize < BCD_BUF_SIZE) *error = OVERFLOW_ERR;
  }
  for(int i = 0; i < MAX_BCD_DIGITS; i++){
	  Bcd digit = get_bcd_digit(bcd, i);
	  if(error != NULL){
		  if(digit > 9) *error = BAD_VALUE_ERR;
	  }
  }
  numChar = snprintf(buf, bufSize, "%llx", bcd);
  return numChar;
}

/** Return the BCD representation of the sum of BCD int's x and y.
 *
 *  If error is not NULL, sets *error to to BAD_VALUE_ERR is x or y
 *  contains a BCD digit which is greater than 9, OVERFLOW_ERR on
 *  overflow, otherwise *error is unchanged.
 */
Bcd
bcd_add(Bcd x, Bcd y, BcdError *error)
{
  Bcd carry = 0;
  Bcd final_sum = 0;
  for(int i = 0;i < MAX_BCD_DIGITS; i++){
	  Bcd digitx = get_bcd_digit(x, i);
	  Bcd digity = get_bcd_digit(y, i);
	  Bcd temp_sum = 0;
	  if(error != NULL){
		  if(digitx > 9 || digity > 9){
			  *error = BAD_VALUE_ERR;
			  break;
		  }
	  }
	  temp_sum = digitx + digity + carry;
	  carry = temp_sum/10;
	  final_sum = final_sum | ((temp_sum%10) << (i*4));
	  if(i == (MAX_BCD_DIGITS-1)){
		  if(error != NULL){
			  if(carry !=0) *error = OVERFLOW_ERR;
		  }
	  }
  } 
  return final_sum;
}

/** Return the BCD representation of the product of BCD int's x and y.
 *
 * If error is not NULL, sets *error to to BAD_VALUE_ERR is x or y
 * contains a BCD digit which is greater than 9, OVERFLOW_ERR on
 * overflow, otherwise *error is unchanged.
 */
Bcd
bcd_multiply(Bcd x, Bcd y, BcdError *error)
{
  Bcd product = 0;
  for(int i = 0; i < MAX_BCD_DIGITS; i++){
	Bcd digit = get_bcd_digit(y, i);
	if(error != NULL){
		if(digit > 9) *error = BAD_VALUE_ERR;
	}
	Bcd temp = bcd_multiply_digit(x, digit, error);
	product = bcd_add(product, (temp*(unsigned long long)pow(10.0, (double)i)), error);
  }
  return product;
}
