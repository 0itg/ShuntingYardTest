#include <stack>
//#include <functional>
#include <iostream>
#include <complex>

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

	Parser<T> R;
	R.RecognizeToken(new SymbolFunc<T,T>(f, "f"));
	R.RecognizeToken(new SymbolFunc<T,T,T>(g, "g"));
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
	auto h = [](T z1, T z2, T z3)->T { return z1*z2*z3/(z1+z2+z3); };
	//R.RecognizeToken(new SymbolFunc<T, T, T, T>(h, "h"));
	R.RecognizeFunc((std::function<T(T,T,T)>) h, "h");
	std::cout << R.Parse("h(2,4,7)") << "\n";
	return 0;
}