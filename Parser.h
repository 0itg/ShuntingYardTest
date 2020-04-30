#pragma once
#define _USE_MATH_DEFINES
#include "Token.h"
#include "zeta.h"

#include <cmath>
#include <map>
#include <sstream>
#include <vector>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/complex.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/unique_ptr.hpp>
#include <boost/serialization/vector.hpp>

template <typename T> class Symbol;
template <typename T> class ParsedFunc;

typedef std::complex<double> cplx;

struct cmp_length_then_alpha;

// Parser<T> outputs a ParsedFunc object, essentially a self-evaluating stack
// of operations/functions and arguments of type T. Tokens objects can be
// defined and added to the parser separately, provided they inherit from
// Symbol<T>. Any type with typical overloads of the basic operations and
// functions (e.g. taking the usual number of args, returning T) should
// work with the templates in Token.h. Any function T(T...) can be recognized
// as a SymbolFunc<T, Ts...>, or using the mariginally more convenient
// RecognizeFunc(std::function<T(Ts...)>, std::string name),

template <typename T> class Parser
{
    friend class boost::serialization::access;

public:
    Parser();
    void RecognizeToken(Symbol<T>* sym)
    {
        tokenLibrary[sym->GetToken()] = std::unique_ptr<Symbol<T>>(sym);
    }
    template <typename... Args>
    void RecognizeFunc(const std::function<T(Args...)>& f,
                       const std::string& name)
    {
        RecognizeToken(new SymbolFunc<T, Args...>(f, name));
    }
    void Initialize();
    ParsedFunc<T> Parse(std::string str);

private:
    ParsedFunc<T> f;
    std::map<std::string, std::unique_ptr<Symbol<T>>, cmp_length_then_alpha>
        tokenLibrary;

    // Save/Load via Boost.Serialization
    template <class Archive>
    void save(Archive& ar, const unsigned int version) const
    {
        ar << f.inputText;
    }
    template <class Archive> void load(Archive& ar, const unsigned int version)
    {
        std::string savedFunc;
        ar >> savedFunc;
        f = this->Parse(savedFunc);
    }
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        boost::serialization::split_member(ar, *this, version);
    }
};

// Contains the result of Parser<T> parsing a string. SetVariable(name, T)
// sets the numerical value of a named parameter, and eval() calculates the
// result of the expression. Tokens can be added and removed manually with
// PushToken(Symbol<T>*) and PopToken(), if necessary.

template <typename T> class ParsedFunc
{
    friend class boost::serialization::access;
    friend class Parser<T>;

public:
    ParsedFunc(){};
    ParsedFunc(const ParsedFunc&& in) noexcept { *this = std::move(in); }

    ParsedFunc(const ParsedFunc& in) noexcept { *this = in; }

    ParsedFunc& operator=(ParsedFunc&& in) noexcept
    {
        symbolStack = std::move(in.symbolStack);
        tokens      = std::move(in.tokens);
        inputText   = std::move(in.inputText);
        for (auto sym : symbolStack)
        {
            sym->SetParent(this);
        }
        return *this;
    }
    ParsedFunc& operator=(const ParsedFunc& in) noexcept
    {
        this->tokens.clear();
        this->symbolStack.clear();
        std::unique_ptr<Symbol<T>> sym;
        for (auto S : in.symbolStack)
        {
            std::string tok = S->GetToken();
            if (tok.length() > 0)
            {
                if (tokens.find(tok) == tokens.end())
                {
                    sym.reset(S->Clone());
                    sym->SetParent(this);
                    tokens[tok] = std::move(sym);
                }
                symbolStack.push_back(tokens[tok].get());
            }
            else
            {
                sym.reset(S->Clone());
                sym->SetParent(this);
                symbolStack.push_back(sym.get());
            }
        }
        inputText = in.inputText;
        return *this;
    }
    T eval()
    {
        itr = symbolStack.end() - 1;
        return (*itr)->eval().GetVal();
    };

    void SetIV(std::string token) { IV_token = token; }
    std::string GetIV() const { return IV_token; }
    void SetVariable(const std::string& name, const T& val);

    typename std::vector<Symbol<T>*>::iterator itr;

    auto GetMinItr() { return symbolStack.begin(); }
    T operator()(T val);
    void PushToken(Symbol<T>* token)
    {
        Symbol<T>* S;
        if (tokens.find(token->GetToken()) == tokens.end())
        {
            S                         = token->Clone();
            tokens[token->GetToken()] = std::unique_ptr<Symbol<T>>(S);
            S->SetParent(this);
        }
        else
            S = tokens[token->GetToken()].get();
        symbolStack.push_back(S);
    }
    void PopToken() { symbolStack.pop_back(); }
    std::string str() { return inputText; }
    void ReplaceVariable(std::string varOld, std::string var);
    auto GetVars();
    auto GetVar(const std::string& tok)
    {
        return tokens.find(tok) != tokens.end() ? tokens[tok].get() : nullptr;
    }
    auto GetVarMap() const;
    void RestoreVarsFromMap(std::map<std::string, T>);
    std::string GetInputText() const { return inputText; }

private:
    // Custom comparator puts longest tokenLibrary first. When tokenizing the
    // input, replacing the longest ones first prevents them being damaged when
    // they contain shorter tokenLibrary.
    std::map<std::string, std::unique_ptr<Symbol<T>>, cmp_length_then_alpha>
        tokens;
    std::vector<Symbol<T>*> symbolStack;
    std::string inputText = "";
    std::string IV_token  = "z";

    template <class Archive>
    void save(Archive& ar, const unsigned int version) const
    {
        ar << inputText;
        ar << GetVarMap();
        ar << IV_token;
    }
    template <class Archive> void load(Archive& ar, const unsigned int version)
    {
        Parser<T> parser;
        std::string savedFunc;
        ar >> savedFunc;
        std::map<std::string, cplx> varMap;
        ar >> varMap;
        *this = parser.Parse(savedFunc);
        RestoreVarsFromMap(varMap);
        std::string IV;
        ar >> IV;
        SetIV(IV);
    }
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        boost::serialization::split_member(ar, *this, version);
    }
};

template <typename T> inline Parser<T>::Parser() { Initialize(); }

template <typename T> ParsedFunc<T> Parser<T>::Parse(std::string input)
{
    f.symbolStack.clear();
    std::vector<Symbol<T>*> opStack;
    f.inputText = input;

    auto PushOp = [&](Symbol<T>* token) {
        token->SetParent(&f);
        opStack.push_back(token);
    };

    auto replaceAll = [](std::string& s, const std::string& toReplace,
                         const std::string& replaceWith) {
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

    // Add white space between tokenLibrary for easier stringstream processing
    size_t pos            = 0;
    std::string inputCopy = input;
    while (pos != std::string::npos)
    {
        pos = inputCopy.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJ"
                                          "KLMNOPQRSTUVWXYZ01234567890_. ",
                                          pos);
        if (pos != std::string::npos)
        {
            std::string y(1, input[pos]);
            input.replace(pos, 1, " " + y + " ");
            inputCopy.replace(pos, 1, "   ");
        }
    }

    std::string s;
    std::stringstream strPre(input);

    std::vector<std::string> tokenVec;

    // Convert string to vector of tokenLibrary.
    // Easier to manage special exceptions this way.

    while (strPre >> s)
    {
        T t;
        std::string name;
        std::stringstream ss(s);
        // If first char is a number, read in the whole number.
        // If anything remains, It is an unrecognized token,
        // so call it a variable and set it to zero.
        if (isdigit(s[0]) || s[0] == '.')
        {
            ss >> t;
            // Add "\\" to name to prevent issues tokenizing in the future
            // if the parser is reused without clearing mapped tokenLibrary.
            s = "\\" + s;
            if (tokenLibrary.find(s) == tokenLibrary.end())
                tokenLibrary[s] = std::make_unique<SymbolNum<T>>(t);
            tokenVec.push_back(s);
            if (ss >> s)
            {
                if (tokenLibrary.find(s) == tokenLibrary.end())
                {
                    tokenLibrary[s] = std::make_unique<SymbolVar<T>>(s, 0);
                    tokenVec.push_back(s);
                }
                else
                    tokenVec.push_back(s);
            }
        }
        else if (tokenLibrary.find(s) == tokenLibrary.end())
        {
            tokenLibrary[s] = std::make_unique<SymbolVar<T>>(s);
            tokenVec.push_back(s);
        }
        else
            tokenVec.push_back(s);
    }

    for (size_t i = 0; i < tokenVec.size(); i++)
    {
        // Special rule: '-' (sub) should be read as '~' (neg) if subraction
        // wouldn't work, i.e. if the left token is dyadic or doesn't exist.
        if (tokenVec[i] == "-")
        {
            if (i > 0)
            {
                if (tokenLibrary[tokenVec[i - 1]]->GetPrecedence() == -1 ||
                    tokenLibrary[tokenVec[i - 1]]->IsDyad())
                { tokenVec[i] = "~"; }
            }
            else if (i == 0)
            {
                tokenVec[i] = "~";
            }
        }

        int currentTokenPrec = tokenLibrary[tokenVec[i]]->GetPrecedence();

        // Special rule: implied multiplication between constants / variables
        // and numbers, e.g. 3i, 2PI.
        if (currentTokenPrec == sym_num)
        {
            int inserted = 0;
            if (i > 0)
            {
                if (tokenVec[i - 1][0] == '\\')
                {
                    tokenVec.insert(tokenVec.begin() + i, "*");
                    inserted++;
                }
            }
            if (i < tokenVec.size() - 1)
            {
                if (tokenVec[i + 1][0] == '\\')
                { tokenVec.insert(tokenVec.begin() + i + 1 + inserted, "*"); }
            }
        }

        // Implied multiplication between parens, other parens, and numbers.
        else if (currentTokenPrec == sym_lparen && i > 0)
        {
            Symbol<T>* prev = tokenLibrary[tokenVec[i - 1]].get();
            if (prev->GetPrecedence() == sym_num ||
                prev->GetPrecedence() == sym_rparen)
            {
                tokenVec.insert(tokenVec.begin() + i, "*");
                i++;
            }
        }
        else if (currentTokenPrec == sym_rparen && i < tokenVec.size() - 1)
        {
            Symbol<T>* next = tokenLibrary[tokenVec[i + 1]].get();
            if (next->GetPrecedence() == sym_num ||
                next->GetPrecedence() == sym_lparen)
            {
                tokenVec.insert(tokenVec.begin() + i + 1, "*");
                i++;
            }
        }
    }

    // Check for errors
    try
    {
        if (tokenVec.empty())
            throw std::invalid_argument("Error: Empy expression.");
        if (tokenLibrary[tokenVec[0]]->IsDyad() ||
            tokenLibrary[tokenVec.back()]->IsDyad())
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
                    if (tokenLibrary[*it]->IsDyad())
                    {
                        if (tokenLibrary[*(it - 1)]->IsDyad() ||
                            tokenLibrary[*(it + 1)]->IsDyad())
                        {
                            throw std::invalid_argument(
                                "Error: Two adjacent dyads.");
                        }
                    }
                }
            }
        }
    }
    catch (std::invalid_argument& msg)
    {
        std::cout << msg.what() << "\n";
        throw msg;
    }

    for (auto s : tokenVec)
    {
        std::stringstream s1(s);
        std::string op;
        s1 >> op;

        // Lower precedence means earlier in the order of operations
        int tokPrec = tokenLibrary[op]->GetPrecedence();

        // First op always goes to the op stack. Left paren is given lowest
        // precedence, so it is always added.
        if (opStack.empty() || tokPrec < opStack.back()->GetPrecedence() ||
            (!tokenLibrary[op]->IsLeftAssoc() &&
             tokPrec == opStack.back()->GetPrecedence()))
            PushOp(tokenLibrary[op].get());
        else if (tokPrec >= opStack.back()->GetPrecedence())
        {
            // Right paren means, assuming it matches with a left, everything on
            // the stack goes to the queue, because it all evaluates to an input
            // for the next op.
            if (tokPrec == sym_rparen)
            {
                while (opStack.back()->GetPrecedence() != sym_lparen)
                {
                    f.PushToken(opStack.back());
                    opStack.pop_back();
                    try
                    {
                        if (opStack.empty())
                        {
                            throw std::invalid_argument(
                                "Warning: Mismatched parentheses. Attempting "
                                "to "
                                "fix.\n");
                        }
                    }
                    catch (std::invalid_argument& msg)
                    {
                        PushOp(tokenLibrary["("].get());
                        std::cout << msg.what();
                    }
                }
                opStack.pop_back();
            }
            // Similarly, anything with lower precedence than the current token
            // Must form an input to that token, so move it all to the output
            // queue.
            else
            {
                while (!opStack.empty() &&
                       opStack.back()->GetPrecedence() != sym_lparen &&
                       opStack.back()->GetPrecedence() <= tokPrec)
                {
                    f.PushToken(opStack.back());
                    opStack.pop_back();
                }
                PushOp(tokenLibrary[op].get());
            }
        }
    }
    // To finish, pop whatever is on the op stack to the output queue.
    while (opStack.size() > 0)
    {
        f.PushToken(opStack.back());
        if (f.symbolStack.back()->GetPrecedence() == sym_lparen) f.PopToken();
        opStack.pop_back();
    }
    return f;
}

struct cmp_length_then_alpha
{
    bool operator()(const std::string& a, const std::string& b) const
    {
        if (a.length() != b.length())
            return a.length() > b.length();
        else
            return a < b;
    }
};

template <typename T>
inline void ParsedFunc<T>::SetVariable(const std::string& name, const T& val)
{
    if (tokens.find(name) != tokens.end()) tokens[name]->SetVal(val);
}

template <typename T> inline T ParsedFunc<T>::operator()(T val)
{
    SetVariable(IV_token, val);
    return eval();
}

template <typename T>
inline void ParsedFunc<T>::ReplaceVariable(std::string varOld,
                                           std::string varNew)
{
    if (tokens.find(varOld) != tokens.end())
    {
        if (tokens.find(varNew) == tokens.end())
        { tokens[varNew] = new SymbolVar<T>(varNew); }
        for (auto& sym : symbolStack)
        {
            if (sym->GetToken() == varOld) { sym = tokens[varOld]; }
        }
    }
}

template <typename T> inline auto ParsedFunc<T>::GetVars()
{
    std::vector<Symbol<T>*> vars;
    for (auto& tok : tokens)
    {
        if (tok.second != nullptr && tok.second->IsVar())
            vars.push_back(tok.second.get());
    }
    return vars;
}

template <typename T> inline auto ParsedFunc<T>::GetVarMap() const
{
    std::map<std::string, T> varMap;
    for (auto S : symbolStack)
    {
        if (S->IsVar()) { varMap[S->GetToken()] = S->GetVal(); }
    }
    return varMap;
}

template <typename T>
inline void ParsedFunc<T>::RestoreVarsFromMap(std::map<std::string, T> varMap)
{
    for (auto S : symbolStack)
    {
        if (S->IsVar()) { S->SetVal(varMap[S->GetToken()]); }
    }
}

template <typename T> inline void Parser<T>::Initialize()
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
    RecognizeToken(new SymbolConst<T>("pi", M_PI));
    RecognizeToken(new SymbolConst<T>("e", exp(1)));

    RecognizeFunc((std::function<T(T)>)[](T z) { return exp(z); }, "exp");
    RecognizeFunc((std::function<T(T)>)[](T z) { return log(z); }, "log");
    RecognizeFunc((std::function<T(T)>)[](T z) { return sqrt(z); }, "sqrt");
    RecognizeFunc((std::function<T(T)>)[](T z) { return sin(z); }, "sin");
    RecognizeFunc((std::function<T(T)>)[](T z) { return cos(z); }, "cos");
    RecognizeFunc((std::function<T(T)>)[](T z) { return tan(z); }, "tan");
    RecognizeFunc((std::function<T(T)>)[](T z) { return sinh(z); }, "sinh");
    RecognizeFunc((std::function<T(T)>)[](T z) { return cosh(z); }, "cosh");
    RecognizeFunc((std::function<T(T)>)[](T z) { return tanh(z); }, "tanh");
    RecognizeFunc((std::function<T(T)>)[](T z) { return asin(z); }, "asin");
    RecognizeFunc((std::function<T(T)>)[](T z) { return acos(z); }, "acos");
    RecognizeFunc((std::function<T(T)>)[](T z) { return atan(z); }, "atan");
    RecognizeFunc((std::function<T(T)>)[](T z) { return asinh(z); }, "asinh");
    RecognizeFunc((std::function<T(T)>)[](T z) { return acosh(z); }, "acosh");
    RecognizeFunc((std::function<T(T)>)[](T z) { return atanh(z); }, "atanh");
    RecognizeFunc((std::function<T(T)>)[](T z) { return zeta(z); }, "zeta");
}

template <> inline void Parser<typename std::complex<double>>::Initialize()
{
    typedef typename std::complex<double> T;
    RecognizeToken(new SymbolAdd<T>);
    RecognizeToken(new SymbolSub<T>);
    RecognizeToken(new SymbolMul<T>);
    RecognizeToken(new SymbolDiv<T>);
    RecognizeToken(new SymbolPow<T>);
    RecognizeToken(new SymbolNeg<T>);
    RecognizeToken(new SymbolLParen<T>);
    RecognizeToken(new SymbolRParen<T>);
    RecognizeToken(new SymbolComma<T>);
    RecognizeToken(new SymbolConst<T>("pi", M_PI));
    RecognizeToken(new SymbolConst<T>("e", exp(1)));
    RecognizeToken(new SymbolConst<T>("i", T(0, 1)));

    RecognizeFunc((std::function<T(T)>)[](T z) { return exp(z); }, "exp");
    RecognizeFunc((std::function<T(T)>)[](T z) { return log(z); }, "log");
    RecognizeFunc((std::function<T(T)>)[](T z) { return sqrt(z); }, "sqrt");
    RecognizeFunc((std::function<T(T)>)[](T z) { return conj(z); }, "conj");
    RecognizeFunc((std::function<T(T)>)[](T z) { return sin(z); }, "sin");
    RecognizeFunc((std::function<T(T)>)[](T z) { return cos(z); }, "cos");
    RecognizeFunc((std::function<T(T)>)[](T z) { return tan(z); }, "tan");
    RecognizeFunc((std::function<T(T)>)[](T z) { return sinh(z); }, "sinh");
    RecognizeFunc((std::function<T(T)>)[](T z) { return cosh(z); }, "cosh");
    RecognizeFunc((std::function<T(T)>)[](T z) { return tanh(z); }, "tanh");
    RecognizeFunc((std::function<T(T)>)[](T z) { return asin(z); }, "asin");
    RecognizeFunc((std::function<T(T)>)[](T z) { return acos(z); }, "acos");
    RecognizeFunc((std::function<T(T)>)[](T z) { return atan(z); }, "atan");
    RecognizeFunc((std::function<T(T)>)[](T z) { return asinh(z); }, "asinh");
    RecognizeFunc((std::function<T(T)>)[](T z) { return acosh(z); }, "acosh");
    RecognizeFunc((std::function<T(T)>)[](T z) { return atanh(z); }, "atanh");
    RecognizeFunc((std::function<T(T)>)[](T z) { return zeta(z); }, "zeta");
}