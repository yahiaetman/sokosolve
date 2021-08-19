#ifndef BITSET_H
#define BITSET_H

typedef unsigned long long bits_t;
typedef unsigned short pos_t;
#define BITS_CNT (sizeof(bits_t)*8)

inline void set_bit(bits_t* bitset, pos_t pos){
    bitset[pos / BITS_CNT] |= (bits_t)(1) << (pos % BITS_CNT);
}

inline void clear_bit(bits_t* bitset, pos_t pos){

    bitset[pos / BITS_CNT] &= ~((bits_t)(1) << (pos % BITS_CNT));
}

inline bool get_bit(bits_t* bitset, pos_t pos){
    return (bitset[pos / BITS_CNT] & ((bits_t)(1) << (pos % BITS_CNT))) != 0;
}

inline bool bitset_covers_all(bits_t* under, bits_t* cover, size_t size){
    for(size_t index = 0; index < size; ++index){
        if(under[index] & ~cover[index]) return false;
    }
    return true;
}

inline bool bitset_covers_any(bits_t* under, bits_t* cover, size_t size){
    for(size_t index = 0; index < size; ++index){
        if(under[index] & cover[index]) return true;
    }
    return false;
}

inline bool bitset_equals(bits_t* first, bits_t* second, size_t size){
    for(size_t index = 0; index < size; ++index){
        if(first[index] != second[index]) return false;
    }
    return true;
}

inline int bitset_cmp(bits_t* first, bits_t* second, size_t size){
    for(size_t index = 0; index < size; ++index){
        bits_t f = first[index], s = second[index];
        if(f < s) return 1;
        else if(f > s) return -1;
    }
    return 0;
}

inline void bitset_copy(bits_t* src, bits_t* dest, size_t size){
    for(size_t index = 0; index < size; ++index)
        dest[index] = src[index];
}

#endif