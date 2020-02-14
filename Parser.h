#pragma once
#include "Token.h"

#include <stack>
#include <map>
#include <sstream>

template<typename T>
class Symbol;

struct cmp_length_then_alpha;

template<typename T>
class Parser
{
public:
	Parser();

	void RecognizeToken(Symbol<T>* sym)
	{
		tokens[sym->GetToken()] = sym;
	}
	T eval()
	{
		itr = out.end() - 1;
		return (*itr)->eval()->getVal();
	};
	void setVariable(std::string name, T val);
	T Parse(std::string str);
	typename std::vector<Symbol<T>*>::iterator itr;


	auto GetMinItr() { return out.begin(); }
private:
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
	RecognizeToken(new SymbolFunc1<T>);
	RecognizeToken(new SymbolFunc2<T>);
	RecognizeToken(new SymbolLParen<T>);
	RecognizeToken(new SymbolRParen<T>);
}

template<typename T>
inline void Parser<T>::setVariable(std::string name, T val)
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
	for (auto tok : tokens)
	{
		replaceAll(input, tok.first, " " + tok.first + " ");
	}

	replaceAll(input, ",", "");

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
				if (!tokens[tokenVec[i - 1]]->IsMonad())
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
		if (tokens[tokenVec[0]]->IsDyad() || tokens[tokenVec.back()]->IsDyad())
		{
			throw std::string("Error: Expression begins or ends with dyad");
		}
		for (auto it = tokenVec.begin() + 1; it != tokenVec.end() - 1; it++)
		{
			if ((*it)[0] == '\\')
			{
				if (tokens[*it]->IsDyad())
				{
					if (tokens[*(it - 1)]->IsDyad() ||
						tokens[*(it + 1)]->IsDyad())
					{
						throw std::string("Error: Two adjacent dyads");
					}
				}
			}
		}
	}
	catch (std::string msg)
	{
		std::cout << msg << "\n";
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
							throw "x";
						}
					}
					catch (...)
					{
						PushOp(tokens["("]);
						std::cout << "Warning: Mismatched parentheses. Attempting to fix.\n";
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
		if (out.back()->GetPrecedence() == -1) out.pop_back();
		opStack.pop_back();
	}

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