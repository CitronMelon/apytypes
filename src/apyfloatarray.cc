// Python object access through Pybind
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
namespace py = pybind11;

#include "apyfloat.h"
#include "apyfloatarray.h"
#include "python_util.h"
#include <algorithm>
#include <fmt/format.h>
#include <iostream>
#include <stdexcept>
#include <string>

APyFloatArray::APyFloatArray(
    const pybind11::sequence& sign_seq,
    const pybind11::sequence& exp_seq,
    const pybind11::sequence& man_seq,
    std::uint8_t exp_bits,
    std::uint8_t man_bits,
    std::optional<exp_t> bias
)
    : exp_bits(exp_bits)
    , man_bits(man_bits)
{
    if (bias.has_value()) {
        this->bias = bias.value();
    } else {
        // Not very pretty way to get the IEEE-like bias
        this->bias = APyFloat::ieee_bias(exp_bits);
    }

    const auto signs_shape = python_sequence_extract_shape(sign_seq);
    const auto exps_shape = python_sequence_extract_shape(exp_seq);
    const auto mans_shape = python_sequence_extract_shape(man_seq);

    if (!((signs_shape == exps_shape) && (signs_shape == mans_shape))) {
        throw std::domain_error("Shape mismatch during construction");
    }

    shape = signs_shape;

    auto signs = python_sequence_walk<py::int_, py::bool_>(sign_seq);
    auto exps = python_sequence_walk<py::int_>(exp_seq);
    auto mans = python_sequence_walk<py::int_>(man_seq);

    for (std::size_t i = 0; i < signs.size(); ++i) {
        bool sign;
        if (py::isinstance<py::bool_>(signs[i])) {
            sign = static_cast<bool>(py::cast<py::bool_>(signs[i]));
        } else if (py::isinstance<py::int_>(signs[i])) {
            sign = static_cast<int>(py::cast<py::int_>(signs[i])
            ); // Must cast to int here
        } else {
            throw std::domain_error("Invalid objects in sign");
        }
        exp_t exp = static_cast<exp_t>(py::cast<py::int_>(exps[i]));
        man_t man = static_cast<man_t>(py::cast<py::int_>(mans[i]));

        data.push_back({ sign, exp, man });
    }
}

APyFloatArray::APyFloatArray(
    const std::vector<std::size_t>& shape,
    exp_t exp_bits,
    std::uint8_t man_bits,
    std::optional<exp_t> bias
)
    : shape(shape)
    , exp_bits(exp_bits)
    , man_bits(man_bits)
    , bias(bias.value_or(APyFloat::ieee_bias(exp_bits)))
{
}

/* ********************************************************************************** *
 * *                            Binary arithmetic operators                         * *
 * ********************************************************************************* */

APyFloatArray APyFloatArray::perform_basic_arithmetic(
    const APyFloatArray& rhs, ArithmeticOperation op
) const
{
    // Make sure `_shape` of `*this` and `rhs` are the same
    if (shape != rhs.shape) {
        std::string op_str;
        if (op == ArithmeticOperation::ADDITION)
            op_str = "add";
        else if (op == ArithmeticOperation::SUBTRACTION)
            op_str = "sub";
        else if (op == ArithmeticOperation::MULTIPLICATION)
            op_str = "mul";
        else if (op == ArithmeticOperation::DIVISION)
            op_str = "truediv";
        throw std::length_error(fmt::format(
            "APyFloatArray.__{}__: shape missmatch, lhs.shape={}, rhs.shape={}",
            op_str,
            string_from_vec(shape),
            string_from_vec(rhs.shape)
        ));
    }

    // Calculate new format
    APyFloatArray res(
        shape, std::max(exp_bits, rhs.exp_bits), std::max(man_bits, rhs.man_bits)
    );
    res.bias = APyFloat::ieee_bias(res.exp_bits);

    // Perform operation
    res.data.resize(data.size());
    for (std::size_t i = 0; i < data.size(); i++) {
        APyFloat lhs_scalar(data[i], exp_bits, man_bits, bias);
        APyFloat rhs_scalar(rhs.data[i], rhs.exp_bits, rhs.man_bits, rhs.bias);

        if (op == ArithmeticOperation::ADDITION)
            res.data[i] = (lhs_scalar + rhs_scalar).get_data();
        else if (op == ArithmeticOperation::SUBTRACTION)
            res.data[i] = (lhs_scalar - rhs_scalar).get_data();
        else if (op == ArithmeticOperation::MULTIPLICATION)
            res.data[i] = (lhs_scalar * rhs_scalar).get_data();
        else if (op == ArithmeticOperation::DIVISION)
            res.data[i] = (lhs_scalar / rhs_scalar).get_data();
        else
            throw NotImplementedException("Arithmetic operation not implemented yet");
    }

    return res;
}

APyFloatArray APyFloatArray::perform_basic_arithmetic(
    const APyFloat& rhs, ArithmeticOperation op
) const
{
    // Calculate new format
    APyFloatArray res(
        shape,
        std::max(exp_bits, rhs.get_exp_bits()),
        std::max(man_bits, rhs.get_man_bits())
    );
    res.bias = APyFloat::ieee_bias(res.exp_bits);

    // Perform operations
    res.data.resize(data.size());
    for (std::size_t i = 0; i < data.size(); i++) {
        APyFloat lhs_scalar(data[i], exp_bits, man_bits, bias);

        if (op == ArithmeticOperation::ADDITION)
            res.data[i] = (lhs_scalar + rhs).get_data();
        else if (op == ArithmeticOperation::SUBTRACTION)
            res.data[i] = (lhs_scalar - rhs).get_data();
        else if (op == ArithmeticOperation::MULTIPLICATION)
            res.data[i] = (lhs_scalar * rhs).get_data();
        else if (op == ArithmeticOperation::DIVISION)
            res.data[i] = (lhs_scalar / rhs).get_data();
        else
            throw NotImplementedException("Arithmetic operation not implemented yet");
    }

    return res;
}

APyFloatArray APyFloatArray::operator+(const APyFloatArray& rhs) const
{
    return perform_basic_arithmetic(rhs, ArithmeticOperation::ADDITION);
}

APyFloatArray APyFloatArray::operator+(const APyFloat& rhs) const
{
    return perform_basic_arithmetic(rhs, ArithmeticOperation::ADDITION);
}

APyFloatArray APyFloatArray::operator-(const APyFloatArray& rhs) const
{
    return perform_basic_arithmetic(rhs, ArithmeticOperation::SUBTRACTION);
}

APyFloatArray APyFloatArray::operator-(const APyFloat& rhs) const
{
    return perform_basic_arithmetic(rhs, ArithmeticOperation::SUBTRACTION);
}

APyFloatArray APyFloatArray::operator*(const APyFloatArray& rhs) const
{
    return perform_basic_arithmetic(rhs, ArithmeticOperation::MULTIPLICATION);
}

APyFloatArray APyFloatArray::operator*(const APyFloat& rhs) const
{
    return perform_basic_arithmetic(rhs, ArithmeticOperation::MULTIPLICATION);
}

APyFloatArray APyFloatArray::operator/(const APyFloatArray& rhs) const
{
    return perform_basic_arithmetic(rhs, ArithmeticOperation::DIVISION);
}

APyFloatArray APyFloatArray::operator/(const APyFloat& rhs) const
{
    return perform_basic_arithmetic(rhs, ArithmeticOperation::DIVISION);
}

std::string APyFloatArray::repr() const
{
    std::stringstream ss {};
    ss << "APyFloatArray(";
    if (shape[0]) {
        std::stringstream sign_str {}, exp_str {}, man_str {};
        sign_str << "[";
        exp_str << "[";
        man_str << "[";
        for (std::size_t i = 0; i < data.size(); ++i) {
            const APyFloatData d = data[i];
            const bool is_last = i == (data.size() - 1);
            sign_str << (d.sign ? "1" : "0") << (is_last ? "" : ", ");
            exp_str << d.exp << (is_last ? "" : ", ");
            man_str << d.man << (is_last ? "" : ", ");
        }
        ss << sign_str.str() << "], " << exp_str.str() << "], " << man_str.str()
           << "], ";
    } else {
        ss << "[], [], [], ";
    }
    ss << "shape=(";
    ss << string_from_vec(shape);
    ss << "), "
       << "exp_bits=" << static_cast<unsigned>(exp_bits) << ", "
       << "man_bits=" << static_cast<unsigned>(man_bits) << ", "
       << "bias=" << bias << ")";
    return ss.str();
}

// The shape of the array
pybind11::tuple APyFloatArray::get_shape() const
{
    py::tuple result(get_ndim());
    for (std::size_t i = 0; i < get_ndim(); i++) {
        result[i] = shape[i];
    }
    return result;
}

size_t APyFloatArray::get_ndim() const { return shape.size(); }

size_t APyFloatArray::get_size() const { return shape[0]; }

py::array_t<double> APyFloatArray::to_numpy() const
{
    // Shape of NumPy object is same as `APyFloatArray` object
    std::vector<py::ssize_t> numpy_shape(shape.begin(), shape.end());

    // The strides of the NumPy object
    std::vector<py::ssize_t> numpy_stride(numpy_shape.size(), 0);
    for (std::size_t i = 0; i < numpy_shape.size(); i++) {
        numpy_stride[i] = std::accumulate(
            shape.crbegin(), shape.crbegin() + i, sizeof(double), std::multiplies {}
        );
    }
    std::reverse(numpy_stride.begin(), numpy_stride.end());

    // Resulting `NumPy` array of float64
    py::array_t<double, py::array::c_style> result(numpy_shape, numpy_stride);

    double* numpy_data = result.mutable_data();
    for (std::size_t i = 0; i < fold_shape(); i++) {
        const auto apy_f = APyFloat(data.at(i), exp_bits, man_bits, bias);
        numpy_data[i] = double(apy_f);
    }

    return result;
}

bool APyFloatArray::is_identical(const APyFloatArray& other) const
{
    const bool same_spec = (shape == other.shape) && (exp_bits == other.exp_bits)
        && (man_bits == other.man_bits) && (bias == other.bias)
        && (data.size() == other.data.size());

    return same_spec
        && std::equal(data.begin(), data.end(), other.data.begin(), other.data.end());
}

APyFloatArray APyFloatArray::from_double(
    const pybind11::sequence& double_seq,
    std::uint8_t exp_bits,
    std::uint8_t man_bits,
    std::optional<exp_t> bias,
    std::optional<RoundingMode> rounding_mode
)
{

    APyFloatArray arr(
        python_sequence_extract_shape(double_seq), exp_bits, man_bits, bias
    );

    auto py_obj = python_sequence_walk<py::float_, py::int_>(double_seq);

    for (const auto& obj : py_obj) {
        double d;
        if (py::isinstance<py::float_>(obj)) {
            d = static_cast<double>(py::cast<py::float_>(obj));
        } else if (py::isinstance<py::int_>(obj)) {
            d = static_cast<int>(py::cast<py::int_>(obj));
        } else {
            throw std::domain_error("Invalid Python objects in sequence");
        }
        const auto fp = APyFloat::from_double(d, exp_bits, man_bits, arr.bias);
        arr.data.push_back({ fp.get_sign(), fp.get_exp(), fp.get_man() });
    }
    return arr;
}

APyFloatArray APyFloatArray::transpose() const
{
    if (get_ndim() > 2) {
        throw NotImplementedException(fmt::format(
            "Not implemented: high-dimensional (ndim={} > 2) tensor transposition",
            get_ndim()
        ));
    } else if (get_ndim() <= 1) {
        // Behave like `NumPy`, simply return `*this` if single-dimensional
        return *this;
    }

    // Resulting array with shape dimensions
    APyFloatArray result(shape, exp_bits, man_bits, bias);
    std::reverse(result.shape.begin(), result.shape.end());

    // Copy data
    result.data.resize(data.size());
    for (std::size_t y = 0; y < shape[0]; y++) {
        for (std::size_t x = 0; x < shape[1]; x++) {
            result.data[x * shape[0] + y] = data[y * shape[1] + x];
        }
    }

    return result;
}

std::size_t APyFloatArray::fold_shape() const
{
    // Fold the shape over multiplication
    return std::accumulate(shape.begin(), shape.end(), 1, std::multiplies {});
}