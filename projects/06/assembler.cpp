#include <stdio.h>
#include <fstream>
#include <sstream>
#include <string>
#include <bitset>
#include <map>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>

#define MAX_ADDR 32767

using string = std::string;

enum InstructionType
{
	INSTR_ADDR, INSTR_COMP
};

const std::array<char, 10> computationCharset = { 'A', 'M', 'D', '-', '+', '&', '|', '!', '1', '0' };
const std::array<string, 7> jumpTypes = {"JGT", "JEQ", "JGE", "JLT", "JNE", "JLE", "JMP"};
const std::array<char, 3> labelCharacters = { '_', '.', '$' };

/// Holds mappings from labels and variables to their addresses in memory
std::map<string, uint32_t> symbolsTable = {
		{"R0", 0}, {"R1", 1}, {"R2", 2}, {"R3", 3}, {"R4", 4},
		{"R5", 5}, {"R6", 6}, {"R7", 7}, {"R8", 8}, {"R9", 9},
		{"R10", 10}, {"R11", 11}, {"R12", 12}, {"R13", 13},
		{"R14", 14}, {"R15", 15}, {"KBD", 32767}, {"SCREEN", 16384}
	};


/// Last used memory address for variables, starts at 15 due to the R0..R15 registers
uint16_t lastUsedAddress = 15;

/// Maps post-preprocessing lines to the original lines in the source code
std::vector<size_t> lineNumberMapping;

/// Parses a given A-type instruction into its equivalent hack machine code
/// An A-type instruction is of the form @[15-bit immediate | label]
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
		TOK_LBL, TOK_NUM
	} type = isdigit(tok[0]) ? TOK_NUM : TOK_LBL;

	for (auto c : tok)
	{
		if (!valid) break;
		
		if (type == TOK_NUM && !isdigit(c))
		{
			valid = false;
			
			std::stringstream errorText;
			errorText << "Character " << c << " is not a digit, but token starts with a digit";
			error = errorText.str();
		}
		else if (type == TOK_LBL && !(isalnum(c) || std::find(labelCharacters.begin(), labelCharacters.end(), c) != labelCharacters.end()))
		{
			valid = false;

			std::stringstream errorText;
			errorText << "Character " << c << " is not a valid label symbol";
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

	if (type == TOK_NUM)
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

	if (parser.eof())
	{
		parser.clear();
		parser.seekg(0);
	}

	std::getline(parser, tok, ';');

	// This flag encodes the appearance of the M register in the computation
	bool useMemory = false;
	std::vector<char> cTokens;

	auto charsetBegin = computationCharset.begin(), charsetEnd = computationCharset.end();
	if (valid) for (auto c : tok)
	{
		if (isspace(c)) continue;

		if (std::find(charsetBegin, charsetEnd, c) == charsetEnd)
		{
			errorText << "Unexpected symbol " << c << " in computation";

			error = errorText.str();
			valid = false;
			break;
		}

		if (c == 'M') useMemory = true;
		if (useMemory && c == 'A')
		{
			error = "Computation cannot use both the A and M registers";
			valid = false;
			break;
		}
		cTokens.push_back(c);
	}

	size_t cTokenCount = cTokens.size();

	if (cTokenCount < 1)
	{
		error = "Empty computation";
		valid = false;
	}

	if (valid && !parser.eof())
	{
		parser >> tok;

		auto jBegin = jumpTypes.begin(), jEnd = jumpTypes.end();
		bool eof = parser.eof();
		std::array<string, 7>::const_iterator position;

		if (!eof || (position = std::find(jBegin, jEnd, tok)) == jEnd)
		{
			if (eof) parser >> tok;

			errorText << "Unexpected symbol " << tok << " after jump type";
			error = errorText.str();
			valid = false;
		}
		else
		{
			size_t index = position - jBegin;
			std::bitset<3> jumpBits(index + 1);

			output[0] = jumpBits[0];
			output[1] = jumpBits[1];
			output[2] = jumpBits[2];
		}
	}

	enum OPERATION {
		NOOP, NEG, NOT, ADD, SUB, AND, OR
	} opType;

	std::bitset<6> opcode(0);

	output[12] = useMemory;

	if (cTokenCount == 1) opType = NOOP;
	else if (valid)
	{
		if (cTokens[0] == '-') opType = NEG;
		else if (cTokens[0] == '!') opType = NOT;
		else if (cTokens[1] == '+') opType = ADD;
		else if (cTokens[1] == '-') opType = SUB;
		else if (cTokens[1] == '&') opType = AND;
		else if (cTokens[1] == '|') opType = OR;
		else 
		{
			error = "Could not evaluate computation";
			valid = false;
		}
	}

	if (valid && (opType == NEG || opType == NOT) && cTokenCount != 2)
	{
		error = "Unary operator cannot have more than 2 operands";
		valid = false;
	}
	else if (valid && (opType == ADD || opType == SUB || opType == AND || opType == OR) && cTokenCount != 3)
	{
		error = "Binary operator must have exactly 2 operands";
		valid = false;
	}

	if (valid) switch (opType)
	{
		char a, b;

		case NEG:
			a = cTokens[1];

			switch (a)
			{
				case 'A':
				case 'M':
					opcode = 0b110011;
				break;

				case 'D':
					opcode = 0b001111;
				break;

				case '1':
					opcode = 0b111010;
				break;

				default:
					errorText << "Invalid token " << a << " in negation operation";
					error = errorText.str();
					valid = false;
				break;
			}
		break;

		case NOT:
			a = cTokens[1];

			switch (a)
			{
				case 'A':
				case 'M':
					opcode = 0b110001;
				break;

				case 'D':
					opcode = 0b001101;
				break;

				default:
					errorText << "Invalid token " << a << " in not operation";
					error = errorText.str();
					valid = false;
				break;
			}
		break;

		case ADD:
			a = cTokens[0];
			b = cTokens[2];

			switch (a)
			{
				case 'D':
					if (b == '1') opcode = 0b011111;
					else if (b == 'A' || b == 'M') opcode = 0b000010;
					else 
					{
						errorText << a << "is not a valid second operand for addition";
						error = errorText.str();
						valid = false;
					}
				break;

				case 'A':
				case 'M':
					if (b == '1') opcode = 0b110111;
					else 
					{
						errorText << a << "is not a valid second operand for addition";
						error = errorText.str();
						valid = false;
					}
				break;

				default:
					errorText << b << " is not a valid first operand for addition";
					error = errorText.str();
					valid = false;
				break;
			}
		break;

		case SUB:
			a = cTokens[0];
			b = cTokens[2];

			switch (a)
			{
				case 'D':
					if (b == '1') opcode = 0b001110;
					else if (b == 'A' || b == 'M') opcode = 0b010011;
					else 
					{
						errorText << a << "is not a valid second operand for subtraction";
						error = errorText.str();
						valid = false;
					}
				break;

				case 'A':
				case 'M':
					if (b == '1') opcode = 0b110010;
					else if (b == 'D') opcode = 0b000111;
					else 
					{
						errorText << a << "is not a valid second operand for subtraction";
						error = errorText.str();
						valid = false;
					}
				break;

				default:
					errorText << b << " is not a valid first operand for subtraction";
					error = errorText.str();
					valid = false;
				break;
			}
		break;

		case AND:
			a = cTokens[0];
			b = cTokens[2];

			if (a != 'D' && !(b == 'M' || b == 'A'))
			{
				errorText << b << " is not a valid second operand for binary and";
				error = errorText.str();
				valid = false;
			}

			opcode = 0b000000;
		break;

		case OR:
			a = cTokens[0];
			b = cTokens[2];

			if (a != 'D' && !(b == 'M' || b == 'A'))
			{
				errorText << b << " is not a valid second operand for binary and";
				error = errorText.str();
				valid = false;
			}

			opcode = 0b010101;
		break;

		default:
			a = cTokens[0];
			if (!(a == '0' || a == '1' || a == 'D' || a == 'M' || a == 'A'))
			{
				errorText << a << " is not a valid constant symbol or register";
				error = errorText.str();
				valid = false;
			}

			if (a == '0') opcode = 0b101010;
			else if (a == '1') opcode = 0b111111;
			else if (a == 'D') opcode = 0b001100;
			else if (a == 'A' || a == 'M') opcode = 0b110000;
		break;
	}
	
	if (!valid)
	{
		errorText.str("");
		errorText << "Malformed computation instruction on line " << lineNumber << ": " << (error.empty() ? "Syntax error" : error);
		
		error = errorText.str();
		return 0;
	}

 	for (int i = 0; i < 6; i++)
	{
		bool b = opcode[i];
		output[i + 6] = b;
	}

	return output.to_ulong();
}

/// Lookup table for parsing function based on the type of instruction (enum value => index), sorry for the horrific syntax
uint16_t (*parseLUT[]) (string &in, size_t lineNumber, size_t instrNumber, string &error, bool &valid) = { ParseAddress, ParseComputation };

/// Identifies the type of a statement and parses it accordingly
uint16_t ParseStatement(string &in, size_t lineNumber, size_t instrNumber, string &error, bool &valid)
{
	InstructionType type = INSTR_COMP;
	uint16_t statement;
	valid = true;
	error = "";

	if (in[0] == '@') type = INSTR_ADDR;

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

		if (!(isalnum(c) || std::find(labelCharacters.begin(), labelCharacters.end(), c) != labelCharacters.end()))
		{
			errorBuilder << "Character " << c << " is not a valid label symbol";
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
		if (line.length() == 0) break;
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