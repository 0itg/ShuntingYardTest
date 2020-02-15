#include <stack>
//#include <functional>
#include <iostream>
#include <complex>

#include "Parser.h"
#include "Token.h"

int main()
{
	using namespace std::complex_literals;
	std::string expr1 = "f(3.14159 / 4) + i*f(3.14159 / 3)";
	std::string expr2 = "i*(2 + i5)^2";
	std::string expr3 = "(((g(PI, i*f(PI/4)^2))))";
	std::string expr4 = "1+2)*(1+2*(4+5 + 3";
	std::string expr5 = "z^2+5zz+1";

	typedef std::complex<double> T;
	//typedef double T;

	auto f = [](T z)->T { return sin(z); };
	auto g = [](T z1, T z2)->T { return z1*z1 + z2*z2; };

	Parser<T> R;
	R.RecognizeToken(new SymbolFunc1<T>(f, "f"));
	R.RecognizeToken(new SymbolFunc2<T>(g, "g"));
	R.RecognizeToken(new SymbolVar<T>("i", T(0, 1)));
	R.RecognizeToken(new SymbolVar<T>("PI", 3.14159));
	R.Parse(expr3);
	std::cout << R.Parse(expr1) << "\n" << R.Parse(expr2)
		<< "\n" << R.Parse(expr3) << "\n" << R.Parse(expr4) << "\n";

		R.Parse(expr5);
	R.setVariable("z", 2);
	R.setVariable("zz", T(0,1));
	std::cout << R.eval() << "\n";
	R.setVariable("z", T(3,1));
	std::cout << R.eval() << "\n";

	R.setVariable("a", 5);
	R.setVariable("b", 7.1);
	try
	{
	std::cout << R.Parse("a ^ 3 + b^(-1) + g(-z, -a)") << "\n";
	std::cout << R.Parse("b^-1)") << "\n";
	}
	catch (std::invalid_argument msg)
	{
		std::cout << msg.what();
	}
	return 0;
}