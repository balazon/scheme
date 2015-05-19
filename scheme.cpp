#include <map>
#include <cstdlib>
#include <sstream>

#include "scheme.h"

//setting debug level
LogSettings Log{LogLevel::ERROR};


ErrorLog logerror;
DebugLog logdebug;


ErrorLogProxy loge;
DebugLogProxy logd;



//S-expression or "symbolic expression"
Sexp::~Sexp() {}

Sexp::operator bool() const {return true;}


Environment::Environment(std::shared_ptr<Environment> parent, std::vector<std::string> names, std::shared_ptr<List> values) : parent{parent}
{
	if(values == nullptr) return;

	if(names.size() > values->size())
	{
		loge << "more names than values for environment bindings";
		return;
	}
	//create bindings
	int i = 0;
	for(auto s : *values)
	{
		if(i >= names.size())
			break;
		
		std::shared_ptr<Sexp> valEval = s->eval(parent);
		bindArg(names[i], valEval);
		logd << " environment binding " << names[i] << " to " << *valEval << "\n";
		i++;
	}
}

void Environment::bindArg(std::string name, std::shared_ptr<Sexp> val)
{
	variables[name] = val;
}

std::shared_ptr<Sexp> Environment::getValue(std::string name)
{
	Environment* env = this;
	while(env != nullptr)
	{
		if(env->variables.count(name) == 1)
			return env->variables[name];
		env = env->parent.get();
	}
	return nullptr;
}

void Environment::print(std::ostream& os) const
{
	const Environment* env = this;
	std::vector<const Environment*> environments;
	do
	{
		environments.push_back(env);
		env = env->parent.get();
	} while(env != nullptr);
	
	os << "Environment: \n";
	for(auto it = environments.crbegin(); it != environments.crend(); ++it)
	{
		for (auto& kv : (*it)->variables)
		{
			os << " " << kv.first << " : " << *kv.second << std::endl;
		}
	}
}

void Environment::clearFunctionReferences()
{
	for (auto& kv : variables)
	{
		std::shared_ptr<Sexp> s = kv.second;
		std::shared_ptr<Lambda> l = std::dynamic_pointer_cast<Lambda>(s);
		if(l != nullptr)
			l->clearEnv();
	}
}

Environment* Environment::Global = nullptr;


std::ostream& operator<<(std::ostream& os, const Environment& e)
{
	e.print(os);
	return os;
}


std::ostream& operator<<(std::ostream& os, const Sexp& e)
{
    e.print(os);
    return os;
}


//Atom
Atom::~Atom(){}


//Number
Number::Number(int val) : value{val} {}
std::shared_ptr<Sexp> Number::eval(std::shared_ptr<Environment> context) const
{
	return std::make_shared<Number>(value);
}
void Number::print(std::ostream &os) const
{
	std::cout << value;
}

Number::operator int()
{
	return value;
}
Number::~Number()
{
	
}




//Symbol

Symbol::Symbol(std::string Name) : Name{Name} {}

std::string Symbol::getName() const
{
	return Name;
}
void Symbol::print(std::ostream &os) const
{
	os << Name;
}
	
std::shared_ptr<Sexp> Symbol::eval(std::shared_ptr<Environment> context) const
{
	std::shared_ptr<Sexp> exp = context->getValue(Name);
	if(exp == nullptr)
		loge << "variable " << Name << " is unbound\n";
	return exp;
}

Symbol::operator bool() const
{
	return Name != "#f";
}

Symbol::~Symbol()
{
	
}



//List

List::List(std::shared_ptr<Sexp> car, std::shared_ptr<List> cdr)
: car{car}, cdr{cdr}
{
	
}
List::List(std::vector<std::shared_ptr<Sexp>> elements)
{
	std::shared_ptr<List> last = nullptr;
	for(int i = elements.size() - 1; i >= 1; i--)
	{
		last = std::make_shared<List>(elements[i], last);
	}
	if(elements.size() > 0)
	{
		car = elements[0];
		cdr = last;
	}
	else
	{
		car = nullptr;
		cdr = nullptr;
	}
}

std::shared_ptr<Sexp> List::Car()
{
	return car;
}
std::shared_ptr<List> List::Cdr()
{
	return cdr;
}
const std::shared_ptr<Sexp> List::Car() const
{
	return car;
}
const std::shared_ptr<List> List::Cdr() const
{
	return cdr;
}
int List::size() const
{
	int i = 0;
	for(const auto& s : *this)
	{
		i++;
	}
	return i;
}

void List::print(std::ostream &os) const
{
	std::string prefix = "";
	os << "(";
	for(const auto& s : *this)
	{
		os << prefix << *s;
		prefix = " ";
	}
	os << ")";
}


std::shared_ptr<Sexp> List::eval(std::shared_ptr<Environment> context) const
{
	logd << "list eval: " << *this << "\n";
	
	if(car == nullptr)
	{
		return std::make_shared<List>();
	}
	
	std::shared_ptr<Sexp> exp;
	if(Symbol* first = dynamic_cast<Symbol*>(car.get()))
	{
		exp = context->getValue(first->getName());
	}
	else
	{
		exp = car->eval(context);
		logd << "exp not a symbol" << *exp << "\n";
	}
	
	Lambda* lambda = dynamic_cast<Lambda*>(exp.get());
	
	
	//call-by-value with some special cases
	// Parameters should be evaluated in current "context", before passed on to usual functions
	//  where they only see the environment of the lambda when it was created
	// Parameter evaluation for these cases are handled in their corresponding eval functions:
	// they use the context passed from here for that to encapsulate that behavior
	
	//for if, both expressions cant be evaluated only the one returning #t
	//for define the name isnt evaluated only the expression after that
	//for create lambda, neither parameters of function body are evaluated,
	// but them and the context are stored
	
	if(dynamic_cast<SyntaxLambda*>(lambda) != nullptr)
	{
		return lambda->eval(context, cdr);
	}
	
	//regular lambda expressions (created by users)
	if (lambda != nullptr)
	{
		logd << *lambda << "\n";
		std::vector<std::shared_ptr<Sexp>> args;
		
		if(cdr != nullptr)
		{
			for(auto s : *cdr)
			{
				std::shared_ptr<Sexp> seval = s->eval(context);
				if(seval == nullptr)
				{
					return nullptr;
				}
				args.push_back(seval);
			}
		}
		
		std::shared_ptr<List> lambda_args = std::make_shared<List>(args);
		
		//doesnt matter what we pass, labmdas will use their creation time context
		return lambda->eval(nullptr, lambda_args);
	}
	return std::make_shared<List>(car, cdr);
}
List::~List()
{
	
}

//iterator...
ListForwardIterator List::begin()
{
	return ListForwardIterator{car == nullptr ? nullptr : this};
}
ListForwardIterator List::end()
{
	return ListForwardIterator{nullptr};
}

ConstListForwardIterator List::begin() const
{
	return cbegin();
}
ConstListForwardIterator List::end() const
{
	return cend();
}


ConstListForwardIterator List::cbegin() const
{
	return ConstListForwardIterator{car == nullptr ? nullptr : this};
}
ConstListForwardIterator List::cend() const
{
	return ConstListForwardIterator{nullptr};
	
}

ListForwardIterator::ListForwardIterator(List* l) : l{l} {}

ListForwardIterator& ListForwardIterator::operator ++ ()
{
	l = l == nullptr ? nullptr : l->Cdr().get();
	return *this;
}
std::shared_ptr<Sexp> ListForwardIterator::operator*()
{
	return l->Car();
}
bool ListForwardIterator::operator !=(const ListForwardIterator& other) const
{
	return l != other.l;	
}


ConstListForwardIterator::ConstListForwardIterator(const List* l) : l{l} {}
ConstListForwardIterator& ConstListForwardIterator::operator ++ ()
{
	l = l == nullptr ? nullptr : l->Cdr().get();
	return *this;
}
const std::shared_ptr<Sexp> ConstListForwardIterator::operator*()
{
	return l->Car();
}
bool ConstListForwardIterator::operator !=(const ConstListForwardIterator& other) const
{
	return l != other.l;	
}



//Lambda
Lambda::Lambda(std::shared_ptr<Environment> env) : Lambda{env, nullptr, nullptr} {}

Lambda::Lambda(std::shared_ptr<Environment> env, std::shared_ptr<List> arglist, std::shared_ptr<Sexp> body) : env{env}, arglist{arglist}, body{body}
{
	if(arglist == nullptr || body == nullptr)
		return;
	
	for (auto s : *arglist)
	{
		std::shared_ptr<Symbol> arg = std::dynamic_pointer_cast<Symbol>(s);
		args.push_back(arg->getName());
	}
}
std::shared_ptr<Sexp> Lambda::eval(std::shared_ptr<Environment> context) const
{
	return std::make_shared<Lambda>(env, arglist, body);
}
void Lambda::print(std::ostream &os) const
{
	os << "(lambda (";
	std::string prefix = "";
	for (auto arg : args)
	{
		std::cout << prefix << arg;
		prefix = " ";
	}
	os << ") " << *body << ")";
	
}

//function call
std::shared_ptr<Sexp> Lambda::eval(std::shared_ptr<Environment> context, std::shared_ptr<List> params) const
{
	logd << "lambda eval: func: " << *this << ", body: " << *body << "\n";
	
	std::shared_ptr<Environment> functionEnv = std::make_shared<Environment>(env, args, params);
	logd << *functionEnv;
	std::shared_ptr<Sexp> result = body->eval(functionEnv);
	
	if(result != nullptr)
		logd << " result: " << *result << "\n";
	return result;
}

void Lambda::clearEnv()
{
	env = nullptr;
}


//Syntax lambda
SyntaxLambda::SyntaxLambda(std::shared_ptr<Environment> env) : Lambda{env} {}


//CreateLambda
// (lambda (vars ...) (body))

CreateLambda::CreateLambda(std::shared_ptr<Environment> env) : SyntaxLambda{env} {}

//params being a list containing (function arguments, function body)
std::shared_ptr<Sexp> CreateLambda::eval(std::shared_ptr<Environment> context, std::shared_ptr<List> params) const
{
	if(params->size() < 2)
	{
		loge << "not enough arguments for lambda. (Expected: 2, got: " << params->size() << ")\n";
		return nullptr;
	}
	std::shared_ptr<List> funargs = std::dynamic_pointer_cast<List>(params->Car());
	std::shared_ptr<Sexp> body = params->Cdr()->Car();
	logd << "args: " <<*funargs << ", body: " << *body << "\n";
	logd << *context << "\n";
	return std::make_shared<Lambda>(context, funargs, body);
}

void CreateLambda::print(std::ostream &os) const
{
	os << "create lambda";
}



//DefineFunction

// (define (fun vars...) (body))
// (define fun (lambda ..))
DefineFunction::DefineFunction(std::shared_ptr<Environment> env) : SyntaxLambda{env} {}

std::shared_ptr<Sexp> DefineFunction::eval(std::shared_ptr<Environment> context, std::shared_ptr<List> params) const
{
	std::shared_ptr<Symbol> variable = std::dynamic_pointer_cast<Symbol>(params->Car());
	std::shared_ptr<Sexp> exp;
	if(variable == nullptr)
	{
		std::shared_ptr<List> first = std::dynamic_pointer_cast<List>(params->Car());
		if(first == nullptr)
		{
			loge << "define expects a name\n";
			return nullptr;
		}
		std::shared_ptr<List> funargs = first->Cdr();
		funargs = funargs == nullptr ? std::make_shared<List>() : funargs;
		variable = std::dynamic_pointer_cast<Symbol>(first->Car());
		std::shared_ptr<Sexp> body = params->Cdr()->Car();
		
		std::vector<std::shared_ptr<Sexp>> listElements;
		listElements.push_back(std::make_shared<Symbol>("lambda"));
		listElements.push_back(funargs);
		listElements.push_back(body);
		exp = std::make_shared<List>(listElements);
	}
	else
	{
		exp = params->Cdr()->Car();
	}
	exp = exp->eval(context);
	
	//env is the global environment here
	env->bindArg(variable->getName(), exp);
	
	//not really needed for define to print out result,
	// but I think it can be useful
	return exp;
}

void DefineFunction::print(std::ostream &os) const
{
	os << "default_define";
}


//IfLambda
// (if test trueExp falseExp)

IfLambda::IfLambda(std::shared_ptr<Environment> env) : SyntaxLambda{env} {}

std::shared_ptr<Sexp> IfLambda::eval(std::shared_ptr<Environment> context, std::shared_ptr<List> params) const
{
	std::shared_ptr<Sexp> test = params->Car()->eval(context);
	std::shared_ptr<Sexp> trueExp = params->Cdr()->Car();
	std::shared_ptr<Sexp> falseExp = params->Cdr()->Cdr()->Car();
	//Sexp* t; t->operator bool()
	return test->operator bool() ? trueExp->eval(context) : falseExp->eval(context);
}

void IfLambda::print(std::ostream &os) const
{
	os << "if form";
}


//(and ...)
AndBooleanAggregateFunction::AndBooleanAggregateFunction(std::shared_ptr<Environment> env) : Lambda{env} {}
std::shared_ptr<Sexp> AndBooleanAggregateFunction::eval(std::shared_ptr<Environment> context, std::shared_ptr<List> params) const
{
	if(params->size() < 1)
	{
		loge << "not enough arguments for " << *this << ". (Expected: at least 1, got: " << params->size() << ")\n";
		return nullptr;
	}
	for(auto s : *params)
	{
		std::shared_ptr<Sexp> head = s->eval(context);
		bool val = (bool) *head;
		if(!val)
		{
			return env->getValue("#f");
		}
	}
	return env->getValue("#t");
}
void AndBooleanAggregateFunction::print(std::ostream &os) const
{
	os << "BasicAnd";
}

//(or ...)
OrBooleanAggregateFunction::OrBooleanAggregateFunction(std::shared_ptr<Environment> env) : Lambda{env} {}
std::shared_ptr<Sexp> OrBooleanAggregateFunction::eval(std::shared_ptr<Environment> context, std::shared_ptr<List> params) const
{
	if(params->size() < 1)
	{
		loge << "not enough arguments for " << *this << ". (Expected: at least 1, got: " << params->size() << ")\n";
		return nullptr;
	}
	for(auto s : *params)
	{
		std::shared_ptr<Sexp> head = s->eval(context);
		bool val = (bool) *head;
		if(!val)
		{
			return env->getValue("#t");
		}
	}
	return env->getValue("#f");
}
void OrBooleanAggregateFunction::print(std::ostream &os) const
{
	os << "BasicOr";
}


Number* CommonInteger::ConvertAndCheck(Sexp* s)
{
	Symbol* unboundVariable = dynamic_cast<Symbol*>(s);
	if(unboundVariable != nullptr)
	{
		loge << *s << " is unbound\n";
		return nullptr;
	}
	Number* num = dynamic_cast<Number*>(s);
	if(num == nullptr)
	{
		loge << *s << " is not a number\n";
		return nullptr;
	}
	return num;
}


BasicCompareFunction::BasicCompareFunction(std::shared_ptr<Environment> env) : Lambda{env} {}
std::shared_ptr<Sexp> BasicCompareFunction::eval(std::shared_ptr<Environment> context, std::shared_ptr<List> params) const
{
	if(params->size() < 2)
	{
		loge << "not enough arguments for " << *this << ". (Expected: at least 2, got: " << params->size() << ")\n";
		return nullptr;
	}
	Number* num = CommonInteger::ConvertAndCheck(params->Car().get());
	if(num == nullptr)
		return nullptr;
	
	int val = (int)*num;
	
	bool res = true;
	
	for(auto s : *params->Cdr())
	{
		num = CommonInteger::ConvertAndCheck(s.get());
		if(num == nullptr)
			return nullptr;
		res = res && do_operator(val, (int)*num);
		val = (int)*num;
	}	
	return res ? env->getValue("#t") : env->getValue("#f");
}

//Less
LessFunction::LessFunction(std::shared_ptr<Environment> env) : BasicCompareFunction{env} {}
bool LessFunction::do_operator(int left, int right) const
{
	return left < right;
}
void LessFunction::print(std::ostream &os) const
{
	os << "basic<";
}

//Greater
GreaterFunction::GreaterFunction(std::shared_ptr<Environment> env) : BasicCompareFunction{env} {}
bool GreaterFunction::do_operator(int left, int right) const
{
	return left > right;
}
void GreaterFunction::print(std::ostream &os) const
{
	os << "basic>";
}

//Equal
EqualFunction::EqualFunction(std::shared_ptr<Environment> env) : BasicCompareFunction{env} {}
bool EqualFunction::do_operator(int left, int right) const
{
	return left == right;
}
void EqualFunction::print(std::ostream &os) const
{
	os << "basic=";
}



//BasicArithmeticFunction

BasicArithmeticFunction::BasicArithmeticFunction(std::shared_ptr<Environment> env) : Lambda{env} {}

std::shared_ptr<Sexp> BasicArithmeticFunction::eval(std::shared_ptr<Environment> context, std::shared_ptr<List> params) const
{
	if(params->size() < 2)
	{
		loge << "not enough arguments for " << *this << ". (Expected: at least 2, got: " << params->size() << ")\n";
		return nullptr;
	}
	Number* num = CommonInteger::ConvertAndCheck(params->Car().get());
	if(num == nullptr)
		return nullptr;
	int res = (int)*num;
	for(auto s : *params->Cdr())
	{
		num = CommonInteger::ConvertAndCheck(s.get());
		if(num == nullptr)
			return nullptr;
		if(checkParamsValid(res, (int)*num))
		{
			res = do_operator(res, (int)*num);
		}
		else
		{
			return nullptr;
		}
	}	
	//all variables bound
	return std::make_shared<Number>(res);
}

bool BasicArithmeticFunction::checkParamsValid(int left, int right) const
{
	return true;
}


//AddFunction
AddFunction::AddFunction(std::shared_ptr<Environment> env) : BasicArithmeticFunction{env} {}

int AddFunction::do_operator(int left, int right) const
{
	return left + right;
}
void AddFunction::print(std::ostream &os) const
{
	os << "basic+";
}


//MultiplyFunction
MultiplyFunction::MultiplyFunction(std::shared_ptr<Environment> env) : BasicArithmeticFunction{env} {}

int MultiplyFunction::do_operator(int left, int right) const
{
	return left * right;
}

void MultiplyFunction::print(std::ostream &os) const
{
	os << "basic*";
}

//MinusFunction
MinusFunction::MinusFunction(std::shared_ptr<Environment> env) : BasicArithmeticFunction{env} {}

int MinusFunction::do_operator(int left, int right) const
{
	return left - right;
}

void MinusFunction::print(std::ostream &os) const
{
	os << "basic-";
}

//DivisionFunction
DivisionFunction::DivisionFunction(std::shared_ptr<Environment> env) : BasicArithmeticFunction{env} {}

int DivisionFunction::do_operator(int left, int right) const
{
	return left / right;
}

bool DivisionFunction::checkParamsValid(int left, int right) const
{
	if(right == 0)
	{
		loge << "division by zero.\n";
		return false;
	}
	return true;
}

void DivisionFunction::print(std::ostream &os) const
{
	os << "basic/";
}




//SchemeInterpreter

SchemeInterpreter::SchemeInterpreter()
	: exit{std::make_shared<Symbol>("exit")}
	, help{std::make_shared<Symbol>("help")}
	, symbol_logdebug{std::make_shared<Symbol>("logdebug")}
	, symbol_logerror{std::make_shared<Symbol>("logerror")}
	, symbol_lognone{std::make_shared<Symbol>("lognone")}
	, exited{false}
	, global{std::make_shared<Environment>()}
{
	Environment::Global = global.get();
	
	global->bindArg("exit", exit);
	global->bindArg("help", help);
	global->bindArg("logdebug", symbol_logdebug);
	global->bindArg("logerror", symbol_logerror);
	global->bindArg("lognone", symbol_lognone);
	
	
	
	global->bindArg("define", std::make_shared<DefineFunction>(global));
	global->bindArg("lambda", std::make_shared<CreateLambda>(global));
	global->bindArg("if", std::make_shared<IfLambda>(global));
	
	global->bindArg("#t", std::make_shared<Symbol>("#t"));
	global->bindArg("#f", std::make_shared<Symbol>("#f"));
	
	eval("(define not (lambda (x) (if x #f #t)))");
	
	global->bindArg("and", std::make_shared<AndBooleanAggregateFunction>(global));
	global->bindArg("or", std::make_shared<OrBooleanAggregateFunction>(global));
	
	
	global->bindArg("<", std::make_shared<LessFunction>(global));
	global->bindArg(">", std::make_shared<GreaterFunction>(global));
	global->bindArg("=", std::make_shared<EqualFunction>(global));
	
	global->bindArg("+", std::make_shared<AddFunction>(global));
	global->bindArg("*", std::make_shared<MultiplyFunction>(global));
	global->bindArg("-", std::make_shared<MinusFunction>(global));
	global->bindArg("/", std::make_shared<DivisionFunction>(global));
	
	
	
	helpDialog = 
	"Bala's scheme interpreter :\n"
	" Basic commands, and functions:\n"
	"\n"
	" help - prints this help dialog\n"
	" exit - exits program\n"
	" define - defines a variable or a function eg. (define a 5) or (define (a x) (+ x 2))\n"
	" lambda - creates a lambda expression eg. (lambda (x y) (* x y))\n"
	" if - (if test true_expression false_expression)\n"
	" not - negates a bool expression\n"
	" < : less\n"
	" > : greater\n"
	" = : equals\n"
	" + : add\n"
	" * : multiply\n"
	" - : minus\n"
	" / : division\n"
	" You can call a function by placing the function and the parameters in a list:\n"
	" (function param1 param2)\n";
	
}

std::shared_ptr<Sexp> SchemeInterpreter::eval(std::string str)
{
	
	std::istringstream ss(str);
	return eval(read(ss));
}

std::shared_ptr<Sexp> SchemeInterpreter::createAtom(std::string temp)
{

	int num = strtol(temp.c_str(), nullptr, 10);
	if(num != 0 || temp == "0")
	{
		return std::make_shared<Number>(num);
	}
	else
	{
		return std::make_shared<Symbol>(temp);
	}
}

std::shared_ptr<Sexp> SchemeInterpreter::readAtom(char c, std::istream& is)
{
	std::string temp;
	getline (is, temp);
	temp.insert(temp.begin(), c);
	size_t varlength = temp.find_first_of("; \t\n\v\f\r");
	temp = varlength != std::string::npos ? temp.substr(0, varlength) : temp;
	
	
	std::shared_ptr<Sexp> expression = global->getValue(temp);
	if(expression == nullptr)
	{
		int num = strtol(temp.c_str(), nullptr, 10);
		if(num != 0 || temp == "0")
		{
			expression = std::make_shared<Number>(num);
		}
		else
		{
			loge << "undefined variable: " << temp << "\n";
			return nullptr;
		}
	}
	return expression->eval(global);
}


std::shared_ptr<Sexp> SchemeInterpreter::read(std::istream& is)
{
	
	char c;
	
	while(is.get(c) && isspace(c));
	if(c != '(')
	{
		//atom
		return readAtom(c);
	}
	else
	{
		//list
		bool comment = false;
		
		int parentheses = 1;
		std::vector<std::vector<std::shared_ptr<Sexp>>> listElements;
		listElements.push_back(std::vector<std::shared_ptr<Sexp>>());
		
		std::string temp;
		do
		{
			bool wasDelimiter = false;
			while(comment)
			{
				is.get(c);
				if(c == '\n')
				{
					comment = false;
				}
			}
			while(is.get(c) && isspace(c)){
				wasDelimiter = true;
			}
			if(c == '(' || c == ')')
			{
				wasDelimiter = true;
			}
			if(wasDelimiter && temp.size() > 0)
			{
				listElements[parentheses - 1].push_back(createAtom(temp));
				temp = "";
			}
			switch (c)
			{
				case ')' :
				{
					parentheses--;
					std::shared_ptr<List> l = std::make_shared<List>(listElements[parentheses]);
					logd << *l << "\n";
					listElements.pop_back();
					if(parentheses > 0)
					{
						listElements[parentheses - 1].push_back(l);
					}
					else
					{
						std::string line;
						getline(is, line);
						return l;
					}
					break;
				}
				case '(' :
				{
					parentheses++;
					listElements.push_back(std::vector<std::shared_ptr<Sexp>>());
					break;
				}
				case ';' :
				{
					//comment
					comment = true;
					break;
				}
				default:
				{
					temp.push_back(c);
					break;
				}
			}
			
		}
		while(parentheses > 0);
	}
	
	throw("parsing error");
	
	
}

std::shared_ptr<Sexp> SchemeInterpreter::eval(std::shared_ptr<Sexp> exp)
{
	return exp->eval(global);
}

void SchemeInterpreter::print(std::shared_ptr<Sexp> exp)
{
	std::cout << *exp << "\n";
}

void SchemeInterpreter::run()
{
	exited = false;
	while(!exited)
	{
		std::cout << ">";
		std::shared_ptr<Sexp> exp = read();
		
		if (exp == exit)
		{
			exited = true;
			continue;
		}
		if (exp == help)
		{
			std::cout << helpDialog;
			continue;
		}
		if (exp == symbol_logdebug)
		{
			Log = LogSettings{LogLevel::DEBUG};
		}
		if (exp == symbol_logerror)
		{
			Log = LogSettings{LogLevel::ERROR};
		}
		if (exp == symbol_lognone)
		{
			Log = LogSettings{LogLevel::NONE};
		}
		
		if(exp == nullptr)
		{
			continue;
		}
		exp = eval(exp);
		if(exp != nullptr)
			print(exp);
		
	}
	std::cout << "Program terminated\n";
}


SchemeInterpreter::~SchemeInterpreter()
{
	//read comments of Environment::clearFunctionReferences() in header
	global->clearFunctionReferences();
}


LogSettings::LogSettings(LogLevel level) : level{level} {}

template <typename T>
ErrorLog& ErrorLog::operator << (T&& t)
{
	if(Log.level >= LogLevel::ERROR)
		std::cout << std::forward<T>(t);
	return *this;
}

template <typename T>
DebugLog& DebugLog::operator << (T&& t)
{
	if(Log.level >= LogLevel::DEBUG)
		std::cout << std::forward<T>(t);
	return *this;
}


template <typename T>
ErrorLog& ErrorLogProxy::operator << (T&& t)
{
	return logerror << "Error: " << std::forward<T>(t);
}

template <typename T>
DebugLog& DebugLogProxy::operator << (T&& t)
{
	return logdebug << "Debug: " << std::forward<T>(t);
}
