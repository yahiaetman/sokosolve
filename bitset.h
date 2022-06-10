#ifndef _SOKOSOLVE_BITSET_H
#define _SOKOSOLVE_BITSET_H

// A bitset is an array of bits_t which was can manipulate on bit level
// The bistset size is multiples of 64-bit unsigned integers
typedef unsigned long long bits_t;
// post_t is used to present a position inside the bitset
typedef unsigned short pos_t;
// BITS_CNT presents the number of bits in bits_t (which should be 64)
#define BITS_CNT (sizeof(bits_t)*8)

// Set the bit in "bitset" at position "pos" to 1
inline void set_bit(bits_t* bitset, pos_t pos){
    bitset[pos / BITS_CNT] |= (bits_t)(1) << (pos % BITS_CNT);
}


// Set the bit in "bitset" at position "pos" to 0
inline void clear_bit(bits_t* bitset, pos_t pos){
    bitset[pos / BITS_CNT] &= ~((bits_t)(1) << (pos % BITS_CNT));
}

// Reads the bit in "bitset" at position "pos"
inline bool get_bit(bits_t* bitset, pos_t pos){
    return (bitset[pos / BITS_CNT] & ((bits_t)(1) << (pos % BITS_CNT))) != 0;
}

// Checks that for every set bit in "under", the corresponding bit in "cover" is also 1
// Size is the number of bits_t in the bitset
inline bool bitset_covers_all(bits_t* under, bits_t* cover, size_t size){
    for(size_t index = 0; index < size; ++index){
        if(under[index] & ~cover[index]) return false;
    }
    return true;
}

// Checks that for any set bit in "under", the corresponding bit in "cover" is also 1
// Size is the number of bits_t in the bitset
inline bool bitset_covers_any(bits_t* under, bits_t* cover, size_t size){
    for(size_t index = 0; index < size; ++index){
        if(under[index] & cover[index]) return true;
    }
    return false;
}

// Checks that the bitsets "first" & "second" are equal
// Size is the number of bits_t in the bitset
inline bool bitset_equals(bits_t* first, bits_t* second, size_t size){
    for(size_t index = 0; index < size; ++index){
        if(first[index] != second[index]) return false;
    }
    return true;
}

// Compares bitsets "first" & "second" as N-bit integers
// returns 1 if second > first, -1 if second < first, 0 if second == first
// Size is the number of bits_t in the bitset
inline int bitset_cmp(bits_t* first, bits_t* second, size_t size){
    for(size_t index = 0; index < size; ++index){
        bits_t f = first[index], s = second[index];
        if(f < s) return 1;
        else if(f > s) return -1;
    }
    return 0;
}

// returns the XOR of the bitsets "first" and "second" in the bitset "result"
// Size is the number of bits_t in the bitset
inline void bitset_xor(bits_t* first, bits_t* second, size_t size, bits_t* result){
    for(size_t index = 0; index < size; ++index){
        result[index] = first[index] ^ second[index];
    }
}

// Copy the content of "src" into "dst"
// Size is the number of bits_t in the bitset
inline void bitset_copy(bits_t* src, bits_t* dest, size_t size){
    for(size_t index = 0; index < size; ++index)
        dest[index] = src[index];
}

#endif