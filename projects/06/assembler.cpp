#include <stdio.h>
#include <fstream>
#include <sstream>
#include <string>
#include <bitset>
#include <map>
#include <cmath>

#define MAX_ADDR 32767

using string = std::string;

enum InstructionType
{
	ADDRESS, COMPUTATION, LABEL
};

std::map<string, uint32_t> symbolsTable;
uint16_t lastUsed = 15;

uint16_t ParseAddress(string &in, size_t lineNumber, string &error, bool &valid)
{
	if (in.length() == 1)
	{
		valid = false;
		
		std::stringstream errorText;
		errorText << "Malformed address instruction on line " << lineNumber << ": No address or label provided";
		
		error = errorText.str();
		return 0;
	}

	std::stringstream parser(in.substr(1));

	if (isspace(parser.peek()))	valid = false;

	string tok;
	parser >> tok;

	if (!parser.eof()) 
	{
		valid = false;
		error = "Unexpected symbol after address token";
	}

	enum TokenType
	{
		LABELTOK, NUMTOK
	} type = isdigit(tok[0]) ? NUMTOK : LABELTOK;

	for (auto c : tok)
	{
		if (!valid) break;
		
		if (type == NUMTOK && !isdigit(c))
		{
			valid = false;
			
			std::stringstream errorText;
			errorText << "Character " << c << " is not a digit, but token starts with a digit. Labels must start with an alphabet and only contain alphanumerical values or underscores";
			error = errorText.str();
		}
		else if (type == LABELTOK && !(isalnum(c) || c == '_'))
		{
			valid = false;

			std::stringstream errorText;
			errorText << "Character " << c << " is not alphanumerical or an underscore";
			error = errorText.str();
		}
	}

	if (!valid)
	{
		std::stringstream errorText;
		errorText << "Malformed address instruction on line " << lineNumber << ": " << (error.empty() ? "Unexpected symbol after @" : error);
		
		error = errorText.str();
		return 0;
	}

	if (type == NUMTOK)
	{
		int val = std::stoi(tok);
		if (val > MAX_ADDR)
		{
			valid = false;

			std::stringstream errorText;
			errorText << "Malformed address instruction on line " << lineNumber << ": Address is outside the range of a 15-bit unsigned integer";
			
			error = errorText.str();
		}

		return val;
	}
	else
	{
		if (symbolsTable.find(tok) != symbolsTable.end()) return symbolsTable.at(tok);

		symbolsTable.insert({tok, ++lastUsed});
		return lastUsed;
	}
}

uint16_t ParseComputation(string &in, size_t lineNumber, string &error, bool &valid)
{
	
}

uint16_t ParseLabel(string &in, size_t lineNumber, string &error, bool &valid)
{
	
}

uint16_t (*parseLUT[]) (string &in, size_t lineNumber, string &error, bool &valid) = { ParseAddress, ParseComputation, ParseLabel };

uint16_t ParseStatement(string &in, size_t lineNumber, string &error, bool &valid)
{	
	InstructionType type = COMPUTATION;
	uint16_t statement;
	valid = true;
	error = "";

	size_t start = 0, end;
	size_t len = in.length();

	while (isspace(in[start]) && start < len) { start++; }

	if (in[start] == '@')
	{
		type = ADDRESS;
	}
	else if (in[start] == '(')
	{
		type = LABEL;
	}

	size_t lastNonSpace = start;

	for(end = start; end < len; end++)
	{
		char c = in[end];

		if (c == '/')
		{
			if (end == len - 1 || in[end + 1] != '/')
			{
				std::stringstream errorMessage;
				errorMessage << "Unexpected / on line " << lineNumber;

				error = errorMessage.str();
				valid = false;
				return 0;
			}

			break;
		}

		if (!isspace(c)) lastNonSpace = end;
	}

	if (start == end)
	{
		valid = false;
		return 0;
	}

	if (in[end] == '/') end--;

	end = lastNonSpace;

	string trimmedInstruction = in.substr(start, end - start + 1);

	statement = parseLUT[type](trimmedInstruction, lineNumber, error, valid);

	return statement;
}

int main(const int argc, const char *argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "Insufficient Arguments: Provide path to the file to be assembled.\n");
		return 1;
	}

	std::ifstream asmFile(argv[1]);

	if (!asmFile.is_open())
	{
		fprintf(stderr, "Could not open file %s.\n", argv[1]);
		return 1;
	}

	std::ofstream output("out.hack");

	if (!output.is_open())
	{
		fprintf(stderr, "Could not create output file.\n");
		return 1;
	}

	symbolsTable.clear();
	symbolsTable.insert({
		{"R0", 0}, {"R1", 1}, {"R2", 2}, {"R3", 3}, {"R4", 4},
		{"R5", 5}, {"R6", 6}, {"R7", 7}, {"R8", 8}, {"R9", 9},
		{"R10", 10}, {"R11", 11}, {"R12", 12}, {"R13", 13},
		{"R14", 14}, {"R15", 15}, {"KBD", 32767}, {"SCREEN", 24576}
	});

	size_t lineNumber = 1;

	while (!asmFile.eof())
	{
		string line, error;
		std::getline(asmFile, line);
		bool valid;
		uint16_t statement = ParseStatement(line, lineNumber, error, valid);

		if (valid)
		{
			std::bitset<16> bits(statement);
			output << bits << std::endl;
		}
		else if (!error.empty())
		{	
			fprintf(stderr, "%s\n", error.c_str());
			return 1;	
		}

		lineNumber++;
	}
}