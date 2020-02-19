#pragma once
#include <functional>
#include <complex>
#include <array>

template<typename T>
class Parser;
template<typename T>
class SymbolError;
template<typename T>
class SymbolNum;

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
	sym_comma = -1
};

// Base class for parsed symbols. Symbol pointers are stored in a vector, and
// each symbol has a pointer "parent" to that vector. Symbols recursively
// evaluate themselves, pulling their arguments from the vector. Any valid
// calculation will return a SymbolNum<T> unique pointer.
template<typename T>
class Symbol
{
	 
public:
	// Various flags and virtual "members" used by the parser.
	virtual int GetPrecedence() = 0;
	virtual std::string GetToken() = 0;
	virtual bool IsLeftAssoc() { return true; }
	virtual bool IsDyad() { return false; }
	virtual bool IsMonad() { return false; }
	virtual bool IsPunctuation() { return false; }

	Symbol() {};
	virtual ~Symbol() {};
	Symbol(Parser<T>* par) : parent(par) {};

	virtual SymbolNum<T> eval() { return SymbolError<T>(); }
	virtual void SetVal(const T& v) {};
	void SetParent(Parser<T>* p) { parent = p; }

	bool leftAssoc = true;
protected:
	Parser<T>* parent = nullptr;
};

// All two-argument operations inherent from this
template<typename T>
class Dyad : public Symbol<T>
{
	 
public:
	virtual SymbolNum<T> Apply(SymbolNum<T> t1, SymbolNum<T> t2) = 0;
	virtual SymbolNum<T> eval()
	{
		try
		{
			if (this->parent->itr < this->parent->GetMinItr() + 1)
				throw std::invalid_argument("Error: Mismatched operations.");
		}
		catch(std::invalid_argument)
		{
			throw;
		}
		SymbolNum<T> s1 = (*(--this->parent->itr))->eval();
		try
		{
			if (this->parent->itr < this->parent->GetMinItr() + 1)
				throw std::invalid_argument("Error: Mismatched operations.");
		}
		catch (std::invalid_argument)
		{
			throw;
		}
		SymbolNum<T> s2 = (*(--this->parent->itr))->eval();
		return Apply(std::move(s1), std::move(s2));
	}
	bool IsDyad() { return true; }
};

// One-argument operations inherit from this
template<typename T>
class Monad : public Symbol<T>
{
	 
public:
	virtual SymbolNum<T> Apply(SymbolNum<T> t1) = 0;
	virtual SymbolNum<T> eval()
	{
		try
		{
			if (this->parent->itr < this->parent->GetMinItr() + 1)
				throw std::invalid_argument("Error: Mismatched operations.");
		}
		catch (std::invalid_argument)
		{
			throw;
		}
		return Apply(std::move((*(--this->parent->itr))->eval()));
	}
	bool IsMonad() { return true; }
};

// Generates a sequence of integers for use in other templates
template<int... Is>
struct seq {};
template<int N, int... Is>
struct int_seq : int_seq<N - 1, N - 1, Is...> {};
template<int... Is>
struct int_seq<0, Is...> : seq<Is...> {};

// Helper function overload of callByArray
template<typename T, typename... Ts, int... Is>
T callByArray(std::function<T(Ts...)> f, std::array<T,
	sizeof...(Ts)>& arguments, seq<Is...>)
{
	return f(arguments[Is]...);
}

// Calls a function with elements of an array pasted into
// each argument slot.
template<typename T, typename... Ts>
T callByArray(std::function<T(Ts...)> f,
	std::array<T, sizeof...(Ts)>& arguments)
{
	return callByArray(f, arguments, int_seq<sizeof...(Ts)>());
}

// Template class for functions of arbitrary argument count.
// All data types must be the same, for now.
template<typename T, typename... Ts>
class SymbolFunc : public Symbol<T>
{
	 
public:
	SymbolFunc() {};
	SymbolFunc(const std::function<T(Ts...)>& g, const std::string& s) :
		f(g), name(s) {};

	std::function <T(Ts...)> f;
	virtual SymbolNum<T> Apply(std::array<T, sizeof...(Ts)>& args)
	{
		return SymbolNum<T>(callByArray(f, args));
	}
	virtual SymbolNum<T> eval()
	{
		std::array<T, sizeof...(Ts)> s;
		for (size_t i = 0; i < sizeof...(Ts); i++)
		{
			try
			{
				if (this->parent->itr < this->parent->GetMinItr() + 1)
					throw std::invalid_argument("Error: Mismatched operations.");
			}
			catch (std::invalid_argument)
			{
				throw;
			}
			s[i] = (*(--this->parent->itr))->eval().getVal();
		}
		return Apply(s);
	}
	virtual int GetPrecedence() { return sym_func; }
	virtual std::string GetToken() { return name; }
private:
	std::string name = "f";	
};

// Used for parsing strings. Should never make it to the output queue
template<typename T>
class SymbolLParen : public Symbol<T>
{
public:
	virtual int GetPrecedence() { return sym_lparen; }
	virtual std::string GetToken() { return "("; }
	virtual bool IsPunctuation() { return true; }
};

// Used for parsing strings. Should never make it to the output queue
template<typename T>
class SymbolRParen : public Symbol<T>
{
public:
	virtual int GetPrecedence() { return sym_rparen; }
	virtual std::string GetToken() { return ")"; }
	virtual bool IsPunctuation() { return true; }
};

// Used for intermediate calculations and parsed numbers
template<typename T>
class SymbolNum : public Symbol<T>
{
	 
public:
	SymbolNum() : Symbol<T>() {}
	SymbolNum(const T& v) : val(v) {}
	SymbolNum(const SymbolNum<T>&& S) { val = S.val; }
	SymbolNum(const SymbolNum<T>* ptr) { val = ptr->val; }

	virtual int GetPrecedence() { return sym_num; }
	virtual std::string GetToken() { return ""; }
	virtual T getVal() { return val; }
	virtual void SetVal(const T& v) { }
	virtual SymbolNum<T> eval() { return SymbolNum<T>(this); }
protected:
	T val;
};

// Number but with stored name and value can be set after parsing.
template<typename T>
class SymbolVar : public SymbolNum<T>
{
public:
	SymbolVar(std::string s, T v) : name(s), SymbolNum<T>(v) {}
	virtual int GetPrecedence() { return sym_num; }
	virtual std::string GetToken() { return name; }
	virtual void SetVal(const T& v) { this->val = v; }
private:
	std::string name;
};

// Number with name but value can't be changed.
template<typename T>
class SymbolConst : public SymbolNum<T>
{
public:
	SymbolConst(std::string s, T v) : name(s), SymbolNum<T>(v) {}
	virtual int GetPrecedence() { return sym_num; }
	virtual std::string GetToken() { return name; }
private:
	std::string name;
};

// Separator. Mainly exists because people expect to type it between
// Function arguments. Also separates numbers from dyadic operations,
// e.g. allowing a minus sign to be interpreted as a negative sign.
template<typename T>
class SymbolComma : public Monad<T>
{
	 
public:
	SymbolComma() {};
	virtual int GetPrecedence() { return sym_comma; }
	virtual std::string GetToken() { return ","; }
	virtual bool IsPunctuation() { return true; }
	virtual SymbolNum<T> Apply(SymbolNum<T> t1) { return this->eval(); }
};

template<typename T>
class SymbolError : public SymbolNum<T>
{
public:
	SymbolError() {};
	virtual int GetPrecedence() { return -100; }
	virtual std::string GetToken() { return "error"; }
};

template<typename T>
class SymbolAdd : public Dyad<T>
{
	 
public:
	virtual int GetPrecedence() { return sym_add; }
	virtual std::string GetToken() { return "+"; }
	virtual SymbolNum<T> Apply(SymbolNum<T> t1, SymbolNum<T> t2)
	{
		return SymbolNum<T>(t2.getVal() + t1.getVal());
	}
};

template<typename T>
class SymbolSub : public Dyad<T>
{
	 
public:
	virtual int GetPrecedence() { return sym_sub; }
	virtual std::string GetToken() { return "-"; }
	virtual SymbolNum<T> Apply(SymbolNum<T> t1, SymbolNum<T> t2)
	{
		return SymbolNum<T>(t2.getVal() - t1.getVal());
	}
};

template<typename T>
class SymbolMul : public Dyad<T>
{
	 
public:
	virtual int GetPrecedence() { return sym_mul; }
	virtual std::string GetToken() { return "*"; }
	virtual SymbolNum<T> Apply(SymbolNum<T> t1, SymbolNum<T> t2)
	{
		return SymbolNum<T>(t2.getVal() * t1.getVal());
	}
};

template<typename T>
class SymbolDiv : public Dyad<T>
{
	 
public:
	virtual int GetPrecedence() { return sym_div; }
	virtual std::string GetToken() { return "/"; }
	virtual SymbolNum<T> Apply(SymbolNum<T> t1, SymbolNum<T> t2)
	{
		return SymbolNum<T>(t2.getVal() / t1.getVal());
	}
};

template<typename T>
class SymbolPow : public Dyad<T>
{
	 
public:
	virtual int GetPrecedence() { return sym_pow; }
	virtual bool IsLeftAssoc() { return false; }
	virtual std::string GetToken() { return "^"; }
	virtual SymbolNum<T> Apply(SymbolNum<T> t1, SymbolNum<T> t2)
	{
		return SymbolNum<T>(pow(t2.getVal(), t1.getVal()));
	}
};

template<typename T>
class SymbolNeg : public Monad<T>
{
	 
public:
	virtual int GetPrecedence() { return sym_neg; }
	virtual std::string GetToken() { return "~"; }
	virtual SymbolNum<T> Apply(SymbolNum<T> t1)
	{
		return SymbolNum<T>(-t1.getVal());
	}
};