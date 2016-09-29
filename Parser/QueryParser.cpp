/*=============================================================================
# Filename: QueryParser.cpp
# Author: Jiaqi, Chen
# Mail: chenjiaqi93@163.com
# Last Modified: 2016-07-14
# Description: implement functions in QueryParser.h
=============================================================================*/

#include "QueryParser.h"

using namespace std;

QueryParser::QueryParser()
{
	_prefix_map.clear();
}

void QueryParser::SPARQLParse(const string &query, QueryTree &querytree)
{
	//uncompress before use
	dfa34_Table_uncompress();

	pANTLR3_INPUT_STREAM input;
	pSparqlLexer lex;
	pANTLR3_COMMON_TOKEN_STREAM tokens;
	pSparqlParser parser;
	input = antlr3StringStreamNew((ANTLR3_UINT8 *)(query.c_str()), ANTLR3_ENC_UTF8, query.length(), (ANTLR3_UINT8 *)"QueryString");
	//input = antlr3FileStreamNew((pANTLR3_UINT8)filePath,ANTLR3_ENC_8BIT);
	lex = SparqlLexerNew(input);

	tokens = antlr3CommonTokenStreamSourceNew(ANTLR3_SIZE_HINT,TOKENSOURCE(lex));
	parser = SparqlParserNew(tokens);

	SparqlParser_workload_return r = parser->workload(parser);
	pANTLR3_BASE_TREE root = r.tree;
/*
	if (printNode(root) > 0)
		throw "Some errors are found in the SPARQL query request.";
*/
	parseWorkload(root, querytree);

	//querytree.print();

	parser->free(parser);
	tokens->free(tokens);
	lex->free(lex);
	input->close(input);
}

int QueryParser::printNode(pANTLR3_BASE_TREE node, int dep)
{
	const char* s = (const char*) node->getText(node)->chars;
	ANTLR3_UINT32 treeType = node->getType(node);

	int hasErrorNode = 0;
	if (treeType == 0)	hasErrorNode = 1;

	for (int i = 0; i < dep; i++)		printf("    ");
	printf("%d: %s\n",treeType,s);

	for (unsigned int i = 0; i < node->getChildCount(node); i++)
	{
		pANTLR3_BASE_TREE childNode = (pANTLR3_BASE_TREE) node->getChild(node, i);
		hasErrorNode += printNode(childNode, dep + 1);
	}
	return hasErrorNode;
}

void QueryParser::parseWorkload(pANTLR3_BASE_TREE node, QueryTree &querytree)
{
	//printf("parseWorkload\n");

	pANTLR3_BASE_TREE childNode = (pANTLR3_BASE_TREE) node->getChild(node, 0);

	//query 145
	if (childNode->getType(childNode) == 145)
	{
		parseQuery(childNode, querytree);
	}
	else
	//update 196
	if (childNode->getType(childNode) == 196)
	{
		parseUpdate(childNode, querytree);
	}
}

void QueryParser::parseQuery(pANTLR3_BASE_TREE node, QueryTree &querytree)
{
	//printf("parseQuery\n");

	for (unsigned int i = 0; i < node->getChildCount(node); i++)
	{
		pANTLR3_BASE_TREE childNode = (pANTLR3_BASE_TREE) node->getChild(node, i);

		//prologue	144
		if (childNode->getType(childNode) == 144)
		{
			parsePrologue(childNode);
		}
		else
		//select 155
		if (childNode->getType(childNode) == 155)
		{
			querytree.setQueryForm(QueryTree::Select_Query);
			parseQuery(childNode, querytree);
		}
		else
		//ask 13
		if (childNode->getType(childNode) == 13)
		{
			querytree.setQueryForm(QueryTree::Ask_Query);
			parseQuery(childNode, querytree);
		}
		else
		//select clause 156
		if (childNode->getType(childNode) == 156)
		{
			parseSelectClause(childNode, querytree);
		}
		else
		//group graph pattern 77
		if (childNode->getType(childNode) == 77)
		{
			parseGroupPattern(childNode, querytree.getGroupPattern());
		}
		else
		//order by 127
		if (childNode->getType(childNode) == 127)
		{
			parseOrderBy(childNode, querytree);
		}
		else
		//offset 120	limit 102
		if (childNode->getType(childNode) == 120 || childNode->getType(childNode) == 102)
		{
			pANTLR3_BASE_TREE gchildNode = (pANTLR3_BASE_TREE) childNode->getChild(childNode, 0);

			//integer 83
			if (gchildNode->getType(gchildNode) == 83)
			{
				string str;
				parseString(gchildNode, str, 0);

				stringstream str2int;
				int num;
				str2int << str;
				str2int >> num;

				if (childNode->getType(childNode) == 120 && num >= 0)
					querytree.setOffset(num);
				if (childNode->getType(childNode) == 102 && num >= 0)
					querytree.setLimit(num);
			}
		}
		else	parseQuery(childNode, querytree);
	}
}

void QueryParser::parsePrologue(pANTLR3_BASE_TREE node)
{
	//printf("parsePrologue\n");

	for (unsigned int i = 0; i < node->getChildCount(node); i++)
	{
		pANTLR3_BASE_TREE childNode = (pANTLR3_BASE_TREE) node->getChild(node, i);

		//prefix 143
		if (childNode->getType(childNode) == 143)
			parsePrefix(childNode);
	}
}

void QueryParser::parsePrefix(pANTLR3_BASE_TREE node)
{
	printf("parsePrefix\n");

	string key, value;

	for (unsigned int i = 0; i < node->getChildCount(node); i++)
	{
		pANTLR3_BASE_TREE childNode = (pANTLR3_BASE_TREE) node->getChild(node, i);
		//prefix namespace 136
		if (childNode->getType(childNode) == 136)
			parseString(childNode, key, 0);

		//prefix IRI 89
		if (childNode->getType(childNode) == 89)
			parseString(childNode, value, 0);
	}
	_prefix_map.insert(make_pair(key, value));
}

void QueryParser::replacePrefix(string &str)
{
	if (str[0] != '<' && str[0] != '\"' && str[0] != '?')
	{
		int sep = str.find(":");
		if (sep == -1)	return;
		string prefix = str.substr(0, sep + 1);

		//blank node
		if (prefix == "_:")	return;

		cout << "prefix = " << prefix << endl;
		if (_prefix_map.find(prefix) != _prefix_map.end())
		{
			str = _prefix_map[prefix].substr(0, _prefix_map[prefix].length() - 1) + str.substr(sep + 1 ,str.length() - sep - 1) + ">";
			cout << "str = " << str << endl;
		}
		else
		{
			cout << "prefix not found..." << endl;
			throw "Some errors are found in the SPARQL query request.";
		}
	}
}

void QueryParser::parseSelectClause(pANTLR3_BASE_TREE node, QueryTree &querytree)
{
	//printf("parseSelectClause\n");

	for (unsigned int i = 0; i < node->getChildCount(node); i++)
	{
		pANTLR3_BASE_TREE childNode = (pANTLR3_BASE_TREE) node->getChild(node, i);

		//distinct 52
		if (childNode->getType(childNode) == 52)
			querytree.setProjectionModifier(QueryTree::Modifier_Distinct);

		//var 199
		if (childNode->getType(childNode) == 199)
			parseSelectVar(childNode, querytree);

		//asterisk 14
		if (childNode->getType(childNode) == 14)
			querytree.setProjectionAsterisk();
	}
}

void QueryParser::parseSelectVar(pANTLR3_BASE_TREE node, QueryTree &querytree)
{
	//printf("parseSelectVar\n");

	string var = "";
	for (unsigned int i = 0; i < node->getChildCount(node); i++)
	{
		pANTLR3_BASE_TREE childNode = (pANTLR3_BASE_TREE) node->getChild(node, i);

		if (childNode->getType(childNode) == 200)
		{
			parseString(childNode, var, 0);
			querytree.addProjectionVar(var);
		}
	}
}

void QueryParser::parseGroupPattern(pANTLR3_BASE_TREE node, QueryTree::GroupPattern &grouppattern)
{
	//printf("parseGroupPattern\n");

	for (unsigned int i = 0; i < node->getChildCount(node); i++)
	{
		pANTLR3_BASE_TREE childNode = (pANTLR3_BASE_TREE) node->getChild(node, i);

		//triples same subject 185
		if (childNode->getType(childNode) == 185)
		{
			parsePattern(childNode, grouppattern);
		}

		//optional	124		minus	108
		if (childNode->getType(childNode) == 124 || childNode->getType(childNode) == 108)
		{
			parseOptionalOrMinus(childNode, grouppattern);
		}

		//union  195
		if (childNode->getType(childNode) == 195)
		{
			grouppattern.addOneGroupUnion();
			parseUnion(childNode, grouppattern);
		}

		//filter 67
		if (childNode->getType(childNode) == 67)
		{
			parseFilter(childNode, grouppattern);
		}

		//group graph pattern 77
		//redundant {}
		if (childNode->getType(childNode) == 77)
		{
			parseGroupPattern(childNode, grouppattern);
		}
	}
}

void QueryParser::parsePattern(pANTLR3_BASE_TREE node, QueryTree::GroupPattern &grouppattern)
{
	//printf("parsePattern\n");

	string subject = "";
	string predicate = "";
	string object = "";
	for (unsigned int i = 0; i < node->getChildCount(node); i++)
	{
		pANTLR3_BASE_TREE childNode = (pANTLR3_BASE_TREE) node->getChild(node, i);

		//subject 177
		if (childNode->getType(childNode) == 177)
		{
			parseString(childNode, subject, 1);
			replacePrefix(subject);
		}

		//predicate 142
		if (childNode->getType(childNode) == 142)
		{
			pANTLR3_BASE_TREE gchildNode = (pANTLR3_BASE_TREE) childNode->getChild(childNode, 0);
			//var 200
			if (gchildNode->getType(gchildNode) == 200)
			{
				parseString(childNode, predicate, 1);
			}
			else
			{
				parseString(childNode, predicate, 4);
			}
			replacePrefix(predicate);
		}

		//object 119
		if (childNode->getType(childNode) == 119)
		{
			parseString(childNode, object, 1);
			replacePrefix(object);
		}

		if (i != 0 && i % 2 == 0)		//triples same subject
		{
			grouppattern.addOnePattern(QueryTree::GroupPattern::Pattern(	QueryTree::GroupPattern::Pattern::Element(subject),
																			QueryTree::GroupPattern::Pattern::Element(predicate),
																			QueryTree::GroupPattern::Pattern::Element(object)));
		}
	}
}

void QueryParser::parseOptionalOrMinus(pANTLR3_BASE_TREE node, QueryTree::GroupPattern &grouppattern)
{
	//optional	124		minus	108
	if (node->getType(node) == 124)
		printf("parseOptional\n");
	else if (node->getType(node) == 108)
		printf("parseMinus\n");

	for (unsigned int i = 0; i < node->getChildCount(node); i++)
	{
		pANTLR3_BASE_TREE childNode = (pANTLR3_BASE_TREE) node->getChild(node, i);

		//group graph pattern 77
		if (childNode->getType(childNode) == 77)
		{
			if (node->getType(node) == 124)
				grouppattern.addOneOptionalOrMinus('o');
			else if (node->getType(node) == 108)
				grouppattern.addOneOptionalOrMinus('m');

			parseGroupPattern(childNode, grouppattern.getLastOptionalOrMinus());
		}
	}
}

void QueryParser::parseUnion(pANTLR3_BASE_TREE node, QueryTree::GroupPattern &grouppattern)
{
	printf("parseUnion\n");

	for (unsigned int i = 0; i < node->getChildCount(node); i++)
	{
		pANTLR3_BASE_TREE childNode = (pANTLR3_BASE_TREE) node->getChild(node, i);

		//group graph pattern 77
		if (childNode->getType(childNode) == 77)
		{
			grouppattern.addOneUnion();
			parseGroupPattern(childNode, grouppattern.getLastUnion());
		}

		//union  195
		if (childNode->getType(childNode) == 195)
		{
			parseUnion(childNode, grouppattern);
		}
	}
}

void QueryParser::parseFilter(pANTLR3_BASE_TREE node, QueryTree::GroupPattern &grouppattern)
{
	printf("parseFilter\n");

	for (unsigned int i = 0; i < node->getChildCount(node); i++)
	{
		pANTLR3_BASE_TREE childNode = (pANTLR3_BASE_TREE) node->getChild(node, i);

		//unary 190
		if (childNode->getType(childNode) == 190)
			childNode = (pANTLR3_BASE_TREE) childNode->getChild(childNode, 0);

		grouppattern.addOneFilterTree();
		parseFilterTree(childNode, grouppattern, grouppattern.getLastFilterTree());
	}
}

void QueryParser::parseFilterTree(pANTLR3_BASE_TREE node, QueryTree::GroupPattern &grouppattern, QueryTree::GroupPattern::FilterTreeNode &filter)
{
	printf("parseFilterTree\n");

	switch (node->getType(node))
	{
		//! 192
		case 192:	filter.oper_type = QueryTree::GroupPattern::FilterTreeNode::Not_type;		break;
		//not 115
		case 115:	filter.oper_type = QueryTree::GroupPattern::FilterTreeNode::Not_type;		break;
		//or 125
		case 125:	filter.oper_type = QueryTree::GroupPattern::FilterTreeNode::Or_type;		break;
		//and 8
		case 8:		filter.oper_type = QueryTree::GroupPattern::FilterTreeNode::And_type;		break;
		//equal 62
		case 62:		filter.oper_type = QueryTree::GroupPattern::FilterTreeNode::Equal_type;		break;
		//not equal 116
		case 116:	filter.oper_type = QueryTree::GroupPattern::FilterTreeNode::NotEqual_type;		break;
		//less 100
		case 100:	filter.oper_type = QueryTree::GroupPattern::FilterTreeNode::Less_type;		break;
		//less equal 101
		case 101:	filter.oper_type = QueryTree::GroupPattern::FilterTreeNode::LessOrEqual_type;		break;
		//greater 72
		case 72:		filter.oper_type = QueryTree::GroupPattern::FilterTreeNode::Greater_type;		break;
		//greater equal 73
		case 73:		filter.oper_type = QueryTree::GroupPattern::FilterTreeNode::GreaterOrEqual_type;		break;

		//regex 150
		case 150:	filter.oper_type = QueryTree::GroupPattern::FilterTreeNode::Builtin_regex_type;		break;
		//lang 96
		case 96:		filter.oper_type = QueryTree::GroupPattern::FilterTreeNode::Builtin_lang_type;		break;
		//langmatches 97
		case 97:		filter.oper_type = QueryTree::GroupPattern::FilterTreeNode::Builtin_langmatches_type;		break;
		//bound 23
		case 23:		filter.oper_type = QueryTree::GroupPattern::FilterTreeNode::Builtin_bound_type;		break;
		//in 81
		case 81:		filter.oper_type = QueryTree::GroupPattern::FilterTreeNode::Builtin_in_type;		break;
		//exists 63
		case 63:		filter.oper_type = QueryTree::GroupPattern::FilterTreeNode::Builtin_exists_type;		break;
		//not exists 117
		case 117:	filter.oper_type = QueryTree::GroupPattern::FilterTreeNode::Not_type;		break;

		default:
			return;
	}

	//in the "NOT IN" case,  in, var and expression list is on the same layer.
	//not 115
	if (node->getType(node) == 115)
	{
		pANTLR3_BASE_TREE childNode = (pANTLR3_BASE_TREE) node->getChild(node, 0);

		//in 81
		if (childNode->getType(childNode) == 81)
		{
			filter.child.push_back(QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild());
			filter.child[0].node_type = QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type;
			filter.child[0].node.oper_type = QueryTree::GroupPattern::FilterTreeNode::Builtin_in_type;
			parseVarInExpressionList(node, filter.child[0].node, 1);

			return;
		}
	}

	//in 81
	if (node->getType(node) == 81)
	{
		parseVarInExpressionList(node, filter, 0);

		return;
	}

	//not exists 117
	if (node->getType(node) == 117)
	{
		filter.child.push_back(QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild());
		filter.child[0].node_type = QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type;
		filter.child[0].node.oper_type = QueryTree::GroupPattern::FilterTreeNode::Builtin_exists_type;

		parseExistsGroupPattern(node, grouppattern, filter.child[0].node);

		return;
	}

	//exists 63
	if (node->getType(node) == 63)
	{
		parseExistsGroupPattern(node, grouppattern, filter);

		return;
	}

	for (unsigned int i = 0; i < node->getChildCount(node); i++)
	{
		pANTLR3_BASE_TREE childNode = (pANTLR3_BASE_TREE) node->getChild(node, i);

		//unary 190
		if (childNode->getType(childNode) == 190)
		{
			pANTLR3_BASE_TREE gchildNode = (pANTLR3_BASE_TREE) childNode->getChild(childNode, 0);
			//unsigned int type = gchildNode->getType(gchildNode);
			//regex 150	lang 96	langmatches 97	bound 23	exists 63
			//if (type == 150 || type == 96 || type == 97 || type == 23 || type == 63)
			if (gchildNode->getChildCount(gchildNode) != 0)
				childNode = gchildNode;
		}

		filter.child.push_back(QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild());

		//unary 190
		if (childNode->getType(childNode) == 190)
		{
			filter.child[i].node_type = QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type;
			parseString(childNode, filter.child[i].arg, 1);
			replacePrefix(filter.child[i].arg);
		}
		else if (childNode->getChildCount(childNode) == 0)
		{
			filter.child[i].node_type = QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type;
			parseString(childNode, filter.child[i].arg, 0);
			replacePrefix(filter.child[i].arg);
		}
		else
		{
			filter.child[i].node_type = QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type;
			parseFilterTree(childNode, grouppattern, filter.child[i].node);
		}
	}
}

void QueryParser::parseVarInExpressionList(pANTLR3_BASE_TREE node, QueryTree::GroupPattern::FilterTreeNode &filter, unsigned int begin)
{
	printf("parseVarInExpressionList\n");

	for (unsigned int i = begin; i < node->getChildCount(node); i++)
	{
		pANTLR3_BASE_TREE childNode = (pANTLR3_BASE_TREE) node->getChild(node, i);

		//unary 190
		if (childNode->getType(childNode) == 190)
		{
			filter.child.push_back(QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild());

			filter.child[i - begin].node_type = QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type;
			parseString(childNode, filter.child[i - begin].arg, 1);
			replacePrefix(filter.child[i - begin].arg);
		}

		//expression list 65
		if (childNode->getType(childNode) == 65)
		{
			for (unsigned int j = 0; j < childNode->getChildCount(childNode); j++)
			{
				pANTLR3_BASE_TREE gchildNode = (pANTLR3_BASE_TREE) childNode->getChild(childNode, j);

				filter.child.push_back(QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild());

				filter.child[i + j - begin].node_type = QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type;
				parseString(gchildNode, filter.child[i + j - begin].arg, 1);
				replacePrefix(filter.child[i + j - begin].arg);
			}
		}
	}
}

void QueryParser::parseExistsGroupPattern(pANTLR3_BASE_TREE node, QueryTree::GroupPattern &grouppattern, QueryTree::GroupPattern::FilterTreeNode &filter)
{
	printf("parseExistsGroupPattern\n");

	pANTLR3_BASE_TREE childNode = (pANTLR3_BASE_TREE) node->getChild(node, 0);

	//group graph pattern 77
	if (childNode->getType(childNode) == 77)
	{
		grouppattern.addOneExistsGroupPattern();
		filter.exists_grouppattern_id = (int)grouppattern.filter_exists_grouppatterns[(int)grouppattern.filter_exists_grouppatterns.size() - 1].size() - 1;
		parseGroupPattern(childNode, grouppattern.getLastExistsGroupPattern());

		return;
	}
}

void QueryParser::parseOrderBy(pANTLR3_BASE_TREE node, QueryTree &querytree)
{
	printf("parseOrderBy\n");

	for (unsigned int i = 0; i < node->getChildCount(node); i++)
	{
		pANTLR3_BASE_TREE childNode = (pANTLR3_BASE_TREE) node->getChild(node, i);

		//order by condition
		if (childNode->getType(childNode) == 128)
		{
			string var;
			bool desending = false;
			for (unsigned int k = 0; k < childNode->getChildCount(childNode); k++)
			{
				pANTLR3_BASE_TREE gchildNode = (pANTLR3_BASE_TREE) childNode->getChild(childNode, k);

				//var 200
				if (gchildNode->getType(gchildNode) == 200)
					parseString(gchildNode, var, 0);

				//unary 190
				if (gchildNode->getType(gchildNode) == 190)
					parseString(gchildNode, var, 1);

				//asend 12
				if (gchildNode->getType(gchildNode) == 12)
					desending = false;

				//desend 49
				if (gchildNode->getType(gchildNode) == 49)
					desending = true;
			}

			querytree.addOrder(var, desending);
		}
	}
}

void QueryParser::parseString(pANTLR3_BASE_TREE node, string &str, int dep)
{
	if (dep == 0)
	{
		str = (const char*) node->getText(node)->chars;
		return;
	}

	while (dep > 1 && node != NULL)
	{
		node = (pANTLR3_BASE_TREE) node->getChild(node, 0);
		dep--;
	}

	if (node == NULL || node->getChildCount(node) == 0)
		throw "Some errors are found in the SPARQL query request.";
	else
	{
		str = "";
		for (unsigned int i = 0; i < node->getChildCount(node); i++)
		{
			pANTLR3_BASE_TREE childNode = (pANTLR3_BASE_TREE) node->getChild(node, i);

			//var 200
			//string literal 170(single quotation marks)	171(double quotation marks)
			//IRI 89
			//PNAME_LN 135
			//custom language 98

			string substr = (const char*) childNode->getText(childNode)->chars;
			if (childNode->getType(childNode) == 170)
				substr = "\"" + substr.substr(1, substr.length() - 2) + "\"";

			if (i > 0)
			{
				pANTLR3_BASE_TREE preChildNode = (pANTLR3_BASE_TREE) node->getChild(node, i - 1);

				//xsd:*
				//reference	149
				if (preChildNode->getType(preChildNode) == 149)
					replacePrefix(substr);
			}

			str += substr;
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void QueryParser::parseUpdate(pANTLR3_BASE_TREE node, QueryTree &querytree)
{
	printf("parseUpdate\n");

	for (unsigned int i = 0; i < node->getChildCount(node); i++)
	{
		pANTLR3_BASE_TREE childNode = (pANTLR3_BASE_TREE) node->getChild(node, i);

		//prologue	144
		if (childNode->getType(childNode) == 144)
		{
			parsePrologue(childNode);
		}
		else
		//insert 82
		if (childNode->getType(childNode) == 82)
		{
			//INSERT
				//DATA
				//TRIPLES_TEMPLATE

			querytree.setUpdateType(QueryTree::Insert_Data);

			pANTLR3_BASE_TREE gchildNode = (pANTLR3_BASE_TREE) childNode->getChild(childNode, 1);
			//triples template 186
			if (gchildNode->getType(gchildNode) == 186)
				parseTripleTemplate(gchildNode, querytree.getInsertPatterns());
		}
		else
		//delete 48
		if (childNode->getType(childNode) == 48 && childNode->getChildCount(childNode) > 0)
		{
			//DELETE
			//DELETE
				//DATA or WHERE
				//TRIPLES_TEMPLATE

			pANTLR3_BASE_TREE gchildNode = (pANTLR3_BASE_TREE) childNode->getChild(childNode, 0);
			//data 41
			if (gchildNode->getType(gchildNode) == 41)
			{
				querytree.setUpdateType(QueryTree::Delete_Data);

				gchildNode = (pANTLR3_BASE_TREE) childNode->getChild(childNode, 1);
				//triples template 186
				if (gchildNode->getType(gchildNode) == 186)
					parseTripleTemplate(gchildNode, querytree.getDeletePatterns());
			}
			else
			//where 203
			if (gchildNode->getType(gchildNode) == 203)
			{
				querytree.setUpdateType(QueryTree::Delete_Where);

				gchildNode = (pANTLR3_BASE_TREE) childNode->getChild(childNode, 1);
				//triples template 186
				if (gchildNode->getType(gchildNode) == 186)
				{
					parseTripleTemplate(gchildNode, querytree.getDeletePatterns());
					querytree.getGroupPattern() = querytree.getDeletePatterns();
				}
			}
		}
		else
		//modify 110
		if (childNode->getType(childNode) == 110)
		{
			parseModify(childNode, querytree);
		}
	}
}

void QueryParser::parseTripleTemplate(pANTLR3_BASE_TREE node, QueryTree::GroupPattern &grouppattern)
{
	printf("parseTripleTemplate\n");

	//TRIPLES_TEMPLATE
		//TRIPLES_SAME_SUBJECT
			//SUBJECT
				//...
			//PREDICATE
				//...
			//OBJECT
				//...

	for (unsigned int i = 0; i < node->getChildCount(node); i++)
	{
		pANTLR3_BASE_TREE childNode = (pANTLR3_BASE_TREE) node->getChild(node, i);

		//triples same subject 185
		if (childNode->getType(childNode) == 185)
		{
			string subject = "";
			string predicate = "";
			string object = "";

			for (unsigned int j = 0; j < childNode->getChildCount(childNode); j++)
			{
				pANTLR3_BASE_TREE gchildNode = (pANTLR3_BASE_TREE) childNode->getChild(childNode, j);

				//subject 177
				if (gchildNode->getType(gchildNode) == 177)
				{
					parseString(gchildNode, subject, 1);
					replacePrefix(subject);
				}

				//predicate 142
				if (gchildNode->getType(gchildNode) == 142)
				{
					parseString(gchildNode, predicate, 1);
					replacePrefix(predicate);
				}

				//object 119
				if (gchildNode->getType(gchildNode) == 119)
				{
					parseString(gchildNode, object, 1);
					replacePrefix(object);
				}

				if (j != 0 && j % 2 == 0)		//triples same subject
				{
					grouppattern.addOnePattern(QueryTree::GroupPattern::Pattern(	QueryTree::GroupPattern::Pattern::Element(subject),
																																		QueryTree::GroupPattern::Pattern::Element(predicate),
																																		QueryTree::GroupPattern::Pattern::Element(object)));
				}
			}
		}
	}
}

void QueryParser::parseModify(pANTLR3_BASE_TREE node, QueryTree &querytree)
{
	printf("parseModify\n");

	//DELETE
	//TRIPLES_TEMPLATE
		//TRIPLES_SAME_SUBJECT
	//INSERT
	//TRIPLES_TEMPLATE
		//TRIPLES_SAME_SUBJECT
	//WHERE
		//GROUP_GRAPH_PATTERN

	for (unsigned int i = 0; i < node->getChildCount(node); i++)
	{
		pANTLR3_BASE_TREE childNode = (pANTLR3_BASE_TREE) node->getChild(node, i);

		//delete 48
		if (childNode->getType(childNode) == 48)
		{
			querytree.setUpdateType(QueryTree::Delete_Clause);
		}
		else
		//insert 82
		if (childNode->getType(childNode) == 82)
		{
			if (querytree.getUpdateType() == QueryTree::Not_Update)
				querytree.setUpdateType(QueryTree::Insert_Clause);
			else if (querytree.getUpdateType() == QueryTree::Delete_Clause)
				querytree.setUpdateType(QueryTree::Modify_Clause);
		}
		else
		//triples template 186
		if (childNode->getType(childNode) == 186)
		{
			if (querytree.getUpdateType() == QueryTree::Delete_Clause)
				parseTripleTemplate(childNode, querytree.getDeletePatterns());
			else if (querytree.getUpdateType() == QueryTree::Insert_Clause || querytree.getUpdateType() == QueryTree::Modify_Clause)
				parseTripleTemplate(childNode, querytree.getInsertPatterns());
		}
		else
		//where 203
		if (childNode->getType(childNode) == 203)
		{
			//WHERE
				//GROUP_GRAPH_PATTERN

			pANTLR3_BASE_TREE gchildNode = (pANTLR3_BASE_TREE) childNode->getChild(childNode, 0);
			//group graph pattern 77
			if (gchildNode->getType(gchildNode) == 77)
				parseGroupPattern(gchildNode, querytree.getGroupPattern());
		}
	}
}
