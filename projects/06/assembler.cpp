#include <stdio.h>
#include <fstream>
#include <sstream>
#include <string>
#include <bitset>
#include <map>
#include <vector>
#include <cmath>

#define MAX_ADDR 32767

using string = std::string;

enum InstructionType
{
	ADDRESS, COMPUTATION
}; 

/// Holds mappings from labels and variables to their addresses in memory
std::map<string, uint32_t> symbolsTable;

/// Last used memory address for variables, starts at 15 due to the R0..R15 registers
uint16_t lastUsedAddress = 15;

/// Maps post-preprocessing lines to the original lines in the source code
std::vector<size_t> lineNumberMapping;

/// Parses a given A-type instruction into its equivalent hack machine code
/// An A-type instruction is of the form @[15-bit immediate | label (alphanumerics + underscores)]
uint16_t ParseAddress(string &in, size_t lineNumber, size_t instrNumber, string &error, bool &valid)
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
			errorText << "Character " << c << " is not a digit, but token starts with a digit";
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

		symbolsTable.insert({tok, ++lastUsedAddress});
		return lastUsedAddress;
	}
}

/// Parses a C-type instruction into its equivalent hack machine code
uint16_t ParseComputation(string &in, size_t lineNumber, size_t instrNumber, string &error, bool &valid)
{
	std::bitset<16> output(0b1110000000000000);

	std::stringstream parser(in), errorText;

	string tok;

	std::getline(parser, tok, '=');

	if (!parser.eof())
	{
		for (char c : tok)
		{
			if (isspace(c)) continue;

			switch (c)
			{
				case 'A':
					if (output.test(5))
					{
						valid = false;
						error = "A repeated in destination";
					}

					output.set(5, true);
				break;

				case 'D':
					if (output.test(4))
					{
						valid = false;
						error = "D repeated in destination";
					}

					output.set(4, true);
				break;

				case 'M':
					if (output.test(3))
					{
						valid = false;
						error = "M repeated in destination";
					}

					output.set(3, true);
				break;

				default:
					valid = false;
					errorText << "Invalid character " << c << " used as computation destination";
					error = errorText.str();
				break;					
			}

			if (!valid) break;
		}
	}

	if (!valid)
	{
		errorText.str("");
		errorText << "Malformed computation instruction on line " << lineNumber << ": " << (error.empty() ? "Syntax error" : error);
		
		error = errorText.str();
		return 0;
	}

	return output.to_ulong();
}

/// Lookup table for parsing function based on the type of instruction (enum value => index), sorry for the horrific syntax
uint16_t (*parseLUT[]) (string &in, size_t lineNumber, size_t instrNumber, string &error, bool &valid) = { ParseAddress, ParseComputation };

/// Identifies the type of a statement and parses it accordingly
uint16_t ParseStatement(string &in, size_t lineNumber, size_t instrNumber, string &error, bool &valid)
{	
	InstructionType type = COMPUTATION;
	uint16_t statement;
	valid = true;
	error = "";

	if (in[0] == '@') type = ADDRESS;

	statement = parseLUT[type](in, lineNumber, instrNumber, error, valid);

	return statement;
}

bool PreprocessLabel(const string &in, size_t instrNumber, string &error)
{
	std::stringstream errorBuilder;

	if (!isalpha(in[0]))
	{
		errorBuilder << "First character" << in[0] << "is not alphabetical";
		error = errorBuilder.str();
		return false;
	}

	std::stringstream tokenBuilder;
	char c;

	for (size_t i = 0, len = in.length(); i < len; i++)
	{
		c = in[i];

		if (c == ')') break;

		if (!(isalnum(c) || c == '_'))
		{
			errorBuilder << "Character" << in[0] << "is not alphanumeric or an underscore";
			error = errorBuilder.str();
			return false;
		}

		tokenBuilder << c;
	}

	if (c != ')')
	{
		error = "Unclosed label";
		return false;
	}

	string tok = tokenBuilder.str();

	if (symbolsTable.find(tok) != symbolsTable.end())
	{
		errorBuilder << "Label" << tok << "already exists";
		error = errorBuilder.str();
		return false;
	}

	symbolsTable.insert({tok, instrNumber});

	return true;
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

	std::ofstream output("out.hack"), pruned;

	// Shows preprocessor output
	if (argc > 2) pruned.open("pruned.asm");

	if (!output.is_open())
	{
		fprintf(stderr, "Could not create output file.\n");
		return 1;
	}

	// Sets up symbols table with default symbols
	symbolsTable.clear();
	symbolsTable.insert({
		{"R0", 0}, {"R1", 1}, {"R2", 2}, {"R3", 3}, {"R4", 4},
		{"R5", 5}, {"R6", 6}, {"R7", 7}, {"R8", 8}, {"R9", 9},
		{"R10", 10}, {"R11", 11}, {"R12", 12}, {"R13", 13},
		{"R14", 14}, {"R15", 15}, {"KBD", 32767}, {"SCREEN", 24576}
	});

	size_t lineNumber = 1, instrNumber = 0;
	std::stringstream source;

	// Preprocessing: Removes empty lines, comments and catalogues labels.
	while (!asmFile.eof())
	{
		string line, error;
		std::getline(asmFile, line);

		size_t start = 0, end = line.length();
		while (start < end)
		{
			bool a = isspace(line[start]), b = isspace(line[end - 1]);
			if (a) start++;
			if (b) end--;

			if (!(a || b)) break;
		}

		if (start == end) 
		{
			lineNumber++;
			continue;
		}

		char first = line[start];

		switch (first)
		{
			bool valid;

			case '(':
				valid = PreprocessLabel(line.substr(start + 1, end - start - 1), instrNumber, error);

				if (!valid)
				{
					fprintf(stderr, "Malformed label on line %d: %s\n", lineNumber, error.c_str());
					return 1;
				}

				lineNumber++;
				continue;
			break;

			case '/':
				valid = start + 1 != end && line[start + 1] == '/';

				if (!valid)
				{
					fprintf(stderr, "Unexpected symbol on line %d: /\n", lineNumber);
					return 1;
				}

				lineNumber++;
				continue;
			break;
		}

		size_t commStart;
		if ((commStart = line.find('/')) != string::npos)
		{
			bool valid = commStart + 1 != end && line[commStart + 1] == '/';

			if (!valid)
			{
				fprintf(stderr, "Unexpected symbol on line %d: /\n", lineNumber);
				return 1;
			}

			// Refit end to remove comment from the line
			for (end = commStart; end > start && isspace(line[end - 1]); end--) { };


		}

		string out = line.substr(start, end - start);

		if (argc > 2) pruned << out << "\n";
		source << out << "\n";
		lineNumberMapping.push_back(lineNumber++);
		instrNumber++;
	}

	lineNumber = instrNumber = 0;

	while (!source.eof())
	{
		string line, error;
		std::getline(source, line);
		bool valid;
		uint16_t statement = ParseStatement(line, lineNumberMapping[lineNumber], instrNumber, error, valid);

		if (valid)
		{
			instrNumber++;
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