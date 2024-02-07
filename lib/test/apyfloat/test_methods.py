from itertools import permutations as perm
import pytest
from apytypes import APyFloat, APyFixed


@pytest.mark.float_special
@pytest.mark.parametrize("float_s", ["nan", "inf", "-inf", "0.0", "-0.0"])
def test_special_conversions(float_s):
    assert (
        str(float(APyFloat.from_float(float(float_s), 5, 5)))
        == str(float(float_s))
        == float_s
    )


@pytest.mark.parametrize("exp", list(perm([5, 6, 7, 8], 2)))
@pytest.mark.parametrize("man", list(perm([5, 6, 7, 8], 2)))
@pytest.mark.parametrize("val", [2.625, 12])
@pytest.mark.parametrize("neg", [-1.0, 1.0])
def test_normal_conversions(exp, man, val, neg):
    val *= neg
    assert (
        float(APyFloat.from_float(val, exp[0], man[0]))
        == float(APyFloat.from_float(val, exp[1], man[1]))
        == val
    )


@pytest.mark.parametrize("sign", ["1", "0"])
@pytest.mark.parametrize(
    "absx,ans",
    [
        ("00000_00", "0.0"),  # Zero
        ("0000_001", "1*2**-9"),  # Min subnorm
        ("0000_010", "2*2**-9"),
        ("0000_011", "3*2**-9"),
        ("0000_100", "4*2**-9"),
        ("0000_101", "5*2**-9"),
        ("0000_110", "6*2**-9"),
        ("0000_111", "7*2**-9"),  # Max subnorm
        ("0001_000", "2**-6"),  # Min normal
        ("1110_111", "240.0"),  # Max normal
        ("1111_000", 'float("inf")'),  # Infinity
    ],
)
def test_bit_conversions_e4m3(absx, sign, ans):
    assert float(APyFloat.from_bits(int(f"{sign}_{absx}", 2), 4, 3)) == eval(
        f'{"-" if sign == "1" else ""}{ans}'
    )
    assert APyFloat.from_float(
        eval(f'{"-" if sign == "1" else ""}{ans}'), 4, 3
    ).to_bits() == int(f"{sign}_{absx}", 2)


@pytest.mark.parametrize("sign", ["1", "0"])
@pytest.mark.parametrize(
    "absx,ans",
    [
        ("11111_01", 'float("nan")'),  # NaN
        ("11111_10", 'float("nan")'),  # NaN
        ("11111_11", 'float("nan")'),  # NaN
    ],
)
def test_bit_conversion_nan_e5m2(absx, sign, ans):
    assert str(float(APyFloat.from_bits(int(f"{sign}_{absx}", 2), 5, 2))) == str(
        eval(f'{"-" if sign == "1" else ""}{ans}')
    )
    bits = APyFloat.from_float(
        eval(f'{"-" if sign == "1" else ""}{ans}'), 5, 2
    ).to_bits()
    assert (bits & 0x3) != 0


def test_latex():
    assert APyFloat.from_float(0, 2, 2)._repr_latex_() == "$0$"
    assert APyFloat.from_float(20, 2, 2)._repr_latex_() == r"$\infty$"
    assert (
        APyFloat.from_float(0.5, 2, 2)._repr_latex_()
        == r"$\frac{2}{2^{2}}2^{1-1} = 2\times 2^{-2} = 0.5$"
    )
    assert (
        APyFloat.from_float(-0.5, 2, 2)._repr_latex_()
        == r"$-\frac{2}{2^{2}}2^{1-1} = -2\times 2^{-2} = -0.5$"
    )
    assert (
        APyFloat.from_float(1.5, 2, 2)._repr_latex_()
        == r"$\left(1 + \frac{2}{2^{2}}\right)2^{1-1} = 6\times 2^{-2} = 1.5$"
    )
    assert APyFloat.from_float(float("NaN"), 2, 2)._repr_latex_() == r"$\textrm{NaN}$"