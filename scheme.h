#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>




#pragma once



class Environment;
/**	S-expression or "symbolic expression" - the base class of all expressions */
class Sexp
{
public:
	/**	an expression can be evaluated in a context*/
	virtual std::shared_ptr<Sexp> eval(std::shared_ptr<Environment> context) const = 0;
	virtual void print(std::ostream &os) const = 0;
	virtual ~Sexp();
	
	/**	all expressions can be evaluated as a boolean true or false (only #f symbol is false) */
	virtual explicit operator bool() const;
	
};

class List;

/**	Environment is a context for storing variable bindings, and they can refer to their parents (except the global context) */
class Environment
{
	std::shared_ptr<Environment> parent;
	std::map<std::string, std::shared_ptr<Sexp>> variables;
	
public:
	Environment(std::shared_ptr<Environment> parent = nullptr, std::vector<std::string> names = std::vector<std::string>(), std::shared_ptr<List> values = nullptr);
	
	void bindArg(std::string name, std::shared_ptr<Sexp> val);
	
	std::shared_ptr<Sexp> getValue(std::string name);
	
	void print(std::ostream& os) const;
	
	static Environment* Global;
	
	
	/**	functions in global environment reference the global environment and the global environment references them
	 * 	so the shared reference loop must be broken to be able to destroy them */
	void clearFunctionReferences();
};

std::ostream & operator<<(std::ostream & os, const Environment& e);


std::ostream & operator<<(std::ostream & os, const Sexp& e);

/**	Atom is the base class of Symbol and Number */
class Atom : public Sexp
{
public:
	virtual ~Atom();
};

/**	Number is an integer */
class Number : public Atom
{
	int value;
	
public:
	Number(int val = 0);
	std::shared_ptr<Sexp> eval(std::shared_ptr<Environment> context) const;
	virtual void print(std::ostream &os) const;
	
	operator int();
	~Number();
};

/**	Symbol is a variable name */
class Symbol : public Atom
{
	std::string Name;
public:
	Symbol(std::string Name);
	
	std::string getName() const;
	virtual void print(std::ostream &os) const;
	
	std::shared_ptr<Sexp> eval(std::shared_ptr<Environment> context) const;
	
	virtual explicit operator bool() const;
	
	~Symbol();
	
};



class ListForwardIterator;
class ConstListForwardIterator;

/**	List  - also known as a Cons cell */
class List : public Sexp
{
	std::shared_ptr<Sexp> car;
	std::shared_ptr<List> cdr;
public:
	List(std::shared_ptr<Sexp> car = nullptr, std::shared_ptr<List> cdr = nullptr);
	List(std::vector<std::shared_ptr<Sexp>> elements);
	
	std::shared_ptr<Sexp> Car();
	std::shared_ptr<List> Cdr();
	const std::shared_ptr<Sexp> Car() const;
	const std::shared_ptr<List> Cdr() const;
	
	int size() const;
	
	virtual void print(std::ostream &os) const;
	
	/**	if car/head is a function then call it with cdr/tail as arguments */
	virtual std::shared_ptr<Sexp> eval(std::shared_ptr<Environment> context) const;
	~List();
	
	using iterator = ListForwardIterator;
	iterator begin();
    iterator end();
	using const_iterator = ConstListForwardIterator;
    const_iterator cbegin() const;
    const_iterator cend() const;
	
	const_iterator begin() const;
    const_iterator end() const;
};

/**	non-const iterator for List */
class ListForwardIterator
{
	List* l;
public:
	ListForwardIterator(List* l);
	ListForwardIterator& operator ++ ();
	std::shared_ptr<Sexp> operator*();
	bool operator !=(const ListForwardIterator& other) const;
};

/**	const iterator for List */
class ConstListForwardIterator
{
	const List* l;
public:
	ConstListForwardIterator(const List* l);
	ConstListForwardIterator& operator ++ ();
	const std::shared_ptr<Sexp> operator*();
	bool operator !=(const ConstListForwardIterator& other) const;
};

/**	lambda is an anonymous function */
class Lambda : public Sexp
{
protected:

	std::shared_ptr<Environment> env;
	
	/**	arglist is a list of symbols */
	std::shared_ptr<List> arglist;
	std::shared_ptr<Sexp> body;
	
	/**	args is just the names of the symbols in arglist */
	std::vector<std::string> args;
	
public:
	Lambda(std::shared_ptr<Environment> env);
	Lambda(std::shared_ptr<Environment> env, std::shared_ptr<List> arglist, std::shared_ptr<Sexp> body);
	
	/**	lambdas return themselves */
	virtual std::shared_ptr<Sexp> eval(std::shared_ptr<Environment> context) const;
	void print(std::ostream &os) const;
	
	/**	function call - this involves creating a new environment*/
	virtual std::shared_ptr<Sexp> eval(std::shared_ptr<Environment> context, std::shared_ptr<List> params) const;
	
	/**	Environment::clearFunctionReferences uses this */
	void clearEnv();
};

/** base class for define, createlambda, and if */
class SyntaxLambda : public Lambda
{
public:
	SyntaxLambda(std::shared_ptr<Environment> env);
};

/** (lambda (vars ...) (body)) */
class CreateLambda : public SyntaxLambda
{
	public:
	CreateLambda(std::shared_ptr<Environment> env);
	
	/**	params being a list containing function arguments, and function body */
	virtual std::shared_ptr<Sexp> eval(std::shared_ptr<Environment> context, std::shared_ptr<List> params) const;
	
	void print(std::ostream &os) const;
};


/**	(define (fun vars...) (body))
 *	(define fun (lambda ..)) */
class DefineFunction : public SyntaxLambda
{
public:
	DefineFunction(std::shared_ptr<Environment> env);
	
	virtual std::shared_ptr<Sexp> eval(std::shared_ptr<Environment> context, std::shared_ptr<List> params) const;
	
	void print(std::ostream &os) const;
};

/**	(if test trueExp falseExp)
 * 	this is like a ternary operator */
class IfLambda : public SyntaxLambda
{
	public:
	IfLambda(std::shared_ptr<Environment> env);
	
	virtual std::shared_ptr<Sexp> eval(std::shared_ptr<Environment> context, std::shared_ptr<List> params) const;
	
	void print(std::ostream &os) const;
};

class AndBooleanAggregateFunction : public Lambda
{
public:
	AndBooleanAggregateFunction(std::shared_ptr<Environment> env);
	virtual std::shared_ptr<Sexp> eval(std::shared_ptr<Environment> context, std::shared_ptr<List> params) const;
	void print(std::ostream &os) const;
};

class OrBooleanAggregateFunction : public Lambda
{
public:
	OrBooleanAggregateFunction(std::shared_ptr<Environment> env);
	virtual std::shared_ptr<Sexp> eval(std::shared_ptr<Environment> context, std::shared_ptr<List> params) const;
	void print(std::ostream &os) const;
};


namespace CommonInteger
{
	/**	this is used for basic operations: +, -, *, /, <, >, =
	 * 	checks if expression is an unbound variable or a non-number
	 * 	if successful, returns the number, upon failure returns nullptr */
	Number* ConvertAndCheck(Sexp* s);
};

/** base class for  <, >, and = boolean operators - they can take more than two args */
class BasicCompareFunction : public Lambda
{
public:
	BasicCompareFunction(std::shared_ptr<Environment> env);
	virtual std::shared_ptr<Sexp> eval(std::shared_ptr<Environment> context, std::shared_ptr<List> params) const;
	virtual bool do_operator(int left, int right) const = 0;
};

class LessFunction : public BasicCompareFunction
{
public:
	LessFunction(std::shared_ptr<Environment> env);
	virtual bool do_operator(int left, int right) const;
	void print(std::ostream &os) const;
};

class GreaterFunction : public BasicCompareFunction
{
public:
	GreaterFunction(std::shared_ptr<Environment> env);
	virtual bool do_operator(int left, int right) const;
	void print(std::ostream &os) const;
};

class EqualFunction : public BasicCompareFunction
{
public:
	EqualFunction(std::shared_ptr<Environment> env);
	virtual bool do_operator(int left, int right) const;
	void print(std::ostream &os) const;
};


/**	base class for +, -, *, / */
class BasicArithmeticFunction : public Lambda
{
public:
	BasicArithmeticFunction(std::shared_ptr<Environment> env);
	virtual std::shared_ptr<Sexp> eval(std::shared_ptr<Environment> context, std::shared_ptr<List> params) const;
	virtual int do_operator(int left, int right) const = 0;
	virtual bool checkParamsValid(int left, int right) const;
};

class AddFunction : public BasicArithmeticFunction
{
public:
	AddFunction(std::shared_ptr<Environment> env);
	
	int do_operator(int left, int right) const;
	
	void print(std::ostream &os) const;
};

class MultiplyFunction : public BasicArithmeticFunction
{
public:
	MultiplyFunction(std::shared_ptr<Environment> env);
	
	int do_operator(int left, int right) const;
	
	void print(std::ostream &os) const;
};

class MinusFunction : public BasicArithmeticFunction
{
public:
	MinusFunction(std::shared_ptr<Environment> env);
	
	int do_operator(int left, int right) const;
	
	void print(std::ostream &os) const;
};

class DivisionFunction : public BasicArithmeticFunction
{
public:
	DivisionFunction(std::shared_ptr<Environment> env);
	
	int do_operator(int left, int right) const;
	
	bool checkParamsValid(int left, int right) const;
	
	void print(std::ostream &os) const;
};






/**	interpreter that handles the read-eval-print-loop, and the global context */
class SchemeInterpreter
{
	std::shared_ptr<Symbol> exit;
	std::shared_ptr<Symbol> help;
	std::shared_ptr<Symbol> symbol_logdebug;
	std::shared_ptr<Symbol> symbol_logerror;
	std::shared_ptr<Symbol> symbol_lognone;
	std::string helpDialog;
	bool exited;
	
	std::shared_ptr<Environment> global;
public:
	SchemeInterpreter();
	
	std::shared_ptr<Sexp> createAtom(std::string temp);
	
	std::shared_ptr<Sexp> readAtom(char c, std::istream& is = std::cin);
	
	std::shared_ptr<Sexp> read(std::istream& is = std::cin);
	
	std::shared_ptr<Sexp> eval(std::string str);
	
	std::shared_ptr<Sexp> eval(std::shared_ptr<Sexp> exp);
	
	void print(std::shared_ptr<Sexp> exp);
	
	void run();
	
	~SchemeInterpreter();
};

/**	Logging system:
 * 	logging errors: loge << "error message" << std::endl;
 * 	logging debugging info: logd << "debug msg" << std::endl;
 * 	loge and logd can also receive any object that has operator<<(std::ostream & os, <object type> e) implemented
 * 	The level of logging can be set in the beginning of the cpp file, 
 * 	or at runtime by entering one of the following into the console: logdebug, logerror, lognone */
enum class LogLevel {NONE, ERROR, DEBUG};
struct LogSettings
{
	LogLevel level;
	LogSettings(LogLevel level = LogLevel::ERROR);
};


struct ErrorLog
{
	template <typename T>
	ErrorLog& operator << (T&& t);
};

struct DebugLog
{
	template <typename T>
	DebugLog& operator << (T&& t);
};

struct ErrorLogProxy
{
	template <typename T>
	ErrorLog& operator << (T&& t);
};

struct DebugLogProxy
{
	template <typename T>
	DebugLog& operator << (T&& t);
};



