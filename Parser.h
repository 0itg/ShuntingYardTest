#pragma once
#include "Token.h"

#include <map>
#include <sstream>
#include <vector>
#include <cmath>

template<typename T>
class Symbol;

struct cmp_length_then_alpha;

template<typename T>
class Parser
{
public:
	Parser();
	~Parser();
	void RecognizeToken(Symbol<T>* sym)
	{
		tokens[sym->GetToken()] = sym;
	}
	T eval()
	{
		itr = out.end() - 1;
		return (*itr)->eval()->getVal();
	};
	template<typename... Ts>
	void RecognizeFunc(const std::function<T(Ts...)>& f,
		const std::string& name)
	{
		RecognizeToken(new SymbolFunc<T, Ts...>(f, name));
	}
	void setVariable(const std::string& name, const T& val);
	std::string str() { return lastValidInput; }
	T Parse(std::string str);
	void Revert() { Parse(lastValidInput); }

	typename std::vector<Symbol<T>*>::iterator itr;
	auto GetMinItr() { return out.begin(); }
	T operator()(T val);
private:
	std::string lastValidInput = "";
	void PushToken(Symbol<T>* token)
	{
		token->SetParent(this);
		out.push_back(token);
	}
	void PopToken()
	{
		out.pop_back();
	}
	// Custom comparator puts longest tokens first. When tokenizing the input,
	// replacing the longest ones first prevents them being damaged when
	// they contain shorter tokens.
	std::map<std::string, Symbol<T>*, cmp_length_then_alpha> tokens;
	std::vector<Symbol<T>*> out;
};

template<typename T>
inline Parser<T>::Parser()
{
	RecognizeToken(new SymbolAdd<T>);
	RecognizeToken(new SymbolSub<T>);
	RecognizeToken(new SymbolMul<T>);
	RecognizeToken(new SymbolDiv<T>);
	RecognizeToken(new SymbolPow<T>);
	RecognizeToken(new SymbolNeg<T>);
	RecognizeToken(new SymbolLParen<T>);
	RecognizeToken(new SymbolRParen<T>);
	RecognizeToken(new SymbolComma<T>);
	RecognizeToken(new SymbolConst<T>("pi", acos(-1)));
	RecognizeToken(new SymbolConst<T>("e", exp(1)));
	RecognizeToken(new SymbolConst<T>("i", T(0,1)));

	RecognizeFunc((std::function<T(T)>)[](T z) {return exp(z); }, "exp");
	RecognizeFunc((std::function<T(T)>)[](T z) {return log(z); }, "log");
	RecognizeFunc((std::function<T(T)>)[](T z) {return sqrt(z); }, "sqrt");
	RecognizeFunc((std::function<T(T)>)[](T z) {return sin(z); }, "sin");
	RecognizeFunc((std::function<T(T)>)[](T z) {return cos(z); }, "cos");
	RecognizeFunc((std::function<T(T)>)[](T z) {return tan(z); }, "tan");
	RecognizeFunc((std::function<T(T)>)[](T z) {return sinh(z); }, "sinh");
	RecognizeFunc((std::function<T(T)>)[](T z) {return cosh(z); }, "cosh");
	RecognizeFunc((std::function<T(T)>)[](T z) {return tanh(z); }, "tanh");
	RecognizeFunc((std::function<T(T)>)[](T z) {return asin(z); }, "asin");
	RecognizeFunc((std::function<T(T)>)[](T z) {return acos(z); }, "acos");
	RecognizeFunc((std::function<T(T)>)[](T z) {return atan(z); }, "atan");
	RecognizeFunc((std::function<T(T)>)[](T z) {return asinh(z); }, "asinh");
	RecognizeFunc((std::function<T(T)>)[](T z) {return acosh(z); }, "acosh");
	RecognizeFunc((std::function<T(T)>)[](T z) {return atanh(z); }, "atanh");
}

template<typename T>
inline Parser<T>::~Parser()
{
	for (auto S : tokens) delete S.second;
}

template<typename T>
inline void Parser<T>::setVariable(const std::string& name, const T& val)
{
	if (tokens.find(name) == tokens.end())
	{
		tokens[name] = new SymbolVar<T>(name, val);
	}
	else tokens[name]->SetVal(val);
}

template<typename T>
T Parser<T>::Parse(std::string input)
{
	std::vector<Symbol<T>*> opStack;
	out.clear();

	auto PushOp = [&](Symbol<T>* token)
	{
		token->SetParent(this);
		opStack.push_back(token);
	};

	auto replaceAll = [](std::string& s, const std::string& toReplace,
		const std::string& replaceWith)
	{
		std::size_t pos = 0;
		while (pos != std::string::npos)
		{
			pos = s.find(toReplace, pos);
			if (pos != std::string::npos)
			{
				s.replace(pos, toReplace.length(), replaceWith);
				pos += replaceWith.length();
			}
		}
		 return s;
	};

	// Add white space between tokens for easier stringstream processing
	size_t pos = 0;
	std::string inputCopy = input;
	while (pos != std::string::npos)
	{
		pos = inputCopy.find_first_not_of(
			"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890_ ", pos);
		if (pos != std::string::npos)
		{
			std::string y(1,input[pos]);
			input.replace(pos, 1, " " + y + " ");
			inputCopy.replace(pos, 1, "   ");
		}
	}
	//std::string inputCopy = input;
	//for (auto tok : tokens)
	//{
	//	if (inputCopy.find(tok.first) != std::string::npos)
	//	{
	//		replaceAll(inputCopy, tok.first, std::string(tok.len," "));
	//		replaceAll(input, tok.first, " " + tok.first + " ");
	//	}
	//}

	std::string s;
	std::stringstream strPre(input);

	std::vector<std::string> tokenVec;

	// Convert string to vector of tokens.
	// Easier to manage special exceptions this way.

	while (strPre >> s)
	{
		T t;
		std::string name;
		std::stringstream ss(s);
		// If first char is a number, read in the whole number.
		// If anything remains, It is an unrecognized token, 
		// so call it a variable and set it to zero.
		if (isdigit(s[0]))
		{
			ss >> t;
			// Add "\\" to name to prevent issues tokenizing in the future
			// if the parser is reused without clearing mapped tokens.
			s = "\\" + s;
			if (tokens.find(s) == tokens.end())
				tokens[s] = new SymbolNum<T>(t);
			tokenVec.push_back(s);
			if (ss >> s)
			{
				if (tokens.find(s) == tokens.end())
				{
					tokens[s] = new SymbolVar<T>(s, 0);
					tokenVec.push_back(s);
				}
				else tokenVec.push_back(s);
			}
		}
		else if (tokens.find(s) == tokens.end())
		{
			tokens[s] = new SymbolVar<T>(s, 0);
			tokenVec.push_back(s);
		}
		else tokenVec.push_back(s);
	}

	for (size_t i = 0; i < tokenVec.size(); i++)
	{
		// Special rule: '-' (sub) should be read as '~' (neg) if subraction
		// wouldn't work, i.e. if the left token is dyadic or doesn't exist.
		if (tokenVec[i] == "-")
		{
			if (i > 0)
			{
				if (tokens[tokenVec[i - 1]]->IsPunctuation())
				{
					tokenVec[i] = "~";
				}
			}
			else if (i == 0)
			{
				tokenVec[i] = "~";
			}
		}

		// Special rule: implied multiplication between constants and numbers,
		// e.g. 3i, 2PI.
		if (tokens[tokenVec[i]]->GetPrecedence() == sym_num)
		{
			int inserted = 0;
			if (i > 0)
			{
				if  (tokenVec[i - 1][0] == '\\')
				{
					tokenVec.insert(tokenVec.begin() + i, "*");
					inserted++;
				}
			}
			if (i < tokenVec.size() - 1)
			{
				if  (tokenVec[i + 1][0] == '\\')
				{
					tokenVec.insert(tokenVec.begin() + i + 1 + inserted, "*");
				}
			}
		}
	}

	// Check for errors
	try
	{
		if (tokenVec.empty()) throw std::invalid_argument(
			"Error: Empy expression.");
		if (tokens[tokenVec[0]]->IsDyad() || tokens[tokenVec.back()]->IsDyad())
		{
			throw std::invalid_argument(
				"Error: Expression begins or ends with dyad.");
		}
		if (tokenVec.size() > 1)
		{
			for (auto it = tokenVec.begin() + 1; it != tokenVec.end() - 1; it++)
			{
				if ((*it)[0] == '\\')
				{
					if (tokens[*it]->IsDyad())
					{
						if (tokens[*(it - 1)]->IsDyad() ||
							tokens[*(it + 1)]->IsDyad())
						{
							throw std::invalid_argument(
								"Error: Two adjacent dyads.");
						}
					}
				}
			}
		}
	}
	catch (std::string msg)
	{
		std::cout << msg << "\n";
		throw msg;
	}

	for (auto s : tokenVec)
	{
		std::stringstream s1(s);
		std::string op;
		s1 >> op;

		// Lower precedence means earlier in the order of operations
		int tokPrec = tokens[op]->GetPrecedence();

		// First op always goes to the op stack. Left paren is given lowest
		// precedence, so it is always added.
		if (opStack.empty() || tokPrec < opStack.back()->GetPrecedence() ||
				(!tokens[op]->IsLeftAssoc() &&
					tokPrec == opStack.back()->GetPrecedence()))
			PushOp(tokens[op]);
		else if (tokPrec >= opStack.back()->GetPrecedence())
		{
			// Right paren means, assuming it matches with a left, everything on the
			// stack goes to the queue, because it all evaluates to an input for the
			// next op.
			if (tokPrec == sym_rparen)
			{
				while (opStack.back()->GetPrecedence() != sym_lparen)
				{
					out.push_back(opStack.back());
					opStack.pop_back();
					try
					{
						if (opStack.empty())
						{
							throw std::invalid_argument("Warning: Mismatched parentheses. Attempting to fix.\n");
						}
					}
					catch (std::invalid_argument msg)
					{
						PushOp(tokens["("]);
						std::cout << msg.what();
					}
				}
				opStack.pop_back();
			}
			// Similarly, anything with lower precedence than the current token
			// Must form an input to that token, so move it all to the output queue.
			else
			{
				while (!opStack.empty() &&
						opStack.back()->GetPrecedence() != sym_lparen && 
						opStack.back()->GetPrecedence() <= tokPrec)
				{
					out.push_back(opStack.back());
					opStack.pop_back();
				}
				PushOp(tokens[op]);
			}
			//}
		}
	}
	// To finish, pop whatever is on the op stack to the output queue.
	while (opStack.size() > 0)
	{
		out.push_back(opStack.back());
		if (out.back()->GetPrecedence() == sym_lparen) out.pop_back();
		opStack.pop_back();
	}

	T result = eval();
	// If no exceptions have been thrown by now, the input should be valid,
	// so store it as the last valid state.
	lastValidInput = input;
	return result;
}

template<typename T>
inline T Parser<T>::operator()(T val)
{
	setVariable("z", val);
	return eval();
}

struct cmp_length_then_alpha {
	bool operator()(const std::string & a, const std::string & b) const {
		if (a.length() != b.length())
			return a.length() > b.length();
		else
			return a < b;
	}
};