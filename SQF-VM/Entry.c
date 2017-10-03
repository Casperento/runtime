#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include <string.h>
#include <setjmp.h>
#include <stdint.h>
#if _WIN32 & _DEBUG
#include <crtdbg.h>
#endif
#ifdef __linux
#include <alloca.h>
#endif // _GCC
#ifdef _WIN32
#include <malloc.h>
#endif // _WIN32

#include "basetype.h"
#include "vector.h"
#include "string_map.h"
#include "textrange.h"
#include "SQF.h"
#include "SQF_types.h"
#include "SQF_object_type.h"
#include "SQF_parse.h"
#include "SQF_commands.h"


#ifdef _WIN32
#define DLLEXPORT_PREFIX __declspec(dllexport)
#else
#define DLLEXPORT_PREFIX __attribute__((visibility("default")))
#endif


static jmp_buf program_exit;
int64_t systime_start = 0;




void cmd_productversion(void* input, CPCMD self);

char* get_line(char* line, size_t lenmax)
{
	char* line_start = line;
	size_t len = lenmax;
	int c;

	if (line == NULL)
		return NULL;

	while (1)
	{
		c = fgetc(stdin);
		if (c == EOF)
			break;

		if (--len == 0)
		{
			len++;
			line--;
		}

		if ((*line++ = c) == '\n')
			break;
	}
	*line = '\0';
	return line_start;
}

#define LINEBUFFER_SIZE 256

void custom_error(PVM vm, const char* errMsg, PSTACK stack)
{
	int i;
	PDBGINF dbginf;
	if (stack->allow_dbg)
	{
		dbginf = 0;
		for (i = stack->top - 1; i >= 0; i--)
		{
			if (stack->data[i]->type == INST_DEBUG_INFO)
			{
				dbginf = get_dbginf(0, 0, stack->data[i]);
				break;
			}
		}
		if (dbginf != 0)
		{
			vm->print(vm, "%s", dbginf->hint);
			vm->print(vm, "[ERR][L%d|C%d] %s\n", dbginf->line, dbginf->col, errMsg);
		}
		else
		{
			vm->print(vm, "[ERR] %s\n", errMsg);
		}
	}
	else
	{
		vm->print(vm, "[ERR] %s\n", errMsg);
	}
	vm->die_flag = 1;
	//longjmp(program_exit, 1);
}
void custom_warn(PVM vm, const char* errMsg, PSTACK stack)
{
	int i;
	PDBGINF dbginf;
	if (stack->allow_dbg)
	{
		dbginf = 0;
		for (i = stack->top - 1; i >= 0; i--)
		{
			if (stack->data[i]->type == INST_DEBUG_INFO)
			{
				dbginf = get_dbginf(0, 0, stack->data[i]);
				break;
			}
		}
		if (dbginf != 0)
		{
			vm->print(vm, "%s", dbginf->hint);
			vm->print(vm, "[WRN][L%d|C%d] %s\n", dbginf->line, dbginf->col, errMsg);
		}
		else
		{
			vm->print(vm, "[WRN] %s\n", errMsg);
		}
	}
	else
	{
		vm->print(vm, "[WRN] %s\n", errMsg);
	}
}

void create_cmd(PVM vm, const char* name, char type, CMD_CB fnc, char precedence, const char* usage, const char* examples_cs, const char* desc)
{
	register_command(vm, create_command(name, type, fnc, precedence, usage, examples_cs, desc));
}
void register_commmands(PVM vm)
{
	if (find_command(vm, "+", 'b') != 0)
		return;
	create_cmd(vm, "+", 'b', cmd_plus, 6, "<SCALAR> + <SCALAR> | <STRING> + <STRING> | <ARRAY> + <ANY>", "1 + 1 //2#" "\"foo\" + \"bar\" //\"foobar\"#" "[] + 1 //[1]#", "b added to a.");
	create_cmd(vm, "-", 'b', cmd_minus, 6, "<SCALAR> - <SCALAR> | <ARRAY> - <ARRAY>", "1 - 1 //0#" "[0,[0],[[0]]] - [0] //[[0], [[0]]]#" "[0,[0],[[0]]] - [[0]] //[0, [[0]]]#" "[0,[0],[[0]]] - [[[0]]] //[0, [0]]#", "Subtracts b from a. a and b need to be of the same type, both Numbers or both Arrays. In Arma 3 it is possible to subtract nested arrays.");
	create_cmd(vm, "*", 'b', cmd_multiply, 7, "<SCALAR> * <SCALAR>", "0.5 * 100 //50#", "Returns the value of a multiplied by b.");
	create_cmd(vm, "/", 'b', cmd_divide, 7, "<SCALAR> / <SCALAR>", "15 / 3 //5#", "a divided by b. Division by 0 throws \"Division by zero\" error, however script doesn't stop and the result of such division is assumed to be 0.");
	create_cmd(vm, ">", 'b', cmd_greaterthan, 3, "<SCALAR> > <SCALAR>", "10 > 1 //true#1 > 10 //false#", "Returns true if a is greater than b, else returns false.");
	create_cmd(vm, "<", 'b', cmd_lessthen, 3, "<SCALAR> < <SCALAR>", "1 < 2 //true#2 < 1 //false#", "Returns true if a is less than b, else returns false.");
	create_cmd(vm, ">=", 'b', cmd_largetthenorequal, 3, "<SCALAR> >= <SCALAR>", "10 > 1 //true#1 > 10 //false#10 >= 10 //true#", "Returns true if a is greater than or equal to b, else returns false.");
	create_cmd(vm, "<=", 'b', cmd_lessthenorequal, 3, "<SCALAR> <= <SCALAR>", "1 < 2 //true#2 <= 1 //false#2 <= 2 //true#", "Returns true if a is less than or equal to b, else returns false.");
	create_cmd(vm, "==", 'b', cmd_equal, 3, "<SCALAR> == <SCALAR> | <OBJECT> == <OBJECT> | <STRING> == <STRING>", "1 == 1 //true#" "1 == 2 //false#" "_obj1 = \"B_Soldier_F\" createVehicle [0, 0, 0]; _obj1 == _obj1 //true#" "_obj1 = \"B_Soldier_F\" createVehicle [0, 0, 0]; _obj2 = \"B_Soldier_F\" createVehicle [0, 0, 0]; _obj1 == _obj2 //false#" "\"foo\" == \"foo\" //true#" "\"foo\" == \"bar\" //false#", "Check if one value is equal to another. Both values need to be of the same type.");
	create_cmd(vm, "!=", 'b', cmd_notequal, 3, "<SCALAR> != <SCALAR> | <OBJECT> != <OBJECT> | <STRING> != <STRING>", "1 != 1 //false#" "1 != 2 //true#" "_obj1 = \"B_Soldier_F\" createVehicle [0, 0, 0]; _obj1 != _obj1 //false#" "_obj1 = \"B_Soldier_F\" createVehicle [0, 0, 0]; _obj2 = \"B_Soldier_F\" createVehicle [0, 0, 0]; _obj1 != _obj2 //true#" "\"foo\" != \"foo\" //false#" "\"foo\" != \"bar\" //true#", "Returns whether one value is not equal to another.");
	create_cmd(vm, "||", 'b', cmd_oror, 1, "<BOOL> || <BOOL> | <BOOL> || <CODE>", "true || true //true#" "true || false //true#" "false || false //false#" "true || {false} //true#" "false || {true} //true#" "false || {false} //false#"  "true or {true} //true#", "Returns true only if one or both conditions are true.In case of the alternative syntax, lazy evaluation is used(if left operand is true, evaluation of the right side is skipped completely).Identical to : a or b");
	create_cmd(vm, "&&", 'b', cmd_andand, 2, "<BOOL> && <BOOL> | <BOOL> && <CODE>", "true && true //true#" "true && false //false#" "false && false //false#" "true && {false} //false#" "false && {true} //false#" "false && {false} //false#" "true && {true} //true#", "Returns true only if both conditions are true. In case of the alternative syntax, lazy evaluation is used (if left operand is false, evaluation of the right side is skipped completely). Identical to: a and b");
	create_cmd(vm, "or", 'b', cmd_oror, 1, "<BOOL> or <BOOL> | <BOOL> or <CODE>", "true or true //true#" "true or false //true#" "false or false //false#" "true or {false} //true#" "false or {true} //true#" "false or {false} //false#"  "true or {true} //true#", "Returns true only if one or both conditions are true. In case of the alternative syntax, lazy evaluation is used (if left operand is true, evaluation of the right side is skipped completely). Identical to: a || b");
	create_cmd(vm, "and", 'b', cmd_andand, 2, "<BOOL> and <BOOL>", "true and true //true#" "true and false //false#" "false and false //false#" "true and {false} //false#" "false and {true} //false#" "false and {false} //false#" "true and {true} //true#", "Returns true only if both conditions are true. In case of the alternative syntax, lazy evaluation is used (if left operand is false, evaluation of the right side is skipped completely). Identical to: a && b");
	create_cmd(vm, "select", 'b', cmd_select, 10, "<ARRAY> select <SCALAR> | <ARRAY> select <BOOL>", "[0] select 0 //0#" "[] select 0 //nil#" "[10, 0] select false //10#" "[10, 0] select true //0#", "Selects an element from an array, config entry from Config or substring from a string or a range from an array.");
	create_cmd(vm, "then", 'b', cmd_then, 4, "<IF> then <ARRAY>", "if true then [0, 1] //0#" "if true then [0, 1] //0# if false then [0, 1] //1#", "First or second element of array is executed depending on result of if condition. Result of the expression executed is returned as a result (result may be Nothing).");
	create_cmd(vm, "else", 'b', cmd_else, 5, "<CODE> else <CODE>", "{} else {} //[{}, {}]#", "Executes else code when if condition returns false");
	create_cmd(vm, "do", 'b', cmd_do, 4, "<WHILE> do <CODE> | <FOR> do <CODE> | <WITH> do <CODE>", "b = 10; a = 0; while {b > a} do {a = a + 1}; a //10#", "Executes code. Used as part of construct, such as while, with, for and switch. The code is always executed in missionNamespace unless do is used with with as an argument.");
	create_cmd(vm, "from", 'b', cmd_from, 4, "<FOR> from <SCALAR>", "", "Continue sequence of for var construct. It sets the start value of the index variable.");
	create_cmd(vm, "to", 'b', cmd_to, 4, "<FOR> to <SCALAR>", "", "Continue sequence of for var construct. It sets the end value of the index variable");
	create_cmd(vm, "step", 'b', cmd_step, 4, "<FOR> step <SCALAR>", "", "Optionally can set step. If you want to count down, step must be specified, and set negative. Default value is 1.");
	create_cmd(vm, "count", 'b', cmd_count, 4, "<CODE> count <ARRAY> | <COUNT> count <STRING>", "{_x > 2} count [1, 2, 3, 4, 5] //3#", "Can be used to count the number of elements in array with a condition.");
	create_cmd(vm, "call", 'b', cmd_call, 4, "<ANY> call <CODE>", "[1] call {_this} //[1]#", "Executes given set of compiled instructions with an argument passed to the executed code in magic variable _this.");
	create_cmd(vm, "atan2", 'b', cmd_atan2, 7, "<SCALAR> atan2 <SCALAR>", "5 atan2 3 //59.0362#", "ArcTangent of x/y. Used to determine the angle of a vector [x,y]. Result in Degrees between -180 and 180. Note that this command can handle y being 0, unlike when using atan, and will return 90");
	create_cmd(vm, "min", 'b', cmd_min, 6, "<SCALAR> min <SCALAR>", "1 min 2 //1#" "10 min 5 //5#", "The smaller of a, b");
	create_cmd(vm, "max", 'b', cmd_max, 6, "<SCALAR> max <SCALAR>", "1 max 2 //2#" "10 max 5 //10#", "The greater of a, b");
	create_cmd(vm, "mod", 'b', cmd_mod, 7, "<SCALAR> mod <SCALAR>", "1 max 2 //1#" "10 max 5 //0#" "3 mod 2 //1#", "Remainder of a divided by b. Identical to: a % b");
	create_cmd(vm, "%", 'b', cmd_mod, 7, "<SCALAR> % <SCALAR>", "1 % 2 //1#" "10 % 5 //0#" "3 % 2 //1#", "Remainder of a divided by b. Identical to: a mod b");
	create_cmd(vm, "^", 'b', cmd_powerof, 9, "<SCALAR> ^ <SCALAR>", "5 ^ 2 //25#", "a raised to the power of b");
	create_cmd(vm, "pushBack", 'b', cmd_pushback, 4, "<ARRAY> pushBack <ANY>", "[] pushBack 1 //0#" "[1, 2, 3] pushBack 4 // 3#", "Insert an element to the back of the given array. This command modifies the original array.");
	create_cmd(vm, "set", 'b', cmd_set, 4, "<ARRAY> set <ANY>", "_arr = [10]; _arr set [0, 9]; _arr //[9]#" "_arr = []; _arr set [0, 1]; _arr //[1]#", "Changes the element at the given(zero - based) index of the array.If the element does not exist, resize index + 1 is called to create it.");
	create_cmd(vm, "isEqualTo", 'b', cmd_isequalto, 4, "<ANY> isEqualTo <ANY>", "1 isEqualTo 2 //false#", "Performs strict comparison between var1 and var2 and returns true if equal, otherwise false. Strict means that it would check that both arguments are of the same data type and then compare the values.");
	create_cmd(vm, "createVehicle", 'b', cmd_createvehicle, 4, "<STRING> createVehicle <ARRAY>", "", "Creates an empty object of given classname type");
	create_cmd(vm, "getVariable", 'b', cmd_getvariable, 4, "<NAMESPACE> getVariable <STRING> | <NAMESPACE> getVariable <ARRAY> | <OBJECT> getVariable <STRING> | <OBJECT> getVariable <ARRAY>", "", "Return the value of variable in the variable space assigned to various data types.");
	create_cmd(vm, "setVariable", 'b', cmd_setvariable, 4, "<NAMESPACE> setVariable <ARRAY> | <OBJECT> setVariable <ARRAY>", "", "Set variable to given value in the variable space of given element.");
	create_cmd(vm, "setPos", 'b', cmd_setpos, 4, " <OBJECT> setPos <ARRAY>", "_obj = \"B_Soldier_F\" createVehicle [0, 0, 0]; _obj setPos [0, 0, 10]; position _obj //[0, 0, 10]#", "Sets object position.");
	create_cmd(vm, "setVelocity", 'b', cmd_setvelocity, 4, " <OBJECT> setVelocity <ARRAY>", "_obj = \"B_Soldier_F\" createVehicle [0, 0, 0]; _obj setVelocity [0, 0, 10]; velocity _obj //[0, 0, 10]#", "Set velocity (speed vector) of a vehicle. Units are in metres per second.");
	create_cmd(vm, "forEach", 'b', cmd_foreach, 4, "<CODE> forEach <ARRAY> | <COUNT> forEach <ARRAY>", "", "Executes the given command(s) on every item of an array. The array items are represented by the magic variable _x.The array indices are represented by _forEachIndex.");
	create_cmd(vm, ":", 'b', cmd_caseoperator, 4, "<SWITCH> : <CODE>", "", "See switch");
	create_cmd(vm, "doMove", 'b', cmd_domove, 4, "<OBJECT> doMove <ARRAY> | <ARRAY> doMove <ARRAY>", "", "Order the given unit(s) to move to the given position (without radio messages).");
	create_cmd(vm, "isEqualType", 'b', cmd_isequaltype, 4, "<ANY> isEqualType <ANY>", "[1, 2] isEqualType 1 //false#" "[1, 2] isEqualType [] //true#" "false isEqualType 1 //false#" "false isEqualType true //true#", "Compares 2 values by their type. A much faster alternative to typeName a == typeName b.");
	create_cmd(vm, "params", 'b', cmd_params, 4, "<ANY> params <ARRAY>", "position (\"B_Soldier_F\" createVehicle [10, 20, 30]) params [\"\", \"\", \"_z\"]; _z" "[1, nil, 2] params [\"_var1\", [\"_var2\", 23], \"_var3\"]; _var2 //23#" "private _paramsres = [1, 2] params [\"_var1\", \"_var2\", [\"_var3\", true, [true]]]; [_var3, _paramsres] //[true, false]#" "private _paramsres = [1, 2, 3] params [\"_var1\", \"_var2\", [\"_var3\", true, [true]]]; [_var3, _paramsres] //[true, false]#" "private _paramsres = [1, \"ok\", [1, 2, 3]] params [[\"_var1\", 0, [0]], [\"_var2\", "", [""]], [\"_var3\", [0,0,0], [[], objNull, 0], [2,3]]]; _paramsres //true#", "Parses input argument into array of private variables. When used without argument, as shown in main syntax, internal variable _this, which is usually available inside functions and event handlers, is used as argument.In addition to simple parsing directly into variables, input can be tested in case it is undefined, of the wrong type or of the wrong size(if array) and substituted if necessary with default values.");
	create_cmd(vm, "createUnit", 'b', cmd_createunit, 4, "<GROUP> createUnit <ARRAY", "", "Create unit of a class that's defined in CfgVehicles. The Group parameter MUST be an existing group or the unit won't be created.");
	create_cmd(vm, "spawn", 'b', cmd_spawn, 4, "<ANY> spawn <CODE>", "", "Adds given code to the scheduler. Exactly when the code will be executed is unknown, it depends on how busy is the engine and how filled up is the scheduler. Therefore spawn does not wait for the supplied code to finish, instead, spawn returns a Script handle to the scheduler task. scriptDone command can be used to check the code completion. Additional arguments are passed to the code in local variable _this. Since Arma 3 v1.55 the script handle also exists inside the code in _thisScript variable.");
	create_cmd(vm, "resize", 'b', cmd_resize, 4, "<ARRAY> resize <SCALAR>", "_arrayNum = [0,1,2,3,4]; _arrayNum resize 2; // _arrayNum is now [0,1]", "Changes the size of the given array. The command does not return new array, it resizes the source array to the desired number of elements. If the new size is bigger than the current size, the new places are filled with nils.");
	create_cmd(vm, "deleteAt", 'b', cmd_deleteat, 4, "<ARRAY> deleteAt <SCALAR>", "", "");
	create_cmd(vm, "append", 'b', cmd_append, 4, "<ARRAY> append <ARRAY>", "_arr = [1,2,3]; _arr append [4,5,6]; hint str _arr; //[1,2,3,4,5,6]", "Appends array2 to the back of array1 modifying array1. NOTE: append does not return array, it modifies existing array. If you need to return a copy, use \"+\"");
	create_cmd(vm, "find", 'b', cmd_find, 4, "<ARRAY> find <ANY>#" "<STRING> find <STRING>", "[\"Apples\",\"Oranges\",\"Pears\"] find \"Oranges\"; //result is 1#" "[1,[2],[[3]]] find [[3]]; //result is 2#" "if (magazines player find \"Strela\" >= 0) then {hint \"You've got Strela!\"};#" "hint str (\"japa is the man!\" find \"the man!\"); //8", "Searches for an array element within array or a string within a string. Returns the 0 based index on success or -1 if not found. Test is cASe-seNsItiVE ");
	create_cmd(vm, "arrayIntersect", 'b', cmd_arrayintersect, 4, "<ARRAY> arrayIntersect <ARRAY>", "_arr1 = [1,2,3,4,5,2,3,4]; _arr2 = [4,5,6,1,2,3,5,6]; hint str (_arr1 arrayIntersect _arr2); // [4,5,1,2,3]", "Intersects array1 with array2 returning array of unique common elements. Additionally, using the same array for array1 and array2 will simply return array of unique elements. Intersects only 1st dimension of an array.");

	create_cmd(vm, "diag_log", 'u', cmd_diag_LOG, 4, "diag_log <ANY>", "", "Dumps the argument's value. Each call creates a new line.");
	create_cmd(vm, "systemChat", 'u', cmd_systemchat, 4, "systemChat <STRING>", "", "Writes the argument's value plaintext. Each call creates a new line.");
	create_cmd(vm, "hint", 'u', cmd_hint, 4, "hint <STRING>", "", "Writes the argument's value plaintext. Each call creates a new line.");
	create_cmd(vm, "private", 'u', cmd_private, 4, "private <STRING> | private <ARRAY>", "_a = 10; [] call { private \"_a\"; _a = 20; }; _a //10#" "_a = 10; _b = 20; [] call { private [\"_a\", \"_b\"]; _a = 50; _b = 50; }; [_a, _b] //[10, 20]#", "Sets a variable to the innermost scope");
	create_cmd(vm, "if", 'u', cmd_if, 4, "if <BOOL>", "", "See then");
	create_cmd(vm, "str", 'u', cmd_str, 4, "str <ANY>", "", "Converts any value into a string by placing \" and \" around the argument.");
	create_cmd(vm, "while", 'u', cmd_while, 4, "while <CODE>", "while {b > a} //{b > a}#", "Repeats Code while condition is true. A part of while do construct.");
	create_cmd(vm, "typeName", 'u', cmd_typename, 4, "typeName <ANY>", "", "Returns the data type of an expression. The type is returned as all-uppercase string.");
	create_cmd(vm, "for", 'u', cmd_for, 4, "for <STRING>", "_a = 0; for \"_i\" from 1 to 10 do { _a = _a + _i; }; _a //45#", "This operator creates a For Type which is used in the for-constructs.");
	create_cmd(vm, "+", 'u', cmd_plus_UNARY, 4, "+ <ARRAY> | + <SCALAR> | + <NaN>", "+1 //1#" "+[10] //[10]#", "Unary plus - returns a copy of valuea.");
	create_cmd(vm, "-", 'u', cmd_minus_UNARY, 4, "- <SCALAR>", "-1 //-1#" "_a = 1; -_a //-1#", "Unary minus : zero minus a");
	create_cmd(vm, "count", 'u', cmd_count_UNARY, 4, "count <STRING> | count <ARRAY>", "count [0, 1, 2] //3#" "count \"foobar\" //6#", "Can be used to count the number of elements in array or the number of characters in a string");
	create_cmd(vm, "format", 'u', cmd_format, 4, "format <ARRAY>", "format [\"%1 - %2 = %3\", 3, 2, 1] //\"3 - 2 = 1\"", "Composes a string containing other variables or other variable types. Converts any variable type to a string.");
	create_cmd(vm, "call", 'u', cmd_call_UNARY, 4, "call <CODE>", "call {1} //1#", "Executes given set of compiled instructions.");
	create_cmd(vm, "abs", 'u', cmd_abs, 4, "abs <SCALAR>", "abs -3 //3#", "Absolute value of a real number");
	create_cmd(vm, "deg", 'u', cmd_deg, 4, "deg <SCALAR>", "deg 1 //57.295#", "Convert a number from Radians to Degrees.");
	create_cmd(vm, "log", 'u', cmd_log, 4, "log <SCALAR>", "log 10 //1#", "Base-10 logarithm of x.");
	create_cmd(vm, "sin", 'u', cmd_sin, 4, "sin <SCALAR>", "sin 30 //0.5#", "Sine of x, argument in Degrees.");
	create_cmd(vm, "cos", 'u', cmd_cos, 4, "cos <SCALAR>", "cos 60 //0.5#", "Cosine of a number, argument in degrees.");
	create_cmd(vm, "tan", 'u', cmd_tan, 4, "tan <SCALAR>", "tan 45 //1#", "Tangent of x, argument in Degrees.");
	create_cmd(vm, "exp", 'u', cmd_exp, 4, "exp <SCALAR>", "1 //2.7182#", "Let x be a number, then exp (x) is equal to e to the power of x (or e^x)");
	create_cmd(vm, "asin", 'u', cmd_asin, 4, "asin <SCALAR>", "asin 0.5 //30#", "Arcsine of a number, result in Degrees.");
	create_cmd(vm, "acos", 'u', cmd_acos, 4, "acos <SCALAR>", "acos 0.5 //60#", "ArcCosine of a number, result in Degrees.");
	create_cmd(vm, "atan", 'u', cmd_atan, 4, "atan <SCALAR>", "atan 1 //45#", "ArcTangent of a number, result in Degrees. Identical to atg.");
	create_cmd(vm, "atg", 'u', cmd_atan, 4, "atg <SCALAR>", "atan 1 //45#", "ArcTangent of a number, result in Degrees. Identical to atan.");
	create_cmd(vm, "rad", 'u', cmd_rad, 4, "rad <SCALAR>", "rad 180 //3.1415#", "Convert x from Degrees to Radians. 360 degrees is equal to 2 multiplied with pi.");
	create_cmd(vm, "sqrt", 'u', cmd_sqrt, 4, "sqrt <SCALAR>", "sqrt 9 //3#", "Returns square root of x.");
	create_cmd(vm, "ceil", 'u', cmd_ceil, 4, "ceil <SCALAR>", "ceil 5.25 //6#" "ceil -5.25 //-5#", "The ceil value of x.");
	create_cmd(vm, "random", 'u', cmd_random, 4, "random <SCALAR>", "", "Random real (floating point) value from 0 (inclusive) to x (not inclusive).");
	create_cmd(vm, "floor", 'u', cmd_floor, 4, "floor <SCALAR>", "ceil 5.25 //5#" "ceil -5.25 //-6#", "Returns the next lowest integer in relation to x.");
	create_cmd(vm, "ln", 'u', cmd_ln, 4, "ln <SCALAR>", "ln 10 //2.302#", "Natural logarithm of x.");
	create_cmd(vm, "round", 'u', cmd_round, 4, "round <SCALAR>", "round 5.25 //5#" "round 5.55 //6#", "Rounds up or down to the closest integer.");
	create_cmd(vm, "!", 'u', cmd_not, 4, "! <BOOL>", "!true //false#" "!false //true#", "Returns a negation of Boolean expression. That means true becomes false and vice versa.");
	create_cmd(vm, "comment", 'u', cmd_comment, 4, "comment <BOOL>", "", "Define a comment. Mainly used in SQF Syntax, as you're able to introduce comment lines with semicolons in a SQS script.");
	create_cmd(vm, "typeOf", 'u', cmd_typeof, 4, "typeOf <OBJECT>", "typeOf (\"B_Soldier_F\" createVehicle [0, 0, 0]) //\"B_Soldier_F\"#", "Returns the config class name of given object or "" if object doesn't have config class.");
	create_cmd(vm, "getPos", 'u', cmd_getpos, 4, "getPos <OBJECT>", "_obj = \"B_Soldier_F\" createVehicle [0, 0, 0]; getPos _obj //[0, 0, 0]#", "Returns the object position in format PositionAGLS. Z value is height over the surface underneath. Identical to position");
	create_cmd(vm, "position", 'u', cmd_getpos, 4, "position <OBJECT>", "_obj = \"B_Soldier_F\" createVehicle [0, 0, 0]; position _obj //[0, 0, 0]#", "Returns the object position in format PositionAGLS. Z value is height over the surface underneath. Identical to getPos");
	create_cmd(vm, "velocity", 'u', cmd_velocity, 4, "velocity <OBJECT>", "_obj = \"B_Soldier_F\" createVehicle [0, 0, 0]; _obj setVelocity [0, 0, 10]; velocity _obj //[0, 0, 10]#", "Return velocity (speed vector) of Unit as an array with format [x, y, z]. Units are in metres per second.");
	create_cmd(vm, "switch", 'u', cmd_switch, 4, "switch <ANY>", "switch (1) do { case 1: { 10 }; case 2: { 22 }; default { false }; //10#" "switch (0) do { case 1: { 10 }; case 2: { 22 }; default { false }; //false#", "Creates a switch type that can be used inside a switch do -block");
	create_cmd(vm, "case", 'u', cmd_case, 4, "case <ANY>", "", "See switch");
	create_cmd(vm, "default", 'u', cmd_default, 4, "default <CODE>", "", "See switch");
	create_cmd(vm, "allVariables", 'u', cmd_allvariables, 4, "allVariables <OBJECT> | allVariables <NAMESPACE> | allVariables <GROUP>", "", "Returns a list of all variables from desired namespace.");
	create_cmd(vm, "with", 'u', cmd_with, 4, "with <NAMESPACE>", "", "Creates a With Type that is used inside a do construct in order to execute code inside a given namespace.");
	create_cmd(vm, "compile", 'u', cmd_compile, 4, "compile <STRING>", "a = 10; call compile \"a = 20;\"; a //20#" "compile \"diag_log 10;\" //{diag_log 10;}#", "Compile expression into executable code.");
	create_cmd(vm, "toArray", 'u', cmd_toarray, 4, "toArray <STRING>", "toArray \"test\" //[116, 101, 115, 116]#", "Converts the supplied String into an Array of Numbers.");
	create_cmd(vm, "toString", 'u', cmd_tostring, 4, "toString <ARRAY>", "toString [116, 101, 115, 116] //\"test\"#", "Converts the supplied Array of Numbers into a String.");
	create_cmd(vm, "params", 'u', cmd_params_UNARY, 4, "params <ARRAY>", "[1, 2, 3] call { params [\"_one\", \"_two\", \"_three\"]; _two} //2#" "123 call { params [\"_myvar\"]; _myvar } //123#" "[1, 2] call { private _paramsres = params [\"_var1\", \"_var2\", [\"_var3\", true, [true]]]; [_var3, _paramsres] }; //[true, false]#" "[1, 2, 3] call { private _paramsres = params [\"_var1\", \"_var2\", [\"_var3\", true, [true]]]; [_var3, _paramsres] }; //[true, false]#" "[1, \"ok\", [1, 2, 3]] call { private _paramsres = params [[\"_var1\", 0, [0]], [\"_var2\", "", [""]], [\"_var3\", [0,0,0], [[], objNull, 0], [2,3]]]; _paramsres}; //true#", "Parses input argument into array of private variables. When used without argument, as shown in main syntax, internal variable _this, which is usually available inside functions and event handlers, is used as argument.In addition to simple parsing directly into variables, input can be tested in case it is undefined, of the wrong type or of the wrong size(if array) and substituted if necessary with default values.");
	create_cmd(vm, "isNil", 'u', cmd_isnil, 4, "isNil <STRING> | isNil <CODE>", "isNil \"_foo\" //true#" "_foo = 10; isNil \"_foo\" //false#" "isNil { nil } //true#" "isNil { 10 } //false#", "Tests whether the variable defined by the String argument is undefined, or whether an expression result passed as Code is undefined. The command returns true if the variable or the expression result is undefined(i.e.the expression result is Void), and false in all other cases.");
	create_cmd(vm, "deleteVehicle", 'u', cmd_deletevehicle, 4, "deleteVehicle <OBJECT>", "", "Deletes an object.");
	create_cmd(vm, "createGroup", 'u', cmd_creategroup, 4, "createGroup <SIDE>", "", "Creates a new Group for the given Side.");
	create_cmd(vm, "deleteGroup", 'u', cmd_deletegroup, 4, "deleteGroup <GROUP>", "", "Destroys the given group. Group must be empty.");
	create_cmd(vm, "groupId", 'u', cmd_groupid, 4, "groupId <GROUP>", "", "Returns group name.");
	create_cmd(vm, "units", 'u', cmd_units, 4, "units <GROUP>", "", "Returns an array with all the units in the group or group of the unit.");
	create_cmd(vm, "scriptDone", 'u', cmd_scriptdone, 4, "scriptDone <SCRIPT>", "", "Check if a script is finished running using the Script_(Handle) returned by execVM or spawn.");
	create_cmd(vm, "selectRandom", 'u', cmd_selectrandom, 4, "selectRandom <ARRAY>", "_randomElement = selectRandom [1,2,3,4,5];", "Returns a random element from the given array. Engine solution to BIS_fnc_selectRandom");
	create_cmd(vm, "reverse", 'u', cmd_reverse, 4, "reverse <ARRAY>", "_arr = [1,2,3]; reverse _arr; hint str _arr; //[3,2,1]", "Reverses given array by reference (modifies the original array, just like resize). ");


	create_cmd(vm, "true", 'n', cmd_true, 4, "true", "", "");
	create_cmd(vm, "false", 'n', cmd_false, 4, "false", "", "");
	create_cmd(vm, "parsingNamespace", 'n', cmd_parsingnamespace, 4, "parsingNamespace", "", "");
	create_cmd(vm, "missionNamespace", 'n', cmd_missionnamespace, 4, "missionNamespace", "", "");
	create_cmd(vm, "uiNamespace", 'n', cmd_uinamespace, 4, "uiNamespace", "", "");
	create_cmd(vm, "profileNamespace", 'n', cmd_profilenamespace, 4, "profileNamespace", "", "");
	create_cmd(vm, "diag_tickTime", 'n', cmd_diag_TICKTIME, 4, "diag_tickTime", "", "");
	create_cmd(vm, "pi", 'n', cmd_pi, 4, "pi", "", "");
	create_cmd(vm, "productVersion", 'n', cmd_productversion, 4, "productVersion", "", "");
	create_cmd(vm, "west", 'n', cmd_west, 4, "west", "", "");
	create_cmd(vm, "blufor", 'n', cmd_blufor, 4, "blufor", "", "");
	create_cmd(vm, "east", 'n', cmd_east, 4, "east", "", "");
	create_cmd(vm, "opfor", 'n', cmd_opfor, 4, "opfor", "", "");
	create_cmd(vm, "resistance", 'n', cmd_resistance, 4, "resistance", "", "");
	create_cmd(vm, "independent", 'n', cmd_independent, 4, "independent", "", "");
	create_cmd(vm, "sideUnknown", 'n', cmd_sideunknown, 4, "sideUnknown", "", "");
	create_cmd(vm, "sideLogic", 'n', cmd_sidelogic, 4, "sideLogic", "", "");
	create_cmd(vm, "sideFriendly", 'n', cmd_sidefriendly, 4, "sideFriendly", "", "");
	create_cmd(vm, "sideEnemy", 'n', cmd_sideenemy, 4, "sideEnemy", "", "");
	create_cmd(vm, "civilian", 'n', cmd_civilian, 4, "civilian", "", "");
	create_cmd(vm, "sideEmpty", 'n', cmd_sideempty, 4, "sideEmpty", "", "");
	create_cmd(vm, "objNull", 'n', cmd_objnull, 4, "objNull", "", "");
	create_cmd(vm, "allGroups", 'n', cmd_allgroups, 4, "allGroups", "", "Return a list of all groups created. Unlike ArmA, contains groups of all sides.");
	create_cmd(vm, "nil", 'n', cmd_nil, 4, "nil", "", "Nil value. This value can be used to undefine existing variables.");



	create_cmd(vm, "help__", 'n', cmd_help, 4, "help__", "help__ //nil#", "Displays all commands available with usage, precedence and type.");
	create_cmd(vm, "help__", 'u', cmd_help_UNARY, 4, "help__ <STRING>", "help__ \"typeOf\"//nil#", "Outputs information to given command. Expects a <STRING> on righthand. Usage: help__ \"command\"");
}

int vm_output_print(PVM vm, const char* format, ...)
{
	va_list args;
	va_list args_bullshittery;
	int len;
	char* buff;
	va_start(args, format);
	va_copy(args_bullshittery, args);
	len = vsnprintf(0, 0, format, args);
	if (len < 500)
	{
		buff = alloca(sizeof(char) * (len + 1));
		vsnprintf(buff, len + 1, format, args_bullshittery);
		string_modify_append(vm->print_custom_data, buff);
	}
	else
	{
		buff = malloc(sizeof(char) * (len + 1));
		vsnprintf(buff, len + 1, format, args_bullshittery);
		string_modify_append(vm->print_custom_data, buff);
		free(buff);
	}
	va_end(args);
	va_end(args_bullshittery);
	return len;
}

DLLEXPORT_PREFIX unsigned char start_program(const char* input, unsigned long max_instructions, char* buffer, size_t buffer_size)
{
	int val;
	int i;
	unsigned char success;
	PVM vm = sqfvm(10000, 50, 1, max_instructions);
	vm->error = custom_error;
	vm->warn = custom_warn;
	if (systime_start == 0)
	{
		systime_start = system_time_ms();
	}


	register_commmands(vm);
	vm->print = vm_output_print;
	vm->print_custom_data = string_create(0);

	val = setjmp(program_exit);
	if (!val)
	{
		push_stack(vm, vm->stack, inst_scope("all"));
		parse(vm, input, 1);
		runvm(vm);
		success = vm->die_flag;
	}
	if (vm->work->top != 0)
	{
		for (i = 0; i < vm->work->top; i++)
		{
			if (vm->work->data[i]->type == INST_VALUE)
			{
				if (get_value(vm, vm->stack, vm->work->data[i])->type == NOTHING_TYPE())
					continue;
				vm->print(vm, "[WORK]: ");
				stringify_value(vm, vm->print_custom_data, get_value(vm, vm->stack, vm->work->data[i]));
				vm->print(vm, "\n");
			}
		}
	}
	vm->print(vm, "Finished in %d instructions\n", vm->instcount);
	if (((PSTRING)vm->print_custom_data)->val != 0)
	{
		if (buffer == 0 || buffer_size == 0)
		{
			printf(((PSTRING)vm->print_custom_data)->val);
		}
		else
		{
			strncpy(buffer, ((PSTRING)vm->print_custom_data)->val, buffer_size);
		}
	}
	else
	{
		if (buffer == 0 || buffer_size == 0)
		{
			printf("<EMPTY>\n");
		}
		else
		{
			strncpy(buffer, "<EMPTY>\n", buffer_size);
		}
	}
	string_destroy(vm->print_custom_data);
	destroy_sqfvm(vm);
	return success;
}


#define RETCDE_OK 0
#define RETCDE_ERROR 1
#define RETCDE_RUNTIME_ERROR 2

int load_file(PSTRING buffer, const char* fpath)
{
	FILE* fptr = fopen(fpath, "r");
	size_t size;
	size_t curlen = buffer->length;
	int tailing = 0;
	int lcount = 1;
	if (fptr == 0)
	{
		printf("[ERR] Could not open file '%s'", fpath);
		return -1;
	}
	fseek(fptr, 0, SEEK_END);
	size = ftell(fptr);;
	rewind(fptr);
	string_modify_append2(buffer, size);
	memset(buffer->val + curlen, 0, sizeof(char) * size);
	fread(buffer->val + curlen, sizeof(char), size, fptr);
	for (; curlen < buffer->length; curlen++)
	{
		if (buffer->val[curlen] == '\n')
			lcount++;
		else if (buffer->val[curlen] == '\0')
			tailing++;
	}
	if (tailing > 0)
	{
		string_modify_append2(buffer, -tailing);
	}
	return lcount;
}

int main(int argc, char** argv)
{
	char linebuffer[LINEBUFFER_SIZE];
	const char* ptr;
	int i, j, k;
	unsigned char just_execute = 0;
	unsigned char prog_success = 0;
	PSTRING pstr;
	unsigned long max_inst = 10000;
	pstr = string_create(0);
#if _WIN32 & _DEBUG
	//_CrtSetBreakAlloc(832);
#endif
	j = 0;

	for (i = 0; i < argc; i++)
	{
		ptr = argv[i];
		if (ptr[0] == '-')
		{
			switch (ptr[1])
			{
			case '\0':
				printf("[ERR] empty argument passed.");
				return RETCDE_ERROR;
			case '?':
				printf("SQF-VM Help page\n");
				printf("./prog [-j] [-s 10000] [-i <FILE>] [-I <CODE>]\n");
				printf("\t-?\tOutputs this help\n");
				printf("\t-i\tLoads provided input file into the code-buffer.\n");
				printf("\t-I\tLoads provided input SQF code into the code-buffer.\n");
				printf("\t-a\tDisables user input and just executes the code-buffer.\n");
				printf("\t-s\tSets the maximum instruction count allowed before termination.\n");
				printf("\t  \tMaximum value is %lu.\n", LONG_MAX);
				printf("\t  \tIf `0` is passed, the limit will be disabled.\n");
				return RETCDE_OK;
				break;
			case 'i':
				if (i + 1 < argc)
				{
					k = load_file(pstr, argv[++i]);
					if (k < 0)
						return RETCDE_ERROR;
					j += k;
				}
				else
				{
					printf("[ERR] -i empty parameter");
					return RETCDE_ERROR;
				}
				break;
			case 'I':
				if (i + 1 < argc)
				{
					string_modify_append(pstr, argv[++i]);
					j++;
				}
				else
				{
					printf("[ERR] -I empty parameter");
					return RETCDE_ERROR;
				}
				break;
			case 'a':
				just_execute = 1;
				break;
			case 's':
				if (i + 1 < argc)
				{
					max_inst = strtoul(argv[i + 1], 0, 10);
					j++;
				}
				else
				{
					printf("[ERR] -I empty parameter");
					return RETCDE_ERROR;
				}
				break;
			default:
				break;
			}
		}
	}
	ptr = 0;
	i = j;

	if (!just_execute)
	{
		printf("Please enter your SQF code.\nTo get the capabilities, use the `help__` instruction.\nTo run the code, Press <ENTER> twice.\n");
		printf("%d:\t", i++);
		while (get_line(linebuffer, LINEBUFFER_SIZE)[0] != '\n')
		{
			string_modify_append(pstr, linebuffer);
			printf("%d:\t", i++);
		}
		//string_modify_append(pstr, "diag_log str [1, 2, \"test\", [1, 2, 3]]");
	}
	if (just_execute)
	{
		if (pstr->length > 0)
			prog_success = start_program(pstr->val, max_inst, 0, 0);
	}
	else
	{
		printf("-------------------------------------\n");
		if (pstr->length > 0)
			prog_success = start_program(pstr->val, max_inst, 0, 0);
		printf("-------------------------------------\n");
		printf("Press <ENTER> to finish.");
		get_line(linebuffer, LINEBUFFER_SIZE);
	}
	string_destroy(pstr);
	namespace_destroy(sqf_missionNamespace());
	namespace_destroy(sqf_parsingNamespace());
	namespace_destroy(sqf_profileNamespace());
	namespace_destroy(sqf_uiNamespace());

	destroy_cmdcnt(GET_PCMDCNT());

#if _WIN32 & _DEBUG
	_CrtDumpMemoryLeaks();
#endif
	return prog_success ? RETCDE_OK : RETCDE_RUNTIME_ERROR;
}
void cmd_productversion(void* input, CPCMD self)
{
	PVM vm = input;

	//String(Arma 3) - Architecture - x64 for 64bit or x86 for 32bit Since version 1.67
	push_stack(vm, vm->stack, inst_arr_push());
	push_stack(vm, vm->stack, inst_value(value(STRING_TYPE(), base_voidptr(string_create2("x86")))));
	//String(Arma 3) - Platform("Windows", "Linux" or "OSX")
	push_stack(vm, vm->stack, inst_arr_push());
#if !_WIN32 & !__linux
	push_stack(vm, vm->stack, inst_value(value(STRING_TYPE(), base_voidptr(string_create2("UNKNOWN")))));
#else
#ifdef _WIN32
	push_stack(vm, vm->stack, inst_value(value(STRING_TYPE(), base_voidptr(string_create2("WINDOWS")))));
#endif // _WIN32
#ifdef __linux
	push_stack(vm, vm->stack, inst_value(value(STRING_TYPE(), base_voidptr(string_create2("UNIX")))));
#endif
#endif
	//Boolean(Arma 3) - Product is launched using mods true or vanilla false
	push_stack(vm, vm->stack, inst_arr_push());
	push_stack(vm, vm->stack, inst_value(value(BOOL_TYPE(), base_int(0))));
	//String(Arma 3) - Product Branch
	push_stack(vm, vm->stack, inst_arr_push());
	push_stack(vm, vm->stack, inst_value(value(STRING_TYPE(), base_voidptr(string_create2("COMMUNITY")))));
	//Number - Product Build Number
	push_stack(vm, vm->stack, inst_arr_push());
	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(3))));
	//Number - Product Version Number
	push_stack(vm, vm->stack, inst_arr_push());
	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(01))));
	//String - Product Name Short
	push_stack(vm, vm->stack, inst_arr_push());
	push_stack(vm, vm->stack, inst_value(value(STRING_TYPE(), base_voidptr(string_create2("SQF-VM")))));
	//String - Product Name-
	push_stack(vm, vm->stack, inst_arr_push());
	push_stack(vm, vm->stack, inst_value(value(STRING_TYPE(), base_voidptr(string_create2("SQF-VM (0.1.4 ALPHA)")))));


	push_stack(vm, vm->stack, inst_value(value(ARRAY_TYPE(), base_voidptr(array_create2(8)))));
}
