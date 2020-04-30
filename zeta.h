#pragma once

#include <array>
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <complex>

using int128_t   = boost::multiprecision::int128_t;
using float128_t = boost::multiprecision::cpp_bin_float_quad;

// Constexpr integer power function
template <typename T> constexpr long long int ipow(T num, unsigned int exp)
{
    if (exp >= sizeof(T) * 8)
        return 0;
    else if (exp == 0)
        return 1;
    else
        return num * ipow(num, exp - 1);
}

constexpr int ZETA_TERMS = 32;

// Based on Borwein's algorithm 2.
template <typename T> constexpr T d_k(long n, long k)
{
    T res = 1;
    for (long i = 1; i <= k; i++)
    {
        T fac = 1;
        for (long j = 1; j < 2 * i; j++)
        {
            fac *= (n - i + j) * 2.0 / j;
        }
        res += fac;
    }

    return n * res;
}

template <typename T> constexpr T zetaCoeff(int j)
{
    T res = d_k<T>(ZETA_TERMS, j) - d_k<T>(ZETA_TERMS, ZETA_TERMS);

    if (j % 2) return -res;
    return res;
}

template <typename T, int N> struct zeta_coeff_table
{
    constexpr zeta_coeff_table() : values()
    {
        for (auto i = 0; i < N; ++i)
        {
            values[i] = zetaCoeff<T>(i);
        }
    }
    T values[N];
};

template <typename T> T zeta(T s)
{
    typedef std::complex<long double> U;
    U res = 0;
    U z   = s;

    static constexpr auto zCoeff = zeta_coeff_table<long double, ZETA_TERMS>();
    static constexpr auto d_n    = d_k<long double>(ZETA_TERMS, ZETA_TERMS);

    for (int i = 0; i < ZETA_TERMS; i++)
    {
        res += (U)zCoeff.values[i] / pow(i + 1.0, z);
    }
    res *=
        (U)-1.0 / (d_n * ((U)1 - pow(static_cast<long double>(2), (U)1 - z)));

    if constexpr (std::is_floating_point_v<T>)
        return (T)res.real();
    else
        return (T)res;
}

// Same but using float_128_t. Slow.

// template <typename T> T zeta(T s)
//{
//    typedef std::complex<float128_t> U;
//    U res{0, 0};
//    U z;
//    if constexpr (std::is_arithmetic_v<T>)
//        z = s;
//    else
//        z = U{s.real(), s.imag()};
//
//    static auto zCoeff = zeta_coeff_table<float128_t, ZETA_PRECISION>();
//    static auto d_n    = d_k<float128_t>(ZETA_PRECISION, ZETA_PRECISION);
//
//    for (int i = 0; i < ZETA_PRECISION; i++)
//    {
//        res += (U)zCoeff.values[i] / (U)pow(static_cast<double>(i + 1.0), s);
//    }
//    res *= (U)-1.0 / (d_n * ((U)1 - (U)pow(2.0, (std::complex<double>)1 -
//    s))); if constexpr (std::is_arithmetic_v<T>)
//        return (T)res.real();
//    else
//        return T{static_cast<double>(res.real()),
//                 static_cast<double>(res.imag())};
//    ;
//}

// Based on Borwein's Algorithm 3. Has some problems with diverging from
// The correct value near the negative real axis, but seems to do better than
// algorithm 2 on the critical line.

// template <typename T> constexpr T binomCoeff(int n, int k)
//{
//    T res = 1;
//    for (int i = 1; i < k; i++)
//    {
//        res *= n + 1 - i;
//        res /= i;
//    }
//    return res;
//}

// constexpr long double zetaCoeff(int j)
//{
//    long double res = -1;
//    if (j > ZETA_PRECISION)
//    {
//        int last = j - ZETA_PRECISION;
//
//        for (int k = 0; k <= last; k++)
//        {
//            res += static_cast<long double>(
//                static_cast<float128_t>(binomCoeff<int128_t>(ZETA_PRECISION,
//                k)) / static_cast<float128_t>(pow(2, ZETA_PRECISION)));
//        }
//    }
//    if (j % 2)
//        return -res;
//    return res;
//}
//
// template <int N> struct zeta_coeff_table
//{
//    constexpr zeta_coeff_table() : values()
//    {
//        for (auto i = 0; i < N; ++i)
//        {
//            values[i] = zetaCoeff(i);
//        }
//    }
//    long double values[N];
//};
//
// template <typename T> T zeta(T s)
//{
//    typedef std::complex<long double> U;
//    U res              = 0;
//    U z                = s;
//    constexpr int last = 2 * ZETA_PRECISION - 1;
//
//    static auto zCoeff = zeta_coeff_table<last>();
//
//    for (int i = 0; i < last; i++)
//    {
//        res += (U)zCoeff.values[i] / pow(i + 1.0, z);
//    }
//    res *= (U)-1.0 / ((U)1 - pow(static_cast<long double>(2), (U)1 - z));
//
//    if constexpr (std::is_floating_point_v<T>)
//        return (T)res.real();
//    else
//        return (T)res;
//}