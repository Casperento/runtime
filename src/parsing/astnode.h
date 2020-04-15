#pragma once
#include <string>
#include <vector>


namespace sqf
{
	namespace parse
	{
		struct astnode
		{
			size_t offset;
			size_t length;
			size_t line;
			size_t col;
			std::string content;
			std::string file;
			short kind;
			std::vector<astnode> children;

			astnode() : offset(0), length(0), line(0), col(0), kind(0) {}
		};

		void print_navigate_ast(std::basic_ostream<char, std::char_traits<char>>* outstreamptr, const astnode& node, const char* (*astkindname)(short), unsigned short level = 0);
	}
}