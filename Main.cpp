#include <stack>
//#include <functional>
#include <iostream>
#include <complex>

#include "Parser.h"
#include "Token.h"

int main()
{
	using namespace std::complex_literals;
	std::string expr = "f(3.14159 / 4) + i*f(3.14159 / 3)";
	std::string expr2 = "i*(2 + i5)^2";
	std::string expr3 = "(((g(PI, i*f(PI/4)^2))))";
	std::string expr4 = "1+2)*(1+2*(4+5 + 3";

	typedef std::complex<double> T;
	//typedef double T;

	auto f = [](T z)->T { return sin(z); };
	auto g = [](T z1, T z2)->T { return z1*z1 + z2*z2; };

	Parser<T> P;
	P.RecognizeToken(new SymbolConst<T>(T(0,1), "i"));
	P.RecognizeToken(new SymbolFunc1<T>(f, "f"));
	P.Parse(expr2);
	P.eval();
	P.Parse(expr);
	Parser<T> Q;
	Q.RecognizeToken(new SymbolConst<T>(T(0, 1), "i"));
	Q.Parse(expr2);
	Parser<T> R;
	R.RecognizeToken(new SymbolFunc1<T>(f, "f"));
	R.RecognizeToken(new SymbolFunc2<T>(g, "g"));
	R.RecognizeToken(new SymbolConst<T>(T(0, 1), "i"));
	R.RecognizeToken(new SymbolConst<T>(3.14159, "PI"));
	R.Parse(expr3);
	std::cout << P.eval() << "\n" << Q.eval() << "\n" << R.eval() << "\n";
	R.Parse(expr4);
	std::cout << R.eval();
	return 0;
}