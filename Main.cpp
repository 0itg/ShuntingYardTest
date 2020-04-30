#include <stack>
//#include <functional>
#include <iostream>
#include <complex>
#include <ctime>

#include "Parser.h"
#include "Token.h"

int main()
{
	using namespace std::complex_literals;
	std::string expr1 = "f(pi / 4) + i*f(pi / 3)";
	std::string expr2 = "i*(2 + 5i)^2";
	std::string expr3 = "g(pi, i*f(pi/4)^2)";
	std::string expr4 = "1+2)*(1+2*(4+5 + 3";
	std::string expr5 = "z^2+z+1";

	typedef std::complex<double> T;
	//typedef double T;

	auto f = [](T z)->T { return sin(z); };
	auto g = [](T z1, T z2)->T { return z1*z1 + z2*z2; };

	auto func = Parser<T>().Parse(expr1);

	std::cout << func.eval();

	return 0;
}