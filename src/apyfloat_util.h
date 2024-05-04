#ifndef _APYFLOAT_UTIL_H
#define _APYFLOAT_UTIL_H

#include "apyfixed.h"
#include "apytypes_common.h"

/*!
 * Sizes of APyFloat datatypes
 */
static constexpr std::size_t _MAN_T_SIZE_BYTES = sizeof(man_t);
static constexpr std::size_t _MAN_T_SIZE_BITS = 8 * _MAN_T_SIZE_BYTES;
static constexpr std::size_t _EXP_T_SIZE_BYTES = sizeof(exp_t);
static constexpr std::size_t _EXP_T_SIZE_BITS = 8 * _EXP_T_SIZE_BYTES;

//! Quantize mantissa
void APY_INLINE quantize_mantissa(
    man_t& man,
    exp_t& exp,
    std::uint8_t bits_to_quantize,
    bool sign,
    man_t man_msb_constant,
    std::uint8_t bits_to_quantize_dec,
    man_t sticky_constant,
    QuantizationMode quantization
)
{
    // Calculate quantization bit
    man_t G, // Guard (bit after LSB)
        T,   // Sticky bit, logical OR of all the bits after the guard bit
        B;   // Quantization bit to add to LSB

    G = (man >> bits_to_quantize_dec) & 1;
    T = (man & sticky_constant) != 0;

    // Initial value for mantissa
    man_t res_man = man >> bits_to_quantize;

    switch (quantization) {
    case QuantizationMode::RND_CONV: // TIES_EVEN
        // Using 'res_man' directly here is fine since G can only be '0' or '1',
        // thus calculating the LSB of 'res_man' is not needed.
        B = G & (res_man | T);
        break;
    case QuantizationMode::RND_CONV_ODD: // TIES_ODD
        B = G & ((res_man ^ 1) | T);
        break;
    case QuantizationMode::TRN_INF: // TO_POSITIVE
        B = sign ? 0 : (G | T);
        break;
    case QuantizationMode::TRN: // TO_NEGATIVE
        B = sign ? (G | T) : 0;
        break;
    case QuantizationMode::TRN_AWAY: // TO_AWAY
        B = G | T;
        break;
    case QuantizationMode::TRN_ZERO: // TO_ZERO
        B = 0;
        break;
    case QuantizationMode::TRN_MAG: // Does not really make sense for
                                    // floating-point
        B = sign;
        break;
    case QuantizationMode::RND_INF: // TIES_AWAY
        B = G;
        break;
    case QuantizationMode::RND_ZERO: // TIES_ZERO
        B = G & T;
        break;
    case QuantizationMode::RND: // TIES_POS
        B = G & (T | !sign);
        break;
    case QuantizationMode::RND_MIN_INF: // TIES_NEG
        B = G & (T | sign);
        break;
    case QuantizationMode::JAM:
        B = 0;
        res_man |= 1;
        break;
    case QuantizationMode::JAM_UNBIASED:
        B = 0;
        if (T || G) {
            res_man |= 1;
        }
        break;
    case QuantizationMode::STOCH_WEIGHTED: {
        const man_t trailing_bits = man & ((1ULL << bits_to_quantize) - 1);
        const man_t weight = random_number_float() & ((1ULL << bits_to_quantize) - 1);
        // Since the weight won't be greater than the discarded bits,
        // this will never round an already exact number.
        B = (trailing_bits + weight) >> bits_to_quantize;
    } break;
    case QuantizationMode::STOCH_EQUAL:
        // Only perform the quantization if the result is not exact.
        B = (G || T) ? random_number_float() & 1 : 0;
        break;
    default:
        throw NotImplementedException(
            "Not implemented: quantize_mantissa() with "
            "unknown (did you pass `int` as `QuantizationMode`?)"
        );
    }
    man = res_man;
    man += B;
    if (man & man_msb_constant) {
        ++exp;
        man = 0;
    }
}

//! Quantize mantissa
void APY_INLINE quantize_mantissa(
    man_t& man,
    exp_t& exp,
    std::uint8_t bits_to_quantize,
    bool sign,
    man_t man_msb_constant,
    QuantizationMode quantization
)
{
    quantize_mantissa(
        man,
        exp,
        bits_to_quantize,
        sign,
        man_msb_constant,
        bits_to_quantize - 1,
        (1ULL << (bits_to_quantize - 1)) - 1,
        quantization
    );
}

//! Check if one should saturate to infinity or maximum normal number
bool APY_INLINE do_infinity(QuantizationMode mode, bool sign)
{
    switch (mode) {
    case QuantizationMode::TRN_ZERO:     // TO_ZERO
    case QuantizationMode::JAM:          // JAM
    case QuantizationMode::JAM_UNBIASED: // JAM_UNBIASED
        return false;
    case QuantizationMode::TRN: // TO_NEG
        return sign;
    case QuantizationMode::TRN_INF: // TO_POS
        return !sign;
    default:
        return true;
    }
}

//! Fast integer power by squaring.
man_t ipow(man_t base, unsigned int n);

APY_INLINE unsigned int leading_zeros_apyfixed(const APyFixed& fx)
{
    // Calculate the number of left shifts needed to make fx>=1.0
    const int zeros = fx.leading_zeros() - fx.int_bits();
    return std::max(0, zeros + 1);
}

void quantize_apymantissa(
    APyFixed& apyman, bool sign, int bits, QuantizationMode quantization
);
QuantizationMode translate_quantization_mode(QuantizationMode quantization, bool sign);

#endif // _APYFLOAT_UTIL_H
