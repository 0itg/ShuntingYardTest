#pragma once
#include <functional>
#include <complex>

template<typename T>
class Parser;
template<typename T>
class SymbolError;

// Precedence ranks which can also serve as identifiers
enum precedence_list
{
	sym_num  = 0,
	sym_func = 1,
	sym_pow  = 2,
	sym_mul  = 3,
	sym_div  = 3,
	sym_neg  = 3,
	sym_add  = 4,
	sym_sub  = 4,
	sym_lparen = -1,
	sym_rparen = 100,
};

template<typename T>
class Symbol
{
public:
	virtual int GetType() { return sym_num; }
	virtual std::string GetToken() { return ""; }
	virtual bool IsLeftAssoc() { return true; }

	virtual bool IsDyad() { return false; }
	virtual bool IsMonad() { return false; }

	Symbol() { parent = nullptr; };
	Symbol(T v) : val(v) {};

	virtual Symbol<T>* eval() { return this; }
	virtual T getVal() { return val; }
	void SetParent(Parser<T>* p) { parent = p; }

	bool leftAssoc = true;
protected:
	T val = 0;
	Parser<T>* parent = nullptr;
};

template<typename T>
class Dyad : public Symbol<T>
{
public:
	virtual Symbol<T>* eval(Symbol<T>* t1, Symbol<T>* t2) = 0;
	virtual Symbol<T>* eval()
	{
		try
		{
			if (this->parent->itr < this->parent->GetMinItr() + 1)
				throw "error";
		}
		catch(...)
		{
			std::cout << "Error: Mismatched operations\n";
			return new SymbolError<T>;
		}
		Symbol<T>* s1 = (*(--this->parent->itr))->eval();
		try
		{
			if (this->parent->itr < this->parent->GetMinItr() + 1)
				throw "error";
		}
		catch (...)
		{
			std::cout << "Error: Mismatched operations\n";
			return new SymbolError<T>;
		}
		Symbol<T>* s2 = (*(--this->parent->itr))->eval();
		return eval(s1, s2);
		//return eval((*(--this->parent->i))->eval(),
		//(*(--this->parent->i))->eval());
	}
	bool IsDyad() { return true; }
};

template<typename T>
class Monad : public Symbol<T>
{
	virtual Symbol<T>* eval(Symbol<T>* t1) = 0;
	virtual Symbol<T>* eval()
	{
		try
		{
			if (this->parent->itr < this->parent->GetMinItr() + 1)
				throw "error3";
		}
		catch (...)
		{
			std::cout << "Error: Mismatched operations\n";
			return new SymbolError<T>;
		}
		return eval((*(--this->parent->itr))->eval());
	}
	bool IsMonad() { return true; }
};

// Used for parsing strings. Should never make it to the output queue
template<typename T>
class SymbolLParen : public Symbol<T>
{
	virtual int GetType() { return sym_lparen; }
	virtual std::string GetToken() { return "("; }
	virtual Symbol<T>* eval(Symbol<T>* t1) { return t1; }
};

// Used for parsing strings. Should never make it to the output queue
template<typename T>
class SymbolRParen : public Symbol<T>
{
	virtual int GetType() { return sym_rparen; }
	virtual std::string GetToken() { return ")"; }
	virtual Symbol<T>* eval(Symbol<T>* t1) { return t1; }
};

template<typename T>
class SymbolConst : public Symbol<T>
{
public:
	std::string name;
	SymbolConst(T v, std::string s) : name(s), Symbol<T>(v) {}
private:
	virtual std::string GetToken() { return name; }
	virtual Symbol<T>* eval(Symbol<T>* t1) { return t1; }
};

template<typename T>
class SymbolError : public Symbol<T>
{
	virtual std::string GetToken() { return "error"; }
	//virtual Symbol<T>* eval(Symbol<T>* t1) { return t1; }
	//virtual Symbol<T>* eval(Symbol<T>* t1, Symbol<T>* t2) { return t1; }
};

template<typename T>
class SymbolAdd : public Dyad<T>
{
	virtual int GetType() { return sym_add; }
	virtual std::string GetToken() { return "+"; }
	virtual Symbol<T>* eval(Symbol<T>* t1, Symbol<T>* t2)
	{
		return new Symbol<T>(t2->getVal() + t1->getVal());
	}
};

template<typename T>
class SymbolSub : public Dyad<T>
{
	virtual int GetType() { return sym_sub; }
	virtual std::string GetToken() { return "-"; }
	virtual Symbol<T>* eval(Symbol<T>* t1, Symbol<T>* t2)
	{
		return new Symbol<T>(t2->getVal() - t1->getVal());
	}
};

template<typename T>
class SymbolMul : public Dyad<T>
{
	virtual int GetType() { return sym_mul; }
	virtual std::string GetToken() { return "*"; }
	virtual Symbol<T>* eval(Symbol<T>* t1, Symbol<T>* t2)
	{
		return new Symbol<T>(t2->getVal() * t1->getVal());
	}
};

template<typename T>
class SymbolDiv : public Dyad<T>
{
	virtual int GetType() { return sym_div; }
	virtual std::string GetToken() { return "/"; }
	virtual Symbol<T>* eval(Symbol<T>* t1, Symbol<T>* t2)
	{
		return new Symbol<T>(t2->getVal() / t1->getVal());
	}
};

template<typename T>
class SymbolPow : public Dyad<T>
{
	virtual int GetType() { return sym_pow; }
	virtual bool IsLeftAssoc() { return false; }
	virtual std::string GetToken() { return "^"; }
	virtual Symbol<T>* eval(Symbol<T>* t1, Symbol<T>* t2)
	{
		return new Symbol<T>(pow(t2->getVal(), t1->getVal()));
	}
};

template<typename T>
class SymbolNeg : public Monad<T>
{
	virtual int GetType() { return sym_neg; }
	virtual std::string GetToken() { return "~"; }
	virtual Symbol<T>* eval(Symbol<T>* t1)
	{
		return new Symbol<T>(-t1->getVal());
	}
};

template<typename T>
class SymbolFunc2 : public Dyad<T> 
{
public:
	SymbolFunc2() {};
	SymbolFunc2(std::function<T(T, T)> g, std::string s) : f(g), name(s) {};
	std::string name = "f";
	std::function<T(T, T)> f = [&](T, T) { return this->val; };
private:
	virtual int GetType() { return sym_func; }
	virtual std::string GetToken() { return name; }
	virtual Symbol<T>* eval(Symbol<T>* t1, Symbol<T>* t2)
	{
		return new Symbol<T>(f(t2->getVal(), t1->getVal()));
	}
};

template<typename T>
class SymbolFunc1 : public Monad<T>
{
public:
	SymbolFunc1() {};
	SymbolFunc1(std::function<T(T)> g, std::string s) : f(g), name(s) {};
	std::string name = "f";
	std::function<T(T)> f = [&](T) { return this->val; };
private:
	virtual int GetType() { return sym_func; }
	virtual std::string GetToken() { return name; }
	virtual Symbol<T>* eval(Symbol<T>* t1)
	{
		return new Symbol<T>(f(t1->getVal()));
	}
};

// Intended for use with std::complex<U>.
//template<typename T>
//class Symbol_i : public Monad<T>
//{
//	virtual int GetType() { return sym_mul; }
//	virtual std::string GetToken() { return "i"; }
//	T val = T(0, 1);
//	virtual Symbol<T>* eval(Symbol<T>* t1)
//	{
//		return new Symbol<T>(t1->getVal() * T(0, 1));
//	}
//};

//#define TypedefTokens(T)      \
//typedef Symbol<T> Num;        \
//typedef SymbolAdd<T> Add;     \
//typedef SymbolSub<T> Sub;     \
//typedef SymbolMul<T> Mul;     \
//typedef SymbolDiv<T> Div;     \
//typedef SymbolPow<T> Pow;     \
//typedef SymbolNeg<T> Neg;     \
//typedef SymbolFunc2<T> Func2; \
//typedef SymbolFunc1<T> Func1; \
//typedef SymbolLParen<T> LPar; \
//typedef SymbolRParen<T> RPar;
