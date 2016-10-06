/*=============================================================================
# Filename: GeneralEvaluation.cpp
# Author: Jiaqi, Chen
# Mail: chenjiaqi93@163.com
# Last Modified: 2016-09-12
# Description: implement functions in GeneralEvaluation.h
=============================================================================*/

#include "GeneralEvaluation.h"

using namespace std;

vector<vector<string> > GeneralEvaluation::getSPARQLQueryVarset()
{
	vector<vector<string> > res;
	for (int i = 0; i < (int)this->sparql_query_varset.size(); i++)
		res.push_back(this->sparql_query_varset[i].varset);
	return res;
}

bool GeneralEvaluation::onlyParseQuery(const string &_query, int& var_num, QueryTree::QueryForm& query_form)
{
    try
    {
        this->query_parser.SPARQLParse(_query, this->query_tree);
    }
    catch(const char* e)
    {
        cerr << e << endl;
        return false;
    }
	
	this->query_tree.getGroupPattern().getVarset();
	var_num = this->query_tree.getGroupPattern().grouppattern_resultset_maximal_varset.varset.size();
	if(this->query_tree.getQueryForm() == QueryTree::Ask_Query){
		query_form = QueryTree::Ask_Query;
	}else{
		query_form = QueryTree::Select_Query;
	}
	
    return true;
}

bool GeneralEvaluation::parseQuery(const string &_query)
{
	try
	{
		this->query_parser.SPARQLParse(_query, this->query_tree);
	}
	catch(const char *e)
	{
		cerr << e << endl;
		return false;
	}
	return true;
}

QueryTree& GeneralEvaluation::getQueryTree()
{
	return this->query_tree;
}

void GeneralEvaluation::getLocalPartialResult(KVstore *_kvstore, string& internal_tag_str, string &lpm_str)
{
	if (this->semantic_evaluation_result_stack.empty())		return;

	TempResultSet *results_id = this->semantic_evaluation_result_stack.top();
	this->semantic_evaluation_result_stack.pop();

	Varset &proj = this->query_tree.getProjection();
	
	stringstream lpm_ss;

	if (this->query_tree.getQueryForm() == QueryTree::Select_Query)
	{
		if (this->query_tree.checkProjectionAsterisk())
		{
			for (int i = 0 ; i < (int)results_id->results.size(); i++)
				proj = proj + results_id->results[i].var;
		}

		if (this->query_tree.getProjectionModifier() == QueryTree::Modifier_Distinct)
		{
			TempResultSet *results_id_distinct = new TempResultSet();

			results_id->doDistinct(proj, *results_id_distinct);

			results_id->release();
			delete results_id;

			results_id = results_id_distinct;
		}

		//int var_num = proj.varset.size();
		//int select_var_num = var_num;
		int ansNum = 0;
		//result_str.setVar(proj.varset);

		for (int i = 0; i < (int)results_id->results.size(); i++){
			ansNum += (int)results_id->results[i].res.size();
		}
		
		BasicQuery &_basicquery = this->expansion_evaluation_stack[0].sparql_query.getBasicQuery(0);
		
		int current_result = 0;
		for (int i = 0; i < (int)results_id->results.size(); i++)
		{
			vector<int> result_str2id = proj.mapTo(results_id->results[i].var);
			int size = results_id->results[i].res.size();
			for (int j = 0; j < size; ++j)
			{
				vector<string> tmp_vec(result_str2id.size(), "");
				//answer.push_back(tmp_vec);
				
				for (int v = 0; v < result_str2id.size(); ++v)
				{
					int ans_id = -1;
					//printf("result_str2id[v] = %d and results_id->results[i].res[j].size() = %d\n", result_str2id[v], results_id->results[i].res[j]);
					if (result_str2id[v] != -1)
						ans_id =  results_id->results[i].res[j][result_str2id[v]];
						
					//answer[current_result][v] = "";
					//printf("ans_id = %d \n", ans_id);
					if (ans_id != -1)
					{	
						if(ans_id >= Util::LITERAL_FIRST_ID){
							lpm_ss << "1" << _kvstore->getLiteralByID(ans_id) << "\t";
						}else if(internal_tag_str.at(ans_id) == 1){
							lpm_ss << "1" << _kvstore->getEntityByID(ans_id) << "\t";
						}else{
							if(_basicquery.getVarDegree(v) != 1){
								lpm_ss << internal_tag_str.at(ans_id) << _kvstore->getEntityByID(ans_id) << "\t";
							}else{
								lpm_ss << "1" << _kvstore->getEntityByID(ans_id) << "\t";
							}
						}
					}else{
						lpm_ss << "-1\t";
					}
					
				}
				lpm_ss << endl;
				current_result++;
			}
		}
	}
	else if (this->query_tree.getQueryForm() == QueryTree::Ask_Query)
	{
		vector<BasicQuery*>& queryList = this->sparql_query.getBasicQueryVec();
		vector<BasicQuery*>::iterator iter = queryList.begin();
		for(; iter != queryList.end(); iter++)
		{
			vector<int*>& all_result_list = (*iter)->getResultList();
			int var_num = (*iter)->getVarNum();
			
			char* dealed_internal_id_sign = new char[var_num + 1];
			set<string> LECF_set;
			
			for(vector<int*>::iterator it = all_result_list.begin(); it != all_result_list.end(); it++){
				memset(dealed_internal_id_sign, '0', sizeof(bool) * var_num);
				dealed_internal_id_sign[var_num] = 0;
				int* result_var = *it;		
				stringstream res_ss;
				
				for(int j = 0; j < var_num; j++){
					int var_degree = (*iter)->getVarDegree(j);
					
					if(result_var[j] == -1)
						continue;
					
					for (int i = 0; i < var_degree; i++)
					{
						if(result_var[j] >= Util::LITERAL_FIRST_ID || internal_tag_str.at(result_var[j]) == '1'){
							dealed_internal_id_sign[j] = '1';
						}else{
							continue;
						}
						// each triple/edge need to be processed only once.
						int edge_id = (*iter)->getEdgeID(j, i);				
						int var_id2 = (*iter)->getEdgeNeighborID(j, i);
						if (var_id2 == -1)
						{
							continue;
						}
						
						if(result_var[var_id2] != -1){
							if(result_var[var_id2] < Util::LITERAL_FIRST_ID && internal_tag_str.at(result_var[var_id2]) == '0' && (*iter)->getVarDegree(var_id2) != 1){
								string _tmp_1, _tmp_2;
								if(result_var[j] < Util::LITERAL_FIRST_ID){
									_tmp_1 = (this->kvstore)->getEntityByID(result_var[j]);
								}else{
									_tmp_1 = (this->kvstore)->getLiteralByID(result_var[j]);
								}
								
								if(result_var[var_id2] < Util::LITERAL_FIRST_ID){
									_tmp_2 = (this->kvstore)->getEntityByID(result_var[var_id2]);
								}else{
									_tmp_2 = (this->kvstore)->getLiteralByID(result_var[var_id2]);
								}
								if(j < var_id2){
									res_ss << j << "\t" << var_id2 << "\t" << _tmp_1 << "\t" << _tmp_2 << "\t";
								}else{
									res_ss << var_id2 << "\t" << j << "\t" << _tmp_2 << "\t" << _tmp_1 << "\t";
								}
							}
						}
					}
				}
				res_ss << dealed_internal_id_sign << endl;
				//log_output << res_ss.str() << endl;
				LECF_set.insert(res_ss.str());
			}
			delete[] dealed_internal_id_sign;
			
			for(set<string>::iterator it1 = LECF_set.begin(); it1 != LECF_set.end(); it1++){
				lpm_ss << *it1 << endl;
			}
		}
	}
	
	lpm_str = lpm_ss.str();
}

void GeneralEvaluation::doQuery(string& internal_tag_str)
{
	this->query_tree.getGroupPattern().getVarset();
	if (this->query_tree.getGroupPattern().grouppattern_subject_object_maximal_varset.hasCommonVar(this->query_tree.getGroupPattern().grouppattern_predicate_maximal_varset))
	{
		cerr << "There are some vars occur both in subject/object and predicate." << endl;
		return;
	}

	this->strategy = Strategy(this->kvstore, this->vstree);
	if ((this->query_tree.getQueryForm() == QueryTree::Select_Query) && this->query_tree.checkWellDesigned() && this->checkExpantionRewritingConnectivity(0))
	{
		//cout << "=================" << endl;
		//cout << "||well-designed||" << endl;
		//cout << "=================" << endl;

		this->distributed_queryRewriteEncodeRetrieveJoin(0, internal_tag_str);
		this->semantic_evaluation_result_stack.push(this->expansion_evaluation_stack[0].result);
	}
	else
	{
		//cout << "=====================" << endl;
		//cout << "||not well-designed||" << endl;
		//cout << "=====================" << endl;

		this->getBasicQuery(this->query_tree.getGroupPattern());

		this->sparql_query.encodeQuery(this->kvstore, this->getSPARQLQueryVarset());
		this->strategy.handle(this->sparql_query, internal_tag_str);
		
		this->generateEvaluationPlan(this->query_tree.getGroupPattern());
		this->doEvaluationPlan();
	}
}

void GeneralEvaluation::doQuery()
{
	this->query_tree.getGroupPattern().getVarset();
	if (this->query_tree.getGroupPattern().grouppattern_subject_object_maximal_varset.hasCommonVar(this->query_tree.getGroupPattern().grouppattern_predicate_maximal_varset))
	{
		cerr << "There are some vars occur both in subject/object and predicate." << endl;
		return;
	}

	this->strategy = Strategy(this->kvstore, this->vstree);
	if (this->query_tree.getQueryForm() == QueryTree::Select_Query && this->query_tree.checkWellDesigned() && this->checkExpantionRewritingConnectivity(0))
	{
		cout << "=================" << endl;
		cout << "||well-designed||" << endl;
		cout << "=================" << endl;

		this->queryRewriteEncodeRetrieveJoin(0);
		this->semantic_evaluation_result_stack.push(this->expansion_evaluation_stack[0].result);
	}
	else
	{
		cout << "=====================" << endl;
		cout << "||not well-designed||" << endl;
		cout << "=====================" << endl;

		this->getBasicQuery(this->query_tree.getGroupPattern());
		long tv_getbq = Util::get_cur_time();

		this->sparql_query.encodeQuery(this->kvstore, this->getSPARQLQueryVarset());
		cout << "sparqlSTR:\t" << this->sparql_query.to_str() << endl;
		long tv_encode = Util::get_cur_time();
		cout << "after Encode, used " << (tv_encode - tv_getbq) << "ms." << endl;

		this->strategy.handle(this->sparql_query);
		long tv_handle = Util::get_cur_time();
		cout << "after Handle, used " << (tv_handle - tv_encode) << "ms." << endl;

		this->generateEvaluationPlan(this->query_tree.getGroupPattern());
		this->doEvaluationPlan();
		long tv_postproc = Util::get_cur_time();
		cout << "after Postprocessing, used " << (tv_postproc - tv_handle) << "ms." << endl;
	}
}

void GeneralEvaluation::getBasicQuery(QueryTree::GroupPattern &grouppattern)
{
	for (int i = 0; i < (int)grouppattern.unions.size(); i++)
		for (int j = 0; j < (int)grouppattern.unions[i].grouppattern_vec.size(); j++)
			getBasicQuery(grouppattern.unions[i].grouppattern_vec[j]);
	for (int i = 0; i < (int)grouppattern.optionals.size(); i++)
		getBasicQuery(grouppattern.optionals[i].grouppattern);

	int current_optional = 0;
	int first_patternid = 0;

	grouppattern.initPatternBlockid();
	vector<int> basicqueryid((int)grouppattern.patterns.size(), 0);

	for (int i = 0; i < (int)grouppattern.patterns.size(); i++)
	{
		for (int j = first_patternid; j < i; j++)
		if (grouppattern.patterns[i].varset.hasCommonVar(grouppattern.patterns[j].varset))
			grouppattern.mergePatternBlockID(i, j);

		if ((current_optional != (int)grouppattern.optionals.size() && i == grouppattern.optionals[current_optional].lastpattern) || i + 1 == (int)grouppattern.patterns.size())
		{
			for (int j = first_patternid; j <= i; j++)
				if ((int)grouppattern.patterns[j].varset.varset.size() > 0)
				{
					if (grouppattern.getRootPatternBlockID(j) == j)			//root node
					{
						this->sparql_query.addBasicQuery();
						this->sparql_query_varset.push_back(Varset());

						for (int k = first_patternid; k <= i; k++)
							if (grouppattern.getRootPatternBlockID(k) == j)
							{
								this->sparql_query.addTriple(Triple(
									grouppattern.patterns[k].subject.value,
									grouppattern.patterns[k].predicate.value,
									grouppattern.patterns[k].object.value));

									basicqueryid[k] = this->sparql_query.getBasicQueryNum() - 1;
									this->sparql_query_varset[(int)this->sparql_query_varset.size() - 1] = this->sparql_query_varset[(int)this->sparql_query_varset.size() - 1] + grouppattern.patterns[k].varset;
							}
					}
				}
				else	basicqueryid[j] = -1;

			for (int j = first_patternid; j <= i; j++)
				grouppattern.pattern_blockid[j] = basicqueryid[j];

			if (current_optional != (int)grouppattern.optionals.size())	current_optional++;
			first_patternid = i + 1;
		}
	}

	for(int i = 0; i < (int)grouppattern.filter_exists_grouppatterns.size(); i++)
		for (int j = 0; j < (int)grouppattern.filter_exists_grouppatterns[i].size(); j++)
			getBasicQuery(grouppattern.filter_exists_grouppatterns[i][j]);
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue
	GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::operator ! ()
{
	if (this->value == true_value)	return false_value;
	if (this->value == false_value)	return true_value;
	if (this->value == error_value)	return error_value;

	return error_value;
}

GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue
	GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::operator || (const EffectiveBooleanValue &x)
{
	if (this->value == true_value && x.value == true_value)		return true_value;
	if (this->value == true_value && x.value == false_value)	return true_value;
	if (this->value == false_value && x.value == true_value)	return true_value;
	if (this->value == false_value && x.value == false_value)	return false_value;
	if (this->value == true_value && x.value == error_value)	return true_value;
	if (this->value == error_value && x.value == true_value)	return true_value;
	if (this->value == false_value && x.value == error_value)	return error_value;
	if (this->value == error_value && x.value == false_value)	return error_value;
	if (this->value == error_value && x.value == error_value)	return error_value;

	return error_value;
}

GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue
	GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::operator && (const EffectiveBooleanValue &x)
{
	if (this->value == true_value && x.value == true_value)		return true_value;
	if (this->value == true_value && x.value == false_value)	return false_value;
	if (this->value == false_value && x.value == true_value)	return false_value;
	if (this->value == false_value && x.value == false_value)	return false_value;
	if (this->value == true_value && x.value == error_value)	return error_value;
	if (this->value == error_value && x.value == true_value)	return error_value;
	if (this->value == false_value && x.value == error_value)	return false_value;
	if (this->value == error_value && x.value == false_value)	return false_value;
	if (this->value == error_value && x.value == error_value)	return error_value;

	return error_value;
}

GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue
	GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::operator == (const EffectiveBooleanValue &x)
{
	if (this->value == true_value && x.value == true_value)		return true_value;
	if (this->value == true_value && x.value == false_value)	return false_value;
	if (this->value == false_value && x.value == true_value)	return false_value;
	if (this->value == false_value && x.value == false_value)	return true_value;

	return error_value;
}

GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue
	GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::operator != (const EffectiveBooleanValue &x)
{
	if (this->value == true_value && x.value == true_value)		return false_value;
	if (this->value == true_value && x.value == false_value)	return true_value;
	if (this->value == false_value && x.value == true_value)	return true_value;
	if (this->value == false_value && x.value == false_value)	return false_value;

	return error_value;
}

GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue
	GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::operator < (const EffectiveBooleanValue &x)
{
	if (this->value == true_value && x.value == true_value)		return false_value;
	if (this->value == true_value && x.value == false_value)	return false_value;
	if (this->value == false_value && x.value == true_value)	return true_value;
	if (this->value == false_value && x.value == false_value)	return false_value;

	return error_value;
}

GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue
	GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::operator <= (const EffectiveBooleanValue &x)
{
	if (this->value == true_value && x.value == true_value)		return true_value;
	if (this->value == true_value && x.value == false_value)	return false_value;
	if (this->value == false_value && x.value == true_value)	return true_value;
	if (this->value == false_value && x.value == false_value)	return true_value;

	return error_value;
}

GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue
	GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::operator > (const EffectiveBooleanValue &x)
{
	if (this->value == true_value && x.value == true_value)		return false_value;
	if (this->value == true_value && x.value == false_value)	return true_value;
	if (this->value == false_value && x.value == true_value)	return false_value;
	if (this->value == false_value && x.value == false_value)	return false_value;

	return error_value;
}

GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue
	GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::operator >= (const EffectiveBooleanValue &x)
{
	if (this->value == true_value && x.value == true_value)		return true_value;
	if (this->value == true_value && x.value == false_value)	return true_value;
	if (this->value == false_value && x.value == true_value)	return false_value;
	if (this->value == false_value && x.value == false_value)	return true_value;

	return error_value;
}

GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue
	GeneralEvaluation::FilterEvaluationMultitypeValue::DateTime::operator == (const DateTime &x)
{
	if (this->date == x.date)	return GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::true_value;
	else	return GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::false_value;
}

GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue
	GeneralEvaluation::FilterEvaluationMultitypeValue::DateTime::operator != (const DateTime &x)
{
	if (this->date != x.date)	return GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::true_value;
	else	return GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::false_value;
}

GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue
	GeneralEvaluation::FilterEvaluationMultitypeValue::DateTime::operator < (const DateTime &x)
{
	if (this->date < x.date)	return GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::true_value;
	else	return GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::false_value;
}

GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue
	GeneralEvaluation::FilterEvaluationMultitypeValue::DateTime::operator <= (const DateTime &x)
{
	if (this->date <= x.date)	return GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::true_value;
	else	return GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::false_value;
}

GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue
	GeneralEvaluation::FilterEvaluationMultitypeValue::DateTime::operator > (const DateTime &x)
{
	if (this->date > x.date)	return GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::true_value;
	else	return GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::false_value;
}

GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue
	GeneralEvaluation::FilterEvaluationMultitypeValue::DateTime::operator >= (const DateTime &x)
{
	if (this->date >= x.date)	return GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::true_value;
	else	return GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::false_value;
}


GeneralEvaluation::FilterEvaluationMultitypeValue
	GeneralEvaluation::FilterEvaluationMultitypeValue::operator !()
{
	FilterEvaluationMultitypeValue ret_femv;
	ret_femv.datatype = xsd_boolean;
	ret_femv.bool_value = EffectiveBooleanValue::error_value;

	if (this->datatype != xsd_boolean)
		return ret_femv;

	ret_femv.bool_value = !this->bool_value;
	return ret_femv;
}

GeneralEvaluation::FilterEvaluationMultitypeValue
	GeneralEvaluation::FilterEvaluationMultitypeValue::operator || (FilterEvaluationMultitypeValue &x)
{
	FilterEvaluationMultitypeValue ret_femv;
	ret_femv.datatype = xsd_boolean;
	ret_femv.bool_value = EffectiveBooleanValue::error_value;

	if (this->datatype != xsd_boolean && x.datatype != xsd_boolean)
		return ret_femv;

	ret_femv.bool_value = (this->bool_value || x.bool_value);
	return ret_femv;
}

GeneralEvaluation::FilterEvaluationMultitypeValue
	GeneralEvaluation::FilterEvaluationMultitypeValue::operator && (FilterEvaluationMultitypeValue &x)
{
	FilterEvaluationMultitypeValue ret_femv;
	ret_femv.datatype = xsd_boolean;
	ret_femv.bool_value = EffectiveBooleanValue::error_value;

	if (this->datatype != xsd_boolean && x.datatype != xsd_boolean)
		return ret_femv;

	ret_femv.bool_value = (this->bool_value && x.bool_value);
	return ret_femv;
}

void GeneralEvaluation::FilterEvaluationMultitypeValue::getSameNumericType (FilterEvaluationMultitypeValue &x)
{
	DataType to_type = max(this->datatype, x.datatype);

	if (this->datatype == xsd_integer && to_type == xsd_decimal)
		this->flt_value = this->int_value;
	if (this->datatype == xsd_integer && to_type == xsd_float)
		this->flt_value = this->int_value;
	if (this->datatype == xsd_integer && to_type == xsd_double)
		this->dbl_value = this->int_value;
	if (this->datatype == xsd_decimal && to_type == xsd_double)
		this->dbl_value = this->flt_value;
	if (this->datatype == xsd_float && to_type == xsd_double)
		this->dbl_value = this->flt_value;
	this->datatype = to_type;

	if (x.datatype == xsd_integer && to_type == xsd_decimal)
		x.flt_value = x.int_value;
	if (x.datatype == xsd_integer && to_type == xsd_float)
		x.flt_value = x.int_value;
	if (x.datatype == xsd_integer && to_type == xsd_double)
		x.dbl_value = x.int_value;
	if (x.datatype == xsd_decimal && to_type == xsd_double)
		x.dbl_value = x.flt_value;
	if (x.datatype == xsd_float && to_type == xsd_double)
		x.dbl_value = x.flt_value;
	x.datatype = to_type;
}

GeneralEvaluation::FilterEvaluationMultitypeValue
	GeneralEvaluation::FilterEvaluationMultitypeValue::operator == (FilterEvaluationMultitypeValue &x)
{
	FilterEvaluationMultitypeValue ret_femv;
	ret_femv.datatype = xsd_boolean;
	ret_femv.bool_value = EffectiveBooleanValue::error_value;

	if (this->datatype == xsd_boolean && x.datatype == xsd_boolean)
	{
		ret_femv.bool_value = (this->bool_value == x.bool_value);
		return ret_femv;
	}

	if (xsd_integer <= this->datatype && this->datatype <= xsd_double && xsd_integer <= x.datatype && x.datatype <= xsd_double)
	{
		this->getSameNumericType(x);

		if (this->datatype == xsd_integer && x.datatype == xsd_integer)
			ret_femv.bool_value = (this->int_value == x.int_value);
		if (this->datatype == xsd_decimal && x.datatype == xsd_decimal)
			ret_femv.bool_value = (this->flt_value == x.flt_value);
		if (this->datatype == xsd_float && x.datatype == xsd_float)
			ret_femv.bool_value = (this->flt_value == x.flt_value);
		if (this->datatype == xsd_double && x.datatype == xsd_double)
			ret_femv.bool_value = (this->dbl_value == x.dbl_value);

		return ret_femv;
	}

	 if (this->datatype == simple_literal && x.datatype == simple_literal)
	 {
		 ret_femv.bool_value = (this->str_value == x.str_value);
		 return ret_femv;
	 }

	 if (this->datatype == xsd_string && x.datatype == xsd_string)
	 {
		 ret_femv.bool_value = (this->str_value == x.str_value);
		 return ret_femv;
	 }

	 if (this->datatype == xsd_datetime && x.datatype == xsd_datetime)
	 {
		 ret_femv.bool_value = (this->dt_value == x.dt_value);
		 return ret_femv;
	 }

	 ret_femv.bool_value = (this->term_value == x.term_value);
	 return ret_femv;
}

GeneralEvaluation::FilterEvaluationMultitypeValue
	GeneralEvaluation::FilterEvaluationMultitypeValue::operator != (FilterEvaluationMultitypeValue &x)
{
	FilterEvaluationMultitypeValue ret_femv;
	ret_femv.datatype = xsd_boolean;
	ret_femv.bool_value = EffectiveBooleanValue::error_value;

	if (this->datatype == xsd_boolean && x.datatype == xsd_boolean)
	{
		ret_femv.bool_value = (this->bool_value != x.bool_value);
		return ret_femv;
	}

	if (xsd_integer <= this->datatype && this->datatype <= xsd_double && xsd_integer <= x.datatype && x.datatype <= xsd_double)
	{
		this->getSameNumericType(x);

		if (this->datatype == xsd_integer && x.datatype == xsd_integer)
			ret_femv.bool_value = (this->int_value != x.int_value);
		if (this->datatype == xsd_decimal && x.datatype == xsd_decimal)
			ret_femv.bool_value = (this->flt_value != x.flt_value);
		if (this->datatype == xsd_float && x.datatype == xsd_float)
			ret_femv.bool_value = (this->flt_value != x.flt_value);
		if (this->datatype == xsd_double && x.datatype == xsd_double)
			ret_femv.bool_value = (this->dbl_value != x.dbl_value);

		return ret_femv;
	}

	 if (this->datatype == simple_literal && x.datatype == simple_literal)
	 {
		 ret_femv.bool_value = (this->str_value != x.str_value);
		 return ret_femv;
	 }

	 if (this->datatype == xsd_string && x.datatype == xsd_string)
	 {
		 ret_femv.bool_value = (this->str_value != x.str_value);
		 return ret_femv;
	 }

	 if (this->datatype == xsd_datetime && x.datatype == xsd_datetime)
	 {
		 ret_femv.bool_value = (this->dt_value != x.dt_value);
		 return ret_femv;
	 }

	 ret_femv.bool_value = (this->term_value != x.term_value);
	 return ret_femv;
}

GeneralEvaluation::FilterEvaluationMultitypeValue
	GeneralEvaluation::FilterEvaluationMultitypeValue::operator < (FilterEvaluationMultitypeValue &x)
{
	FilterEvaluationMultitypeValue ret_femv;
	ret_femv.datatype = xsd_boolean;
	ret_femv.bool_value = EffectiveBooleanValue::error_value;

	if (this->datatype == xsd_boolean && x.datatype == xsd_boolean)
	{
		ret_femv.bool_value = (this->bool_value < x.bool_value);
		return ret_femv;
	}

	if (xsd_integer <= this->datatype && this->datatype <= xsd_double && xsd_integer <= x.datatype && x.datatype <= xsd_double)
	{
		this->getSameNumericType(x);

		if (this->datatype == xsd_integer && x.datatype == xsd_integer)
			ret_femv.bool_value = (this->int_value < x.int_value);
		if (this->datatype == xsd_decimal && x.datatype == xsd_decimal)
			ret_femv.bool_value = (this->flt_value < x.flt_value);
		if (this->datatype == xsd_float && x.datatype == xsd_float)
			ret_femv.bool_value = (this->flt_value < x.flt_value);
		if (this->datatype == xsd_double && x.datatype == xsd_double)
			ret_femv.bool_value = (this->dbl_value < x.dbl_value);

		return ret_femv;
	}

	 if (this->datatype == simple_literal && x.datatype == simple_literal)
	 {
		 ret_femv.bool_value = (this->str_value < x.str_value);
		 return ret_femv;
	 }

	 if (this->datatype == xsd_string && x.datatype == xsd_string)
	 {
		 ret_femv.bool_value = (this->str_value < x.str_value);
		 return ret_femv;
	 }

	 if (this->datatype == xsd_datetime && x.datatype == xsd_datetime)
	 {
		 ret_femv.bool_value = (this->dt_value < x.dt_value);
		 return ret_femv;
	 }

	 return ret_femv;
}

GeneralEvaluation::FilterEvaluationMultitypeValue
	GeneralEvaluation::FilterEvaluationMultitypeValue::operator <= (FilterEvaluationMultitypeValue &x)
{
	FilterEvaluationMultitypeValue ret_femv;
	ret_femv.datatype = xsd_boolean;
	ret_femv.bool_value = EffectiveBooleanValue::error_value;

	if (this->datatype == xsd_boolean && x.datatype == xsd_boolean)
	{
		ret_femv.bool_value = (this->bool_value <= x.bool_value);
		return ret_femv;
	}

	if (xsd_integer <= this->datatype && this->datatype <= xsd_double && xsd_integer <= x.datatype && x.datatype <= xsd_double)
	{
		this->getSameNumericType(x);

		if (this->datatype == xsd_integer && x.datatype == xsd_integer)
			ret_femv.bool_value = (this->int_value <= x.int_value);
		if (this->datatype == xsd_decimal && x.datatype == xsd_decimal)
			ret_femv.bool_value = (this->flt_value <= x.flt_value);
		if (this->datatype == xsd_float && x.datatype == xsd_float)
			ret_femv.bool_value = (this->flt_value <= x.flt_value);
		if (this->datatype == xsd_double && x.datatype == xsd_double)
			ret_femv.bool_value = (this->dbl_value <= x.dbl_value);

		return ret_femv;
	}

	 if (this->datatype == simple_literal && x.datatype == simple_literal)
	 {
		 ret_femv.bool_value = (this->str_value <= x.str_value);
		 return ret_femv;
	 }

	 if (this->datatype == xsd_string && x.datatype == xsd_string)
	 {
		 ret_femv.bool_value = (this->str_value <= x.str_value);
		 return ret_femv;
	 }

	 if (this->datatype == xsd_datetime && x.datatype == xsd_datetime)
	 {
		 ret_femv.bool_value = (this->dt_value <= x.dt_value);
		 return ret_femv;
	 }

	 return ret_femv;
}

GeneralEvaluation::FilterEvaluationMultitypeValue
	GeneralEvaluation::FilterEvaluationMultitypeValue::operator > (FilterEvaluationMultitypeValue &x)
{
	FilterEvaluationMultitypeValue ret_femv;
	ret_femv.datatype = xsd_boolean;
	ret_femv.bool_value = EffectiveBooleanValue::error_value;

	if (this->datatype == xsd_boolean && x.datatype == xsd_boolean)
	{
		ret_femv.bool_value = (this->bool_value > x.bool_value);
		return ret_femv;
	}

	if (xsd_integer <= this->datatype && this->datatype <= xsd_double && xsd_integer <= x.datatype && x.datatype <= xsd_double)
	{
		this->getSameNumericType(x);

		if (this->datatype == xsd_integer && x.datatype == xsd_integer)
			ret_femv.bool_value = (this->int_value > x.int_value);
		if (this->datatype == xsd_decimal && x.datatype == xsd_decimal)
			ret_femv.bool_value = (this->flt_value > x.flt_value);
		if (this->datatype == xsd_float && x.datatype == xsd_float)
			ret_femv.bool_value = (this->flt_value > x.flt_value);
		if (this->datatype == xsd_double && x.datatype == xsd_double)
			ret_femv.bool_value = (this->dbl_value > x.dbl_value);

		return ret_femv;
	}

	 if (this->datatype == simple_literal && x.datatype == simple_literal)
	 {
		 ret_femv.bool_value = (this->str_value > x.str_value);
		 return ret_femv;
	 }

	 if (this->datatype == xsd_string && x.datatype == xsd_string)
	 {
		 ret_femv.bool_value = (this->str_value > x.str_value);
		 return ret_femv;
	 }

	 if (this->datatype == xsd_datetime && x.datatype == xsd_datetime)
	 {
		 ret_femv.bool_value = (this->dt_value > x.dt_value);
		 return ret_femv;
	 }

	 return ret_femv;
}

GeneralEvaluation::FilterEvaluationMultitypeValue
	GeneralEvaluation::FilterEvaluationMultitypeValue::operator >= (FilterEvaluationMultitypeValue &x)
{
	FilterEvaluationMultitypeValue ret_femv;
	ret_femv.datatype = xsd_boolean;
	ret_femv.bool_value = EffectiveBooleanValue::error_value;

	if (this->datatype == xsd_boolean && x.datatype == xsd_boolean)
	{
		ret_femv.bool_value = (this->bool_value >= x.bool_value);
		return ret_femv;
	}

	if (xsd_integer <= this->datatype && this->datatype <= xsd_double && xsd_integer <= x.datatype && x.datatype <= xsd_double)
	{
		this->getSameNumericType(x);

		if (this->datatype == xsd_integer && x.datatype == xsd_integer)
			ret_femv.bool_value = (this->int_value >= x.int_value);
		if (this->datatype == xsd_decimal && x.datatype == xsd_decimal)
			ret_femv.bool_value = (this->flt_value >= x.flt_value);
		if (this->datatype == xsd_float && x.datatype == xsd_float)
			ret_femv.bool_value = (this->flt_value >= x.flt_value);
		if (this->datatype == xsd_double && x.datatype == xsd_double)
			ret_femv.bool_value = (this->dbl_value >= x.dbl_value);

		return ret_femv;
	}

	 if (this->datatype == simple_literal && x.datatype == simple_literal)
	 {
		 ret_femv.bool_value = (this->str_value >= x.str_value);
		 return ret_femv;
	 }

	 if (this->datatype == xsd_string && x.datatype == xsd_string)
	 {
		 ret_femv.bool_value = (this->str_value >= x.str_value);
		 return ret_femv;
	 }

	 if (this->datatype == xsd_datetime && x.datatype == xsd_datetime)
	 {
		 ret_femv.bool_value = (this->dt_value >= x.dt_value);
		 return ret_femv;
	 }

	 return ret_femv;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void GeneralEvaluation::TempResult::release()
{
	for (int i = 0; i < (int)this->res.size(); i++)
		delete[] res[i];
}

int GeneralEvaluation::TempResult::compareFunc(int *a, vector<int> &p, int *b, vector<int> &q)			//compare a[p] & b[q]
{
	int p_size = (int)p.size();
	for (int i = 0; i < p_size; i++)
	{
		if (a[p[i]] < b[q[i]])		return -1;
		if (a[p[i]] > b[q[i]])		return 1;
	}
	return 0;
}

void GeneralEvaluation::TempResult::sort(int l, int r, vector<int> &p)
{
	int i = l, j = r;
	int *x = this->res[(l + r) / 2];
	do
	{
		while (compareFunc(this->res[i], p, x, p) == -1)	i++;
		while(compareFunc(x, p, this->res[j], p) == -1)	j--;
		if (i <= j)
		{
			swap(this->res[i], this->res[j]);
			i++;
			j--;
		}
	}
	while (i <= j);
	if (l < j)	sort(l, j, p);
	if (i < r)	sort(i, r, p);
}

int GeneralEvaluation::TempResult::findLeftBounder(vector<int> &p, int *b, vector<int> &q)
{
	int l = 0, r = (int)this->res.size() - 1;

	if (r == -1)
		return -1;

	while (l < r)
	{
		int m = (l + r) / 2;
		if (compareFunc(this->res[m], p, b, q) >= 0)		r = m;
		else l = m + 1;
	}
	if (compareFunc(this->res[l], p, b, q) == 0)		return l;
	else return -1;
}

int GeneralEvaluation::TempResult::findRightBounder(vector<int> &p, int *b, vector<int> &q)
{
	int l = 0, r = (int)this->res.size() - 1;

	if (r == -1)
		return -1;

	while (l < r)
	{
		int m = (l + r) / 2 + 1;
		if (compareFunc(this->res[m], p, b, q) <= 0)		l = m;
		else r = m - 1;
	}
	if (compareFunc(this->res[l], p, b, q) == 0)		return l;
	else return -1;
}

void GeneralEvaluation::TempResult::doJoin(TempResult &x, TempResult &r)
{
	if ((int)r.var.varset.size() == 0)		return;

	vector <int> this2r = this->var.mapTo(r.var);
	vector <int> x2r = x.var.mapTo(r.var);

	Varset common_var = this->var * x.var;
	int r_varnum = (int)r.var.varset.size();
	int this_varnum = (int)this->var.varset.size();
	int x_varnum = (int)x.var.varset.size();
	if ((int)common_var.varset.size() == 0)
	{
		for (int i = 0; i < (int)this->res.size(); i++)
			for (int j = 0; j < (int)x.res.size(); j++)
			{
				int *a = new int [r_varnum];
				for (int k = 0; k < this_varnum; k++)
					a[this2r[k]] = this->res[i][k];
				for (int k = 0; k < x_varnum; k++)
					a[x2r[k]] = x.res[j][k];
				r.res.push_back(a);
			}
	}
	else
	if ((int)x.res.size() > 0)
	{
		vector<int> common2x = common_var.mapTo(x.var);
		x.sort(0, (int)x.res.size() - 1, common2x);

		vector<int> common2this = common_var.mapTo(this->var);
		for (int i = 0; i < (int)this->res.size(); i++)
		{
			int left, right;
			left = x.findLeftBounder(common2x, this->res[i], common2this);
			if (left == -1)	continue;
			right = x.findRightBounder(common2x, this->res[i], common2this);

			for (int j = left; j <= right; j++)
			{
				int *a = new int [r_varnum];
				for (int k = 0; k < this_varnum; k++)
					a[this2r[k]] = this->res[i][k];
				for (int k = 0; k < x_varnum; k++)
					a[x2r[k]] = x.res[j][k];
				r.res.push_back(a);
			}
		}
	}
}

void GeneralEvaluation::TempResult::doUnion(TempResult &rt)
{
	vector <int> this2rt = this->var.mapTo(rt.var);

	int rt_varnum = (int)rt.var.varset.size();
	int this_varnum = (int)this->var.varset.size();
	for (int i = 0; i < (int)this->res.size(); i++)
	{
		int *a = new int [rt_varnum];
		for (int j = 0; j < this_varnum; j++)
			a[this2rt[j]] = this->res[i][j];
		rt.res.push_back(a);
	}
}

void GeneralEvaluation::TempResult::doOptional(vector<bool> &binding, TempResult &x, TempResult &rn, TempResult &ra, bool add_no_binding)
{
	vector <int> this2rn = this->var.mapTo(rn.var);

	Varset common_var = this->var * x.var;
	if ((int)common_var.varset.size() != 0 && (int)x.res.size() != 0)
	{
		vector <int> this2ra = this->var.mapTo(ra.var);
		vector <int> x2ra = x.var.mapTo(ra.var);

		vector<int> common2x = common_var.mapTo(x.var);
		x.sort(0, (int)x.res.size() - 1, common2x);

		vector<int> common2this = common_var.mapTo(this->var);
		int ra_varnum = (int)ra.var.varset.size();
		int this_varnum = (int)this->var.varset.size();
		int x_varnum = (int)x.var.varset.size();
		for (int i = 0; i < (int)this->res.size(); i++)
		{
			int left, right;
			left = x.findLeftBounder(common2x, this->res[i], common2this);
			if (left != -1)
			{
				binding[i] = true;
				right = x.findRightBounder(common2x, this->res[i], common2this);
				for (int j = left; j <= right; j++)
				{
					int *a = new int [ra_varnum];
					for (int k = 0; k < this_varnum; k++)
						a[this2ra[k]] = this->res[i][k];
					for (int k = 0; k < x_varnum; k++)
						a[x2ra[k]] = x.res[j][k];
					ra.res.push_back(a);
				}
			}
		}
	}

	if (add_no_binding)
	{
		int rn_varnum = (int)rn.var.varset.size();
		int this_varnum = (int)this->var.varset.size();
		for (int i = 0; i < (int)this->res.size(); i++)
		if (!binding[i])
		{
			int *a = new int [rn_varnum];
			for (int j = 0; j < this_varnum; j++)
				a[this2rn[j]] = this->res[i][j];
			rn.res.push_back(a);
		}
	}
}

void GeneralEvaluation::TempResult::doMinus(TempResult &x, TempResult &r)
{
	vector <int> this2r = this->var.mapTo(r.var);

	Varset common_var = this->var * x.var;
	int r_varnum = (int)r.var.varset.size();
	int this_varnum = (int)this->var.varset.size();
	if ((int)common_var.varset.size() == 0)
	{
		for (int i = 0; i < (int)this->res.size(); i++)
		{
			int *a = new int [r_varnum];
			for (int j = 0; j < this_varnum; j++)
				a[this2r[j]] = this->res[i][j];
			r.res.push_back(a);
		}
	}
	else
	if ((int)x.res.size() > 0)
	{
		vector<int> common2x = common_var.mapTo(x.var);
		x.sort(0, (int)x.res.size() - 1, common2x);

		vector<int> common2this = common_var.mapTo(this->var);
		for (int i = 0; i < (int)this->res.size(); i++)
		{
			int left = x.findLeftBounder(common2x, this->res[i], common2this);
			if (left == -1)
			{
				int *a = new int [r_varnum];
				for (int j = 0; j < this_varnum; j++)
					a[this2r[j]] = this->res[i][j];
				r.res.push_back(a);
			}
		}
	}
}

void GeneralEvaluation::TempResult::doDistinct(TempResult &r)
{
	vector <int> this2r = this->var.mapTo(r.var);

	if ((int)this->res.size() > 0)
	{
		vector<int> r2this = r.var.mapTo(this->var);
		this->sort(0, (int)this->res.size() - 1, r2this);

		int r_varnum = (int)r.var.varset.size();
		int this_varnum = (int)this->var.varset.size();
		for (int i = 0; i < (int)this->res.size(); i++)
		if (i == 0 || (i > 0 && compareFunc(this->res[i], r2this, this->res[i - 1], r2this) != 0))
		{
			int *a = new int [r_varnum];
			for (int j = 0; j < this_varnum; j++)
				if (this2r[j] != -1)
					a[this2r[j]] = this->res[i][j];
			r.res.push_back(a);
		}
	}
}



void GeneralEvaluation::TempResult::mapFilterTree2Varset(QueryTree::GroupPattern::FilterTreeNode &filter, Varset &v, Varset &entity_literal_varset)
{
	if (filter.oper_type == QueryTree::GroupPattern::FilterTreeNode::Not_type)
	{
		mapFilterTree2Varset(filter.child[0].node, v, entity_literal_varset);
	}
	else if (filter.oper_type == QueryTree::GroupPattern::FilterTreeNode::Or_type || filter.oper_type == QueryTree::GroupPattern::FilterTreeNode::And_type)
	{
		mapFilterTree2Varset(filter.child[0].node, v, entity_literal_varset);
		mapFilterTree2Varset(filter.child[1].node, v, entity_literal_varset);
	}
	else if (QueryTree::GroupPattern::FilterTreeNode::Equal_type <= filter.oper_type && filter.oper_type <= QueryTree::GroupPattern::FilterTreeNode::GreaterOrEqual_type)
	{
		if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			mapFilterTree2Varset(filter.child[0].node, v, entity_literal_varset);
		else if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type && filter.child[0].arg[0] == '?')
		{
			filter.child[0].pos = Varset(filter.child[0].arg).mapTo(v)[0];
			if (entity_literal_varset.findVar(filter.child[0].arg))	filter.child[0].isel = true;
			else filter.child[0].isel = false;
		}

		if (filter.child[1].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			mapFilterTree2Varset(filter.child[1].node, v, entity_literal_varset);
		else if (filter.child[1].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type && filter.child[1].arg[0] == '?')
		{
			filter.child[1].pos = Varset(filter.child[1].arg).mapTo(v)[0];
			if (entity_literal_varset.findVar(filter.child[1].arg))	filter.child[1].isel = true;
			else filter.child[1].isel = false;
		}
	}
	else if (filter.oper_type == QueryTree::GroupPattern::FilterTreeNode::Builtin_regex_type || filter.oper_type == QueryTree::GroupPattern::FilterTreeNode::Builtin_lang_type || filter.oper_type == QueryTree::GroupPattern::FilterTreeNode::Builtin_langmatches_type || filter.oper_type == QueryTree::GroupPattern::FilterTreeNode::Builtin_bound_type || filter.oper_type == QueryTree::GroupPattern::FilterTreeNode::Builtin_in_type)
	{
		if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			mapFilterTree2Varset(filter.child[0].node, v, entity_literal_varset);
		else if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type && filter.child[0].arg[0] == '?')
		{
			filter.child[0].pos = Varset(filter.child[0].arg).mapTo(v)[0];
			if (entity_literal_varset.findVar(filter.child[0].arg))	filter.child[0].isel = true;
			else filter.child[0].isel = false;
		}
	}
}

void GeneralEvaluation::TempResult::doFilter(QueryTree::GroupPattern::FilterTreeNode &filter, FilterExistsGroupPatternResultSetRecord &filter_exists_grouppattern_resultset_record, TempResult &r, StringIndex *stringindex, Varset &entity_literal_varset)
{
	mapFilterTree2Varset(filter, this->var, entity_literal_varset);

	r.var = this->var;

	int varnum = (int)this->var.varset.size();
	for (int i = 0; i < (int)this->res.size(); i++)
	{
		GeneralEvaluation::FilterEvaluationMultitypeValue ret_femv = matchFilterTree(filter, filter_exists_grouppattern_resultset_record, this->res[i], stringindex);
		if (ret_femv.datatype == GeneralEvaluation::FilterEvaluationMultitypeValue::xsd_boolean && ret_femv.bool_value.value == GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::true_value)
		{
			int *a = new int[varnum];
			memcpy(a, this->res[i], sizeof(int) * varnum);
			r.res.push_back(a);
		}
	}
}

void GeneralEvaluation::TempResult::getFilterString(QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild &child, FilterEvaluationMultitypeValue &femv, int *row, StringIndex *stringindex)
{
	if (child.node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
	{
		femv.datatype = GeneralEvaluation::FilterEvaluationMultitypeValue::rdf_term;

		if (child.arg[0] == '?')
		{
			int id = -1;
			if (child.pos != -1)	id = row[child.pos];

			femv.term_value = "";
			if (id != -1)
			{
				if (child.isel)
					stringindex->randomAccess(id, &femv.term_value, true);
				else
					stringindex->randomAccess(id, &femv.term_value, false);
			}
		}
		else femv.term_value = child.arg;

		//' to "
		if (femv.term_value[0] == '\'')
		{
			femv.term_value[0] = '"';
			femv.term_value[femv.term_value.rfind('\'')] = '"';
		}

		if (femv.term_value[0] == '<' && femv.term_value[femv.term_value.length() - 1] == '>')
		{
			femv.datatype = GeneralEvaluation::FilterEvaluationMultitypeValue::iri;
			femv.str_value = femv.term_value;
		}

		if (femv.term_value[0] == '"' && femv.term_value.find("\"^^<") == -1 && femv.term_value[femv.term_value.length() - 1] != '>' )
		{
			femv.datatype = GeneralEvaluation::FilterEvaluationMultitypeValue::simple_literal;
			femv.str_value = femv.term_value;
		}

		if (femv.term_value[0] == '"' && femv.term_value.find("^^<http://www.w3.org/2001/XMLSchema#string>") != -1)
		{
			femv.datatype = GeneralEvaluation::FilterEvaluationMultitypeValue::xsd_string;
			femv.str_value = femv.term_value.substr(0, femv.term_value.find("^^<http://www.w3.org/2001/XMLSchema#string>"));
		}

		if (femv.term_value[0] == '"' && femv.term_value.find("^^<http://www.w3.org/2001/XMLSchema#boolean>") != -1)
		{
			femv.datatype = GeneralEvaluation::FilterEvaluationMultitypeValue::xsd_boolean;

			string value = femv.term_value.substr(0, femv.term_value.find("^^<http://www.w3.org/2001/XMLSchema#boolean>"));
			if (value == "\"true\"")
				femv.bool_value = GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::true_value;
			else if (value == "\"false\"")
				femv.bool_value = GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::false_value;
			else
				femv.bool_value = GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::error_value;
		}

		if (femv.term_value[0] == '"' && femv.term_value.find("^^<http://www.w3.org/2001/XMLSchema#integer>") != -1)
		{
			femv.datatype = GeneralEvaluation::FilterEvaluationMultitypeValue::xsd_integer;

			string value = femv.term_value.substr(1, femv.term_value.find("^^<http://www.w3.org/2001/XMLSchema#integer>") - 2);
			stringstream ss;
			ss << value;
			ss >> femv.int_value;
		}

		if (femv.term_value[0] == '"' && femv.term_value.find("^^<http://www.w3.org/2001/XMLSchema#decimal>") != -1)
		{
			femv.datatype = GeneralEvaluation::FilterEvaluationMultitypeValue::xsd_decimal;

			string value = femv.term_value.substr(1, femv.term_value.find("^^<http://www.w3.org/2001/XMLSchema#decimal>") - 2);
			stringstream ss;
			ss << value;
			ss >> femv.flt_value;
		}

		if (femv.term_value[0] == '"' && femv.term_value.find("^^<http://www.w3.org/2001/XMLSchema#float>") != -1)
		{
			femv.datatype = GeneralEvaluation::FilterEvaluationMultitypeValue::xsd_float;

			string value = femv.term_value.substr(1, femv.term_value.find("^^<http://www.w3.org/2001/XMLSchema#float>") - 2);
			stringstream ss;
			ss << value;
			ss >> femv.flt_value;
		}

		if (femv.term_value[0] == '"' && femv.term_value.find("^^<http://www.w3.org/2001/XMLSchema#double>") != -1)
		{
			femv.datatype = GeneralEvaluation::FilterEvaluationMultitypeValue::xsd_double;

			string value = femv.term_value.substr(1, femv.term_value.find("^^<http://www.w3.org/2001/XMLSchema#double>") - 2);
			stringstream ss;
			ss << value;
			ss >> femv.dbl_value;
		}

		if (femv.term_value[0] == '"' && (femv.term_value.find("^^<http://www.w3.org/2001/XMLSchema#dateTime>") != -1 || femv.term_value.find("^^<http://www.w3.org/2001/XMLSchema#date>") != -1))
		{
			femv.datatype = GeneralEvaluation::FilterEvaluationMultitypeValue::xsd_datetime;

			string value;
			if (femv.term_value.find("^^<http://www.w3.org/2001/XMLSchema#dateTime>") != -1)
				value = femv.term_value.substr(1, femv.term_value.find("^^<http://www.w3.org/2001/XMLSchema#dateTime>") - 2);
			if (femv.term_value.find("^^<http://www.w3.org/2001/XMLSchema#date>") != -1)
				value = femv.term_value.substr(1, femv.term_value.find("^^<http://www.w3.org/2001/XMLSchema#date>") - 2);

			vector <int> date;
			for (int i = 0; i < 6; i++)
			if (value.length() > 0)
			{
				int p = 0;
				stringstream ss;
				while (p < value.length() && '0' <= value[p] && value[p] <= '9')
				{
					ss << value[p];
					p++;
				}

				int x;
				ss >> x;
				date.push_back(x);

				while (p < value.length() && (value[p] < '0' || value[p] > '9'))
					p++;
				value = value.substr(p);
			}
			else
				date.push_back(0);

			femv.dt_value = GeneralEvaluation::FilterEvaluationMultitypeValue::DateTime(date[0], date[1], date[2], date[3], date[4], date[5]);
		}
	}
}

GeneralEvaluation::FilterEvaluationMultitypeValue
	GeneralEvaluation::TempResult::matchFilterTree(QueryTree::GroupPattern::FilterTreeNode &filter, FilterExistsGroupPatternResultSetRecord &filter_exists_grouppattern_resultset_record, int *row, StringIndex *stringindex)
{
	FilterEvaluationMultitypeValue ret_femv;
	ret_femv.datatype = GeneralEvaluation::FilterEvaluationMultitypeValue::xsd_boolean;
	ret_femv.bool_value = GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::error_value;

	if (filter.oper_type == QueryTree::GroupPattern::FilterTreeNode::Not_type)
	{
		FilterEvaluationMultitypeValue x;

		if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
			getFilterString(filter.child[0], x, row, stringindex);
		else if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			x = matchFilterTree(filter.child[0].node, filter_exists_grouppattern_resultset_record, row, stringindex);

		return !x;
	}

	if (filter.oper_type == QueryTree::GroupPattern::FilterTreeNode::Or_type)
	{
		FilterEvaluationMultitypeValue x, y;

		if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
			getFilterString(filter.child[0], x, row, stringindex);
		else if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			x = matchFilterTree(filter.child[0].node, filter_exists_grouppattern_resultset_record, row, stringindex);

		if (filter.child[1].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
			getFilterString(filter.child[1], y, row, stringindex);
		else if (filter.child[1].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			y = matchFilterTree(filter.child[1].node, filter_exists_grouppattern_resultset_record, row, stringindex);

		return x || y;
	}

	if (filter.oper_type == QueryTree::GroupPattern::FilterTreeNode::And_type)
	{
		FilterEvaluationMultitypeValue x, y;

		if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
			getFilterString(filter.child[0], x, row, stringindex);
		else if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			x = matchFilterTree(filter.child[0].node, filter_exists_grouppattern_resultset_record, row, stringindex);

		if (filter.child[1].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
			getFilterString(filter.child[1], y, row, stringindex);
		else if (filter.child[1].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			y = matchFilterTree(filter.child[1].node, filter_exists_grouppattern_resultset_record, row, stringindex);

		return x && y;
	}

	if (filter.oper_type == QueryTree::GroupPattern::FilterTreeNode::Equal_type)
	{
		FilterEvaluationMultitypeValue x, y;

		if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
			getFilterString(filter.child[0], x, row, stringindex);
		else if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			x = matchFilterTree(filter.child[0].node, filter_exists_grouppattern_resultset_record, row, stringindex);

		if (filter.child[1].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
			getFilterString(filter.child[1], y, row, stringindex);
		else if (filter.child[1].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			y = matchFilterTree(filter.child[1].node, filter_exists_grouppattern_resultset_record, row, stringindex);

		return x == y;
	}

	if (filter.oper_type == QueryTree::GroupPattern::FilterTreeNode::NotEqual_type)
	{
		FilterEvaluationMultitypeValue x, y;

		if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
			getFilterString(filter.child[0], x, row, stringindex);
		else if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			x = matchFilterTree(filter.child[0].node, filter_exists_grouppattern_resultset_record, row, stringindex);

		if (filter.child[1].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
			getFilterString(filter.child[1], y, row, stringindex);
		else if (filter.child[1].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			y = matchFilterTree(filter.child[1].node, filter_exists_grouppattern_resultset_record, row, stringindex);

		return x != y;
	}

	if (filter.oper_type == QueryTree::GroupPattern::FilterTreeNode::Less_type)
	{
		FilterEvaluationMultitypeValue x, y;

		if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
			getFilterString(filter.child[0], x, row, stringindex);
		else if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			x = matchFilterTree(filter.child[0].node, filter_exists_grouppattern_resultset_record, row, stringindex);

		if (filter.child[1].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
			getFilterString(filter.child[1], y, row, stringindex);
		else if (filter.child[1].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			y = matchFilterTree(filter.child[1].node, filter_exists_grouppattern_resultset_record, row, stringindex);

		return x < y;
	}

	if (filter.oper_type == QueryTree::GroupPattern::FilterTreeNode::LessOrEqual_type)
	{
		FilterEvaluationMultitypeValue x, y;

		if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
			getFilterString(filter.child[0], x, row, stringindex);
		else if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			x = matchFilterTree(filter.child[0].node, filter_exists_grouppattern_resultset_record, row, stringindex);

		if (filter.child[1].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
			getFilterString(filter.child[1], y, row, stringindex);
		else if (filter.child[1].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			y = matchFilterTree(filter.child[1].node, filter_exists_grouppattern_resultset_record, row, stringindex);

		return x <= y;
	}

	if (filter.oper_type == QueryTree::GroupPattern::FilterTreeNode::Greater_type)
	{
		FilterEvaluationMultitypeValue x, y;

		if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
			getFilterString(filter.child[0], x, row, stringindex);
		else if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			x = matchFilterTree(filter.child[0].node, filter_exists_grouppattern_resultset_record, row, stringindex);

		if (filter.child[1].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
			getFilterString(filter.child[1], y, row, stringindex);
		else if (filter.child[1].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			y = matchFilterTree(filter.child[1].node, filter_exists_grouppattern_resultset_record, row, stringindex);

		return x > y;
	}

	if (filter.oper_type == QueryTree::GroupPattern::FilterTreeNode::GreaterOrEqual_type)
	{
		FilterEvaluationMultitypeValue x, y;

		if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
			getFilterString(filter.child[0], x, row, stringindex);
		else if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			x = matchFilterTree(filter.child[0].node, filter_exists_grouppattern_resultset_record, row, stringindex);

		if (filter.child[1].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
			getFilterString(filter.child[1], y, row, stringindex);
		else if (filter.child[1].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			y = matchFilterTree(filter.child[1].node, filter_exists_grouppattern_resultset_record, row, stringindex);

		return x >= y;
	}

	if (filter.oper_type == QueryTree::GroupPattern::FilterTreeNode::Builtin_regex_type)
	{
		FilterEvaluationMultitypeValue x, y, z;
		string t, p, f;

		if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
			getFilterString(filter.child[0], x, row, stringindex);
		else if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			x = matchFilterTree(filter.child[0].node, filter_exists_grouppattern_resultset_record, row, stringindex);
		if (x.datatype == GeneralEvaluation::FilterEvaluationMultitypeValue::simple_literal)
		{
			t = x.str_value;
			t = t.substr(1, t.rfind('"') - 1);
		}
		else
			return ret_femv;

		if (filter.child[1].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
			getFilterString(filter.child[1], y, row, stringindex);
		else if (filter.child[1].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			y = matchFilterTree(filter.child[1].node, filter_exists_grouppattern_resultset_record, row, stringindex);
		if (y.datatype == GeneralEvaluation::FilterEvaluationMultitypeValue::simple_literal)
		{
			p = y.str_value;
			p = p.substr(1, p.rfind('"') - 1);
		}
		else
			return ret_femv;

		if ((int)filter.child.size() >= 3)
		{
			if (filter.child[2].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
				getFilterString(filter.child[2], z, row, stringindex);
			else if (filter.child[2].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
				z = matchFilterTree(filter.child[2].node, filter_exists_grouppattern_resultset_record, row, stringindex);
			if (z.datatype == GeneralEvaluation::FilterEvaluationMultitypeValue::simple_literal)
			{
				f = z.str_value;
				f = f.substr(1, f.rfind('"') - 1);
			}
			else
				return ret_femv;
		}

		RegexExpression re;
		if (!re.compile(p, f))
			ret_femv.bool_value = GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::false_value;
		else
			ret_femv.bool_value = re.match(t);

		return ret_femv;
	}

	if (filter.oper_type == QueryTree::GroupPattern::FilterTreeNode::Builtin_lang_type)
	{
		FilterEvaluationMultitypeValue x;

		if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
			getFilterString(filter.child[0], x, row, stringindex);
		else if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			x = matchFilterTree(filter.child[0].node, filter_exists_grouppattern_resultset_record, row, stringindex);

		if (x.datatype == GeneralEvaluation::FilterEvaluationMultitypeValue::simple_literal)
		{
			ret_femv.datatype = GeneralEvaluation::FilterEvaluationMultitypeValue::simple_literal;

			int p = x.str_value.rfind('@');
			if (p != -1)
				ret_femv.str_value = "\"" + x.str_value.substr(p + 1) + "\"";
			else
				ret_femv.str_value = "";

			return ret_femv;
		}
		else
			return ret_femv;
	}

	if (filter.oper_type == QueryTree::GroupPattern::FilterTreeNode::Builtin_langmatches_type)
	{
		FilterEvaluationMultitypeValue x, y;

		if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
			getFilterString(filter.child[0], x, row, stringindex);
		else if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			x = matchFilterTree(filter.child[0].node, filter_exists_grouppattern_resultset_record, row, stringindex);
		if (x.datatype != GeneralEvaluation::FilterEvaluationMultitypeValue::simple_literal)
			return ret_femv;

		if (filter.child[1].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
			getFilterString(filter.child[1], y, row, stringindex);
		else if (filter.child[1].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			y = matchFilterTree(filter.child[1].node, filter_exists_grouppattern_resultset_record, row, stringindex);
		if (y.datatype != GeneralEvaluation::FilterEvaluationMultitypeValue::simple_literal)
			return ret_femv;

		ret_femv.bool_value = ((x.str_value == y.str_value) || (x.str_value.length() > 0 && y.str_value == "\"*\""));

		return ret_femv;
	}

	if (filter.oper_type == QueryTree::GroupPattern::FilterTreeNode::Builtin_bound_type)
	{
		ret_femv.bool_value = (filter.child[0].pos != -1);

		return ret_femv;
	}

	if (filter.oper_type == QueryTree::GroupPattern::FilterTreeNode::Builtin_in_type)
	{
		FilterEvaluationMultitypeValue x;

		if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
			getFilterString(filter.child[0], x, row, stringindex);
		else if (filter.child[0].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			x = matchFilterTree(filter.child[0].node, filter_exists_grouppattern_resultset_record, row, stringindex);

		for (int i = 1; i < (int)filter.child.size(); i++)
		{
			FilterEvaluationMultitypeValue y;

			if (filter.child[i].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::String_type)
				getFilterString(filter.child[i], y, row, stringindex);
			else if (filter.child[i].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
				y = matchFilterTree(filter.child[i].node, filter_exists_grouppattern_resultset_record, row, stringindex);

			FilterEvaluationMultitypeValue equal = (x == y);
			if (i == 1)
				ret_femv = equal;
			else
				ret_femv = (ret_femv || equal);

			if (ret_femv.datatype == GeneralEvaluation::FilterEvaluationMultitypeValue::xsd_boolean && ret_femv.bool_value.value == GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::true_value)
				return ret_femv;
		}

		return ret_femv;
	}

	if (filter.oper_type == QueryTree::GroupPattern::FilterTreeNode::Builtin_exists_type)
	{
		int id = filter.exists_grouppattern_id;
		for (int i = 0; i < (int)filter_exists_grouppattern_resultset_record.resultset[id]->results.size(); i++)
		{
			if (((int)filter_exists_grouppattern_resultset_record.resultset[id]->results[i].res.size() > 0) &&
				((int)filter_exists_grouppattern_resultset_record.common[id][i].varset.size() == 0 ||
				filter_exists_grouppattern_resultset_record.resultset[id]->results[i].findLeftBounder(
					filter_exists_grouppattern_resultset_record.common2resultset[id][i].first, row,
					filter_exists_grouppattern_resultset_record.common2resultset[id][i].second) != -1))
			{
				ret_femv.bool_value = GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::true_value;
				return ret_femv;
			}
		}
		ret_femv.bool_value = GeneralEvaluation::FilterEvaluationMultitypeValue::EffectiveBooleanValue::false_value;
		return ret_femv;
	}

	return ret_femv;
}



void GeneralEvaluation::TempResult::print()
{
	int varnum = (int)this->var.varset.size();

	this->var.print();

	printf("temp result:\n");
	for (int i = 0; i < (int)this->res.size(); i++)
	{
		for (int j = 0; j < varnum; j++)
			printf("%d ", this->res[i][j]);
		printf("\n");
	}
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void GeneralEvaluation::TempResultSet::release()
{
	for (int i = 0; i < (int)this->results.size(); i++)
		results[i].release();
}

int GeneralEvaluation::TempResultSet::findCompatibleResult(Varset &_varset)
{
	for (int i = 0; i < (int)this->results.size(); i++)
		if (results[i].var == _varset)
			return i;

	int p = (int)this->results.size();
	this->results.push_back(TempResult());
	this->results[p].var = _varset;
	return p;
}


void GeneralEvaluation::TempResultSet::doJoin(TempResultSet &x, TempResultSet &r)
{
	long tv_begin = Util::get_cur_time();

	for (int i = 0; i < (int)this->results.size(); i++)
		for (int j = 0; j < (int)x.results.size(); j++)
		{
			Varset var_r = this->results[i].var + x.results[j].var;
			int pos_r = r.findCompatibleResult(var_r);
			this->results[i].doJoin(x.results[j], r.results[pos_r]);
		}

	long tv_end = Util::get_cur_time();
	printf("after doJoin, used %ld ms.\n", tv_end - tv_begin);
}

void GeneralEvaluation::TempResultSet::doUnion(TempResultSet &x, TempResultSet &r)
{
	long tv_begin = Util::get_cur_time();

	for (int i = 0; i < (int)this->results.size(); i++)
	{
		Varset &var_r = this->results[i].var;
		int pos = r.findCompatibleResult(var_r);
		this->results[i].doUnion(r.results[pos]);
	}

	for (int i = 0; i < (int)x.results.size(); i++)
	{
		Varset &var_r = x.results[i].var;
		int pos = r.findCompatibleResult(var_r);
		x.results[i].doUnion(r.results[pos]);
	}

	long tv_end = Util::get_cur_time();
	//printf("after doUnion, used %ld ms.\n", tv_end - tv_begin);
}

void GeneralEvaluation::TempResultSet::doOptional(TempResultSet &x, TempResultSet &r)
{
	long tv_begin = Util::get_cur_time();

	for (int i = 0; i < (int)this->results.size(); i++)
	{
		vector <bool> binding((int)this->results[i].res.size(), false);
		for (int j = 0; j < (int)x.results.size(); j++)
		{
			Varset &var_rn = this->results[i].var;
			Varset var_ra = this->results[i].var + x.results[j].var;
			int pos_rn = r.findCompatibleResult(var_rn);
			int pos_ra = r.findCompatibleResult(var_ra);
			this->results[i].doOptional(binding, x.results[j], r.results[pos_rn], r.results[pos_ra], j + 1 == (int)x.results.size());
		}
	}

	long tv_end = Util::get_cur_time();
	printf("after doOptional, used %ld ms.\n", tv_end - tv_begin);
}

void GeneralEvaluation::TempResultSet::doMinus(TempResultSet &x, TempResultSet &r)
{
	long tv_begin = Util::get_cur_time();

	for (int i = 0; i < (int)this->results.size(); i++)
	{
		vector <TempResult> tr;
		for (int j = 0; j < (int)x.results.size(); j++)
		{
			if (j == 0 && j + 1 == (int)x.results.size())
			{
				int pos_r = r.findCompatibleResult(this->results[i].var);
				this->results[i].doMinus(x.results[j], r.results[pos_r]);
			}
			else if (j == 0)
			{
				tr.push_back(TempResult());
				tr[0].var = this->results[i].var;
				this->results[i].doMinus(x.results[j], tr[0]);
			}
			else if (j + 1 == (int)x.results.size())
			{
				int pos_r = r.findCompatibleResult(this->results[i].var);
				tr[j - 1].doMinus(x.results[j], r.results[pos_r]);
			}
			else
			{
				tr.push_back(TempResult());
				tr[j].var = this->results[i].var;
				tr[j - 1].doMinus(x.results[j], tr[j]);
			}
		}
	}

	long tv_end = Util::get_cur_time();
	printf("after doMinus, used %ld ms.\n", tv_end - tv_begin);
}

void GeneralEvaluation::TempResultSet::doDistinct(Varset &projection, TempResultSet &r)
{
	long tv_begin = Util::get_cur_time();

	TempResultSet *temp = new TempResultSet();
	temp->findCompatibleResult(projection);
	r.findCompatibleResult(projection);

	int projection_varnum = (int)projection.varset.size();
	for (int i = 0; i < (int)this->results.size(); i++)
	{
		vector<int> projection2this = projection.mapTo(this->results[i].var);
		for (int j = 0; j < (int)this->results[i].res.size(); j++)
		{
			int *a = new int[projection_varnum];
			for (int k = 0; k < projection_varnum; k++)
				if (projection2this[k] == -1)	a[k] = -1;
				else	a[k] = this->results[i].res[j][projection2this[k]];
			temp->results[0].res.push_back(a);
		}
	}

	temp->results[0].doDistinct(r.results[0]);

	temp->release();
	delete temp;

	long tv_end = Util::get_cur_time();
	printf("after doDistinct, used %ld ms.\n", tv_end - tv_begin);
}

void GeneralEvaluation::TempResultSet::doFilter(QueryTree::GroupPattern::FilterTreeNode &filter, FilterExistsGroupPatternResultSetRecord &filter_exists_grouppattern_resultset_record, TempResultSet &r, StringIndex *stringindex, Varset &entity_literal_varset)
{
	long tv_begin = Util::get_cur_time();

	for (int i = 0; i < (int)this->results.size(); i++)
	{
		filter_exists_grouppattern_resultset_record.common.clear();
		filter_exists_grouppattern_resultset_record.common2resultset.clear();

		Varset &var_r = this->results[i].var;

		for (int j = 0; j < (int)filter_exists_grouppattern_resultset_record.resultset.size(); j++)
		{
			filter_exists_grouppattern_resultset_record.common.push_back(vector<Varset>());
			filter_exists_grouppattern_resultset_record.common2resultset.push_back(vector< pair< vector<int>, vector<int> > >());

			for (int k = 0; k < (int)filter_exists_grouppattern_resultset_record.resultset[j]->results.size(); k++)
			{
				TempResult &filter_result = filter_exists_grouppattern_resultset_record.resultset[j]->results[k];

				Varset common = var_r * filter_result.var;
				filter_exists_grouppattern_resultset_record.common[j].push_back(common);
				vector<int> common2filter_result = common.mapTo(filter_result.var);
				filter_exists_grouppattern_resultset_record.common2resultset[j].push_back(make_pair(common2filter_result, common.mapTo(var_r)));

				if (common.varset.size() > 0 && filter_result.res.size() > 0)
					filter_result.sort(0, filter_result.res.size() - 1, common2filter_result);
			}
		}

		int pos_r = r.findCompatibleResult(var_r);
		this->results[i].doFilter(filter, filter_exists_grouppattern_resultset_record, r.results[pos_r], stringindex, entity_literal_varset);
	}

	long tv_end = Util::get_cur_time();
	printf("after doFilter, used %ld ms.\n", tv_end - tv_begin);
}


void GeneralEvaluation::TempResultSet::print()
{
	printf("total temp result : %d\n", (int)this->results.size());
	for (int i = 0; i < (int)this->results.size(); i++)
	{
		printf("temp result no.%d\n", i);
		this->results[i].print();
	}
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void GeneralEvaluation::generateEvaluationPlan(QueryTree::GroupPattern &grouppattern)
{
	if ((int)grouppattern.patterns.size() == 0 && (int)grouppattern.unions.size() == 0 && (int)grouppattern.optionals.size() == 0)
	{
		TempResultSet *temp = new TempResultSet();
		temp->results.push_back(TempResult());
		this->semantic_evaluation_plan.push_back(EvaluationUnit('r', temp));

		return;
	}

	int current_pattern = 0, current_unions = 0;
	Varset current_result_varset;

	grouppattern.addOneOptionalOrMinus(' ');	//for convenience

	for (int i = 0; i < (int)grouppattern.optionals.size(); i++)
	{
		if (current_pattern <= grouppattern.optionals[i].lastpattern || current_unions <= grouppattern.optionals[i].lastunions)
		{
			vector <pair<char, int> > node_info;
			vector <Varset> node_varset;
			vector <vector<int> > edge;

			if ((int)current_result_varset.varset.size() > 0)
			{
				node_info.push_back(make_pair('r', -1));
				node_varset.push_back(current_result_varset);
			}

			while (current_pattern <= grouppattern.optionals[i].lastpattern)
			{
				int current_blockid = grouppattern.pattern_blockid[current_pattern];
				if (current_blockid != -1)
				{
					bool found = false;
					for (int i = 0; i < (int)node_info.size(); i++)
						if (node_info[i].second == current_blockid)
						{
							found = true;
							break;
						}
					if (!found)
					{
						node_info.push_back(make_pair('b', current_blockid));
						node_varset.push_back(this->sparql_query_varset[current_blockid]);
					}
				}
				current_pattern ++;
			}

			while (current_unions <= grouppattern.optionals[i].lastunions)
			{
				node_info.push_back(make_pair('u', current_unions));
				Varset varset = grouppattern.unions[current_unions].grouppattern_vec[0].grouppattern_resultset_minimal_varset;
				for (int j = 1; j < (int)grouppattern.unions[current_unions].grouppattern_vec.size(); j++)
					varset = varset * grouppattern.unions[current_unions].grouppattern_vec[j].grouppattern_resultset_minimal_varset;
				node_varset.push_back(varset);
				current_unions ++;
			}

			for (int i = 0; i < (int)node_varset.size(); i++)
			{
				edge.push_back(vector<int>());
				for (int j = 0; j < (int)node_varset.size(); j++)
					if (i != j && node_varset[i].hasCommonVar(node_varset[j]))
						edge[i].push_back(j);
			}

			for (int i = 0; i < (int)node_info.size(); i++)
				if (node_info[i].first != 'v')					//visited
				{
					dfsJoinableResultGraph(i, node_info, edge, grouppattern);
					if (i != 0)
						this->semantic_evaluation_plan.push_back(EvaluationUnit('j'));
				}

			for (int i = 0; i < (int)node_varset.size(); i++)
				current_result_varset = current_result_varset + node_varset[i];
		}

		if (i + 1 != (int)grouppattern.optionals.size())
		{
			generateEvaluationPlan(grouppattern.optionals[i].grouppattern);
			if (i != 0 || grouppattern.optionals[i].lastpattern != -1 || grouppattern.optionals[i].lastunions != -1)
				this->semantic_evaluation_plan.push_back(EvaluationUnit(grouppattern.optionals[i].type));
		}
	}

	grouppattern.optionals.pop_back();

	for (int i = 0; i < (int)grouppattern.filters.size(); i++)
	{
		for (int j = 0; j < (int)grouppattern.filter_exists_grouppatterns[i].size(); j++)
			generateEvaluationPlan(grouppattern.filter_exists_grouppatterns[i][j]);

		this->semantic_evaluation_plan.push_back(EvaluationUnit('f', &grouppattern.filters[i]));
	}
}


void GeneralEvaluation::dfsJoinableResultGraph(int x, vector < pair<char, int> > &node_info, vector < vector<int> > &edge, QueryTree::GroupPattern &grouppattern)
{
	if (node_info[x].first == 'b')
	{
		int blockid = node_info[x].second;

		TempResultSet *temp = new TempResultSet();
		temp->results.push_back(TempResult());
		temp->results[0].var = this->sparql_query_varset[blockid];

		int varnum = (int)temp->results[0].var.varset.size();

		vector<int*> &basicquery_result =this->sparql_query.getBasicQuery(blockid).getResultList();
		int basicquery_result_num = (int)basicquery_result.size();

		temp->results[0].res.reserve(basicquery_result_num);
		for (int i = 0; i < basicquery_result_num; i++)
		{
			int *result_vec = new int [varnum];
			memcpy(result_vec, basicquery_result[i], sizeof(int) * varnum);
			temp->results[0].res.push_back(result_vec);
		}
		this->semantic_evaluation_plan.push_back(EvaluationUnit('r', temp));
	}
	if (node_info[x].first == 'u')
	{
		int unionsid = node_info[x].second;
		for (int i = 0; i < (int)grouppattern.unions[unionsid].grouppattern_vec.size(); i++)
		{
			generateEvaluationPlan(grouppattern.unions[unionsid].grouppattern_vec[i]);
			if (i > 0)	this->semantic_evaluation_plan.push_back(EvaluationUnit('u'));
		}
	}

	node_info[x].first = 'v';

	for (int i = 0; i < (int)edge[x].size(); i++)
	{
		int y = edge[x][i];
		if (node_info[y].first != 'v')
		{
			dfsJoinableResultGraph(y, node_info, edge, grouppattern);
			this->semantic_evaluation_plan.push_back(EvaluationUnit('j'));
		}
	}
}

int GeneralEvaluation::countFilterExistsGroupPattern(QueryTree::GroupPattern::FilterTreeNode &filter)
{
	int count = 0;
	if (filter.oper_type == QueryTree::GroupPattern::FilterTreeNode::Builtin_exists_type)
		count = 1;
	for (int i = 0; i < (int)filter.child.size(); i++)
		if (filter.child[i].node_type == QueryTree::GroupPattern::FilterTreeNode::FilterTreeChild::Tree_type)
			count += countFilterExistsGroupPattern(filter.child[i].node);
	return count;
}

void GeneralEvaluation::doEvaluationPlan()
{
	for (int i = 0; i < (int)this->semantic_evaluation_plan.size(); i++)
	{
		if (semantic_evaluation_plan[i].getType() == 'r')
			this->semantic_evaluation_result_stack.push((TempResultSet*)semantic_evaluation_plan[i].getPointer());
		if (semantic_evaluation_plan[i].getType() == 'j' || semantic_evaluation_plan[i].getType() == 'o' || semantic_evaluation_plan[i].getType() == 'm' || semantic_evaluation_plan[i].getType() == 'u')
		{
			TempResultSet *b = semantic_evaluation_result_stack.top();
			semantic_evaluation_result_stack.pop();
			TempResultSet *a = semantic_evaluation_result_stack.top();
			semantic_evaluation_result_stack.pop();
			TempResultSet *r = new TempResultSet();

			if (semantic_evaluation_plan[i].getType() == 'j')
				a->doJoin(*b, *r);
			if (semantic_evaluation_plan[i].getType() == 'o')
				a->doOptional(*b, *r);
			if (semantic_evaluation_plan[i].getType() == 'm')
				a->doMinus(*b, *r);
			if (semantic_evaluation_plan[i].getType() == 'u')
				a->doUnion(*b, *r);

			a->release();
			b->release();
			delete a;
			delete b;

			semantic_evaluation_result_stack.push(r);
		}

		if (semantic_evaluation_plan[i].getType() == 'f')
		{
			int filter_exists_grouppattern_size = countFilterExistsGroupPattern(*(QueryTree::GroupPattern::FilterTreeNode *)semantic_evaluation_plan[i].getPointer());

			if (filter_exists_grouppattern_size > 0)
				for (int i = 0; i < filter_exists_grouppattern_size; i++)
				{
					this->filter_exists_grouppattern_resultset_record.resultset.push_back(semantic_evaluation_result_stack.top());
					semantic_evaluation_result_stack.pop();
				}

			TempResultSet *a = semantic_evaluation_result_stack.top();
			semantic_evaluation_result_stack.pop();

			TempResultSet *r = new TempResultSet();

			a->doFilter(*(QueryTree::GroupPattern::FilterTreeNode*)semantic_evaluation_plan[i].getPointer(), this->filter_exists_grouppattern_resultset_record, *r, this->stringindex, this->query_tree.getGroupPattern().grouppattern_subject_object_maximal_varset);

			if (filter_exists_grouppattern_size > 0)
			{
				for (int i = 0; i < filter_exists_grouppattern_size; i++)
					this->filter_exists_grouppattern_resultset_record.resultset[i]->release();
				this->filter_exists_grouppattern_resultset_record.resultset.clear();
			}

			a->release();
			delete a;

			semantic_evaluation_result_stack.push(r);
		}
	}
}

bool GeneralEvaluation::expanseFirstOuterUnionGroupPattern(QueryTree::GroupPattern &grouppattern, deque<QueryTree::GroupPattern> &queue)
{
	if (grouppattern.unions.size() > 0)
	{
		QueryTree::GroupPattern copy = grouppattern;

		for (int union_id = 0; union_id < (int)copy.unions[0].grouppattern_vec.size(); union_id++)
		{
			grouppattern = QueryTree::GroupPattern();

			int lastpattern = -1, lastunions = -1, lastoptional = -1;
			while (lastpattern + 1 < (int)copy.patterns.size() || lastunions + 1 < (int)copy.unions.size() || lastoptional + 1 < (int)copy.optionals.size())
			{
				if (lastoptional + 1 < (int)copy.optionals.size() && copy.optionals[lastoptional + 1].lastpattern == lastpattern && copy.optionals[lastoptional + 1].lastunions == lastunions)
				//optional
				{
					grouppattern.addOneOptionalOrMinus('o');
					grouppattern.getLastOptionalOrMinus() = copy.optionals[lastoptional + 1].grouppattern;
					lastoptional++;
				}
				else if (lastunions + 1 < (int)copy.unions.size() && copy.unions[lastunions + 1].lastpattern == lastpattern)
				//union
				{
					//first union
					if (lastunions == -1)
					{
						QueryTree::GroupPattern &inner_grouppattern = copy.unions[0].grouppattern_vec[union_id];

						int inner_lastpattern = -1, inner_lastunions = -1, inner_lastoptional = -1;
						while (inner_lastpattern + 1 < (int)inner_grouppattern.patterns.size() || inner_lastunions + 1 < (int)inner_grouppattern.unions.size() || inner_lastoptional + 1 < (int)inner_grouppattern.optionals.size())
						{
							if (inner_lastoptional + 1 < (int)inner_grouppattern.optionals.size() && inner_grouppattern.optionals[inner_lastoptional + 1].lastpattern == inner_lastpattern && inner_grouppattern.optionals[inner_lastoptional + 1].lastunions == inner_lastunions)
							//inner optional
							{
								grouppattern.addOneOptionalOrMinus('o');
								grouppattern.getLastOptionalOrMinus() = inner_grouppattern.optionals[inner_lastoptional + 1].grouppattern;
								inner_lastoptional++;
							}
							else if (inner_lastunions + 1 < (int)inner_grouppattern.unions.size() && inner_grouppattern.unions[inner_lastunions + 1].lastpattern == inner_lastpattern)
							//inner union
							{
								grouppattern.addOneGroupUnion();
								for (int i = 0; i < (int)inner_grouppattern.unions[inner_lastunions + 1].grouppattern_vec.size(); i++)
								{
									grouppattern.addOneUnion();
									grouppattern.getLastUnion() = inner_grouppattern.unions[inner_lastunions + 1].grouppattern_vec[i];
								}
								inner_lastunions++;
							}
							else
							//inner triple pattern
							{
								grouppattern.addOnePattern(inner_grouppattern.patterns[inner_lastpattern + 1]);
								inner_lastpattern++;
							}
						}
						//inner filter
						for (int i = 0; i < (int)inner_grouppattern.filters.size(); i++)
						{
							grouppattern.addOneFilterTree();
							grouppattern.getLastFilterTree() = inner_grouppattern.filters[i].root;
						}
					}
					else
					{
						grouppattern.addOneGroupUnion();
						for (int i = 0; i < (int)copy.unions[lastunions + 1].grouppattern_vec.size(); i++)
						{
							grouppattern.addOneUnion();
							grouppattern.getLastUnion() = copy.unions[lastunions + 1].grouppattern_vec[i];
						}
					}
					lastunions++;
				}
				else
				//triple pattern
				{
					grouppattern.addOnePattern(copy.patterns[lastpattern + 1]);
					lastpattern++;
				}
			}
			//filter
			for (int i = 0; i < (int)copy.filters.size(); i++)
			{
				grouppattern.addOneFilterTree();
				grouppattern.getLastFilterTree() = copy.filters[i].root;
			}

			queue.push_back(grouppattern);
		}

		grouppattern = copy;
		return true;
	}

	return false;
}

bool GeneralEvaluation::checkExpantionRewritingConnectivity(int dep)
{
	if (dep == 0)
	{
		this->expansion_evaluation_stack.clear();
		//avoid copying stack
		this->expansion_evaluation_stack.reserve(100);
		this->expansion_evaluation_stack.push_back(ExpansionEvaluationStackUnit());
		this->expansion_evaluation_stack[0].grouppattern = this->query_tree.getGroupPattern();
	}

	deque<QueryTree::GroupPattern> queue;
	queue.push_back(this->expansion_evaluation_stack[dep].grouppattern);
	vector<QueryTree::GroupPattern> cand;

	while (!queue.empty())
	{
		QueryTree::GroupPattern front = queue.front();
		queue.pop_front();

		if (!this->expanseFirstOuterUnionGroupPattern(front, queue))
			cand.push_back(front);
	}

	for (int cand_id = 0; cand_id < (int)cand.size(); cand_id++)
	{
		this->expansion_evaluation_stack[dep].grouppattern = cand[cand_id];
		QueryTree::GroupPattern &grouppattern = this->expansion_evaluation_stack[dep].grouppattern;

		Varset varset;
		for (int i = 0; i < (int)grouppattern.patterns.size(); i++)
			varset = varset + grouppattern.patterns[i].varset;

		for (int i = 0; i < dep; i++)
		{
			QueryTree::GroupPattern &upper_grouppattern = this->expansion_evaluation_stack[i].grouppattern;
			for (int j = 0; j < (int)upper_grouppattern.patterns.size(); j++)
				if (varset.hasCommonVar(upper_grouppattern.patterns[j].varset))
					grouppattern.addOnePattern(upper_grouppattern.patterns[j]);
		}

		//check connectivity
		{
			grouppattern.initPatternBlockid();

			for (int i = 0; i < (int)grouppattern.patterns.size(); i++)
			{
				for (int j = 0; j < i; j++)
					if (grouppattern.patterns[i].varset.hasCommonVar(grouppattern.patterns[j].varset))
						grouppattern.mergePatternBlockID(i, j);
			}

			int first_block_id = -1;
			for (int i = 0; i < (int)grouppattern.patterns.size(); i++)
			{
				int root_id = grouppattern.getRootPatternBlockID(i);
				if (first_block_id == -1)
					first_block_id = root_id;
				else
				{
					if (first_block_id != root_id)
						return false;
				}
			}
		}

		for (int i = 0; i < (int)grouppattern.optionals.size(); i++)
		{
			if ((int)this->expansion_evaluation_stack.size() > dep + 1)
				this->expansion_evaluation_stack[dep + 1] = ExpansionEvaluationStackUnit();
			else
				this->expansion_evaluation_stack.push_back(ExpansionEvaluationStackUnit());

			this->expansion_evaluation_stack[dep + 1].grouppattern = grouppattern.optionals[i].grouppattern;
			if (!checkExpantionRewritingConnectivity(dep + 1))
				return false;
		}
	}
	return true;
}

void GeneralEvaluation::queryRewriteEncodeRetrieveJoin(int dep)
{
	if (dep == 0)
	{
		this->expansion_evaluation_stack.clear();
		//avoid copying stack
		this->expansion_evaluation_stack.reserve(100);
		this->expansion_evaluation_stack.push_back(ExpansionEvaluationStackUnit());
		this->expansion_evaluation_stack[0].grouppattern = this->query_tree.getGroupPattern();
	}

	deque<QueryTree::GroupPattern> queue;
	queue.push_back(this->expansion_evaluation_stack[dep].grouppattern);
	vector<QueryTree::GroupPattern> cand;

	while (!queue.empty())
	{
		QueryTree::GroupPattern front = queue.front();
		queue.pop_front();

		if (!this->expanseFirstOuterUnionGroupPattern(front, queue))
			cand.push_back(front);
	}

	this->expansion_evaluation_stack[dep].result = new TempResultSet();
	for (int cand_id = 0; cand_id < (int)cand.size(); cand_id++)
	{
		this->expansion_evaluation_stack[dep].grouppattern = cand[cand_id];
		QueryTree::GroupPattern &grouppattern = this->expansion_evaluation_stack[dep].grouppattern;
		grouppattern.getVarset();

		for (int j = 0; j < 80; j++)			printf("=");	printf("\n");
		grouppattern.print(dep);
		for (int j = 0; j < 80; j++)			printf("=");	printf("\n");

		TempResultSet *temp = new TempResultSet();
		//get the result of grouppattern
		{
			this->expansion_evaluation_stack[dep].sparql_query = SPARQLquery();
			this->expansion_evaluation_stack[dep].sparql_query.addBasicQuery();

			Varset varset;
			for (int i = 0; i < (int)grouppattern.patterns.size(); i++)
			{
				this->expansion_evaluation_stack[dep].sparql_query.addTriple(Triple(
																		grouppattern.patterns[i].subject.value,
																		grouppattern.patterns[i].predicate.value,
																		grouppattern.patterns[i].object.value
																	));
				varset = varset + grouppattern.patterns[i].varset;
			}

			for (int i = 0; i < dep; i++)
			{
				QueryTree::GroupPattern &upper_grouppattern = this->expansion_evaluation_stack[i].grouppattern;
				for (int j = 0; j < (int)upper_grouppattern.patterns.size(); j++)
					if (varset.hasCommonVar(upper_grouppattern.patterns[j].varset))
					{
						this->expansion_evaluation_stack[dep].sparql_query.addTriple(Triple(
																				upper_grouppattern.patterns[j].subject.value,
																				upper_grouppattern.patterns[j].predicate.value,
																				upper_grouppattern.patterns[j].object.value
																			));
					}
			}

			//reduce return result vars
			if (!this->query_tree.checkProjectionAsterisk())
			{
				Varset useful = this->query_tree.getProjection().varset;

				for (int i = 0; i < dep; i++)
				{
					QueryTree::GroupPattern &upper_grouppattern = this->expansion_evaluation_stack[i].grouppattern;
					for (int j = 0; j < (int)upper_grouppattern.patterns.size(); j++)
						useful = useful + upper_grouppattern.patterns[j].varset;
					for (int j = 0; j < (int)upper_grouppattern.filters.size(); j++)
						useful = useful + upper_grouppattern.filters[j].varset;
				}

				for (int i = 0; i < (int)grouppattern.optionals.size(); i++)
					useful = useful + grouppattern.optionals[i].grouppattern.grouppattern_resultset_maximal_varset;

				for (int i = 0; i < (int)grouppattern.filters.size(); i++)
					useful = useful + grouppattern.filters[i].varset;

				varset = varset * useful;
			}

			printf("select vars : ");
			varset.print();

			this->expansion_evaluation_stack[dep].sparql_query.encodeQuery(this->kvstore, vector<vector<string> >(1, varset.varset));
			long tv_encode = Util::get_cur_time();

			if (dep > 0)
				this->strategy.handle(this->expansion_evaluation_stack[dep].sparql_query, &this->result_filter);
			else
				this->strategy.handle(this->expansion_evaluation_stack[dep].sparql_query);
			long tv_handle = Util::get_cur_time();
			//cout << "after Handle, used " << (tv_handle - tv_encode) << "ms." << endl;

			temp->results.push_back(TempResult());
			temp->results[0].var = varset;

			int varnum = (int)varset.varset.size();

			vector<int*> &basicquery_result = this->expansion_evaluation_stack[dep].sparql_query.getBasicQuery(0).getResultList();
			int basicquery_result_num = (int)basicquery_result.size();

			temp->results[0].res.reserve(basicquery_result_num);
			for (int i = 0; i < basicquery_result_num; i++)
			{
				int *result_vec = new int [varnum];
				memcpy(result_vec, basicquery_result[i], sizeof(int) * varnum);
				temp->results[0].res.push_back(result_vec);
			}
		}

		for (int i = 0; i < (int)grouppattern.filters.size(); i++)
		if (!grouppattern.filters[i].done && grouppattern.filters[i].varset.belongTo(grouppattern.grouppattern_resultset_minimal_varset))
		//var in Filter and Optional but not in Pattern
		{
			grouppattern.filters[i].done = true;

			TempResultSet *r = new TempResultSet();
			temp->doFilter(grouppattern.filters[i].root, this->filter_exists_grouppattern_resultset_record, *r, this->stringindex, grouppattern.grouppattern_subject_object_maximal_varset);

			temp->release();
			delete temp;

			temp = r;
		}

		if (temp->results[0].res.size() > 0 && grouppattern.optionals.size() > 0)
		{
			this->result_filter.changeResultHashTable(this->expansion_evaluation_stack[dep].sparql_query, 1);

			for (int i = 0; i < (int)grouppattern.optionals.size(); i++)
			{
				if ((int)this->expansion_evaluation_stack.size() > dep + 1)
					this->expansion_evaluation_stack[dep + 1] = ExpansionEvaluationStackUnit();
				else
					this->expansion_evaluation_stack.push_back(ExpansionEvaluationStackUnit());

				this->expansion_evaluation_stack[dep + 1].grouppattern = grouppattern.optionals[i].grouppattern;
				queryRewriteEncodeRetrieveJoin(dep + 1);

				TempResultSet *r = new TempResultSet();
				temp->doOptional(*this->expansion_evaluation_stack[dep + 1].result, *r);

				this->expansion_evaluation_stack[dep + 1].result->release();
				temp->release();
				delete this->expansion_evaluation_stack[dep + 1].result;
				delete temp;

				temp = r;
			}

			this->result_filter.changeResultHashTable(this->expansion_evaluation_stack[dep].sparql_query, -1);
		}

		TempResultSet *r = new TempResultSet();
		this->expansion_evaluation_stack[dep].result->doUnion(*temp, *r);

		this->expansion_evaluation_stack[dep].result->release();
		temp->release();
		delete this->expansion_evaluation_stack[dep].result;
		delete temp;

		this->expansion_evaluation_stack[dep].result = r;

		for (int i = 0; i < (int)grouppattern.filters.size(); i++)
		if (!grouppattern.filters[i].done)
		{
			grouppattern.filters[i].done = true;

			TempResultSet *r = new TempResultSet();
			this->expansion_evaluation_stack[dep].result->doFilter(grouppattern.filters[i].root, this->filter_exists_grouppattern_resultset_record, *r, this->stringindex, grouppattern.grouppattern_subject_object_maximal_varset);

			this->expansion_evaluation_stack[dep].result->release();
			delete this->expansion_evaluation_stack[dep].result;

			this->expansion_evaluation_stack[dep].result = r;
		}
	}
}

void GeneralEvaluation::distributed_queryRewriteEncodeRetrieveJoin(int dep, string& internal_tag_str)
{
	if (dep == 0)
	{
		this->expansion_evaluation_stack.clear();
		//avoid copying stack
		this->expansion_evaluation_stack.reserve(100);
		this->expansion_evaluation_stack.push_back(ExpansionEvaluationStackUnit());
		this->expansion_evaluation_stack[0].grouppattern = this->query_tree.getGroupPattern();
	}

	deque<QueryTree::GroupPattern> queue;
	queue.push_back(this->expansion_evaluation_stack[dep].grouppattern);
	vector<QueryTree::GroupPattern> cand;

	while (!queue.empty())
	{
		QueryTree::GroupPattern front = queue.front();
		queue.pop_front();

		if (!this->expanseFirstOuterUnionGroupPattern(front, queue))
			cand.push_back(front);
	}

	this->expansion_evaluation_stack[dep].result = new TempResultSet();
	for (int cand_id = 0; cand_id < (int)cand.size(); cand_id++)
	{
		this->expansion_evaluation_stack[dep].grouppattern = cand[cand_id];
		QueryTree::GroupPattern &grouppattern = this->expansion_evaluation_stack[dep].grouppattern;
		grouppattern.getVarset();
/*
		for (int j = 0; j < 80; j++)			printf("=");	printf("\n");
		grouppattern.print(dep);
		for (int j = 0; j < 80; j++)			printf("=");	printf("\n");
*/
		TempResultSet *temp = new TempResultSet();
		//get the result of grouppattern
		{
			this->expansion_evaluation_stack[dep].sparql_query = SPARQLquery();
			this->expansion_evaluation_stack[dep].sparql_query.addBasicQuery();

			Varset varset;
			for (int i = 0; i < (int)grouppattern.patterns.size(); i++)
			{
				this->expansion_evaluation_stack[dep].sparql_query.addTriple(Triple(
																		grouppattern.patterns[i].subject.value,
																		grouppattern.patterns[i].predicate.value,
																		grouppattern.patterns[i].object.value
																	));
				varset = varset + grouppattern.patterns[i].varset;
			}

			for (int i = 0; i < dep; i++)
			{
				QueryTree::GroupPattern &upper_grouppattern = this->expansion_evaluation_stack[i].grouppattern;
				for (int j = 0; j < (int)upper_grouppattern.patterns.size(); j++)
					if (varset.hasCommonVar(upper_grouppattern.patterns[j].varset))
					{
						this->expansion_evaluation_stack[dep].sparql_query.addTriple(Triple(
																				upper_grouppattern.patterns[j].subject.value,
																				upper_grouppattern.patterns[j].predicate.value,
																				upper_grouppattern.patterns[j].object.value
																			));
					}
			}

			//reduce return result vars
			if (!this->query_tree.checkProjectionAsterisk())
			{
				Varset useful = this->query_tree.getProjection().varset;

				for (int i = 0; i < dep; i++)
				{
					QueryTree::GroupPattern &upper_grouppattern = this->expansion_evaluation_stack[i].grouppattern;
					for (int j = 0; j < (int)upper_grouppattern.patterns.size(); j++)
						useful = useful + upper_grouppattern.patterns[j].varset;
					for (int j = 0; j < (int)upper_grouppattern.filters.size(); j++)
						useful = useful + upper_grouppattern.filters[j].varset;
				}

				for (int i = 0; i < (int)grouppattern.optionals.size(); i++)
					useful = useful + grouppattern.optionals[i].grouppattern.grouppattern_resultset_maximal_varset;

				for (int i = 0; i < (int)grouppattern.filters.size(); i++)
					useful = useful + grouppattern.filters[i].varset;

				varset = varset * useful;
			}

			//printf("select vars : ");
			//varset.print();

			this->expansion_evaluation_stack[dep].sparql_query.encodeQuery(this->kvstore, vector<vector<string> >(1, varset.varset));
			long tv_encode = Util::get_cur_time();

			if (dep > 0){
				this->strategy.handle(this->expansion_evaluation_stack[dep].sparql_query, internal_tag_str, &this->result_filter);
				//printf("dep >>>>>>>>>>>>>>>>>> 0\n");
			}
			else{
				//printf("dep ================== 0\n");
				this->strategy.handle(this->expansion_evaluation_stack[dep].sparql_query, internal_tag_str);
			}
			long tv_handle = Util::get_cur_time();
			//cout << "after Handle, used " << (tv_handle - tv_encode) << "ms." << endl;

			temp->results.push_back(TempResult());
			temp->results[0].var = varset;

			int varnum = (int)varset.varset.size();

			vector<int*> &basicquery_result = this->expansion_evaluation_stack[dep].sparql_query.getBasicQuery(0).getResultList();
			int basicquery_result_num = (int)basicquery_result.size();

			temp->results[0].res.reserve(basicquery_result_num);
			for (int i = 0; i < basicquery_result_num; i++)
			{
				int *result_vec = new int [varnum];
				memcpy(result_vec, basicquery_result[i], sizeof(int) * varnum);
				temp->results[0].res.push_back(result_vec);
			}
		}

		for (int i = 0; i < (int)grouppattern.filters.size(); i++)
		if (!grouppattern.filters[i].done && grouppattern.filters[i].varset.belongTo(grouppattern.grouppattern_resultset_minimal_varset))
		//var in Filter and Optional but not in Pattern
		{
			grouppattern.filters[i].done = true;

			TempResultSet *r = new TempResultSet();
			temp->doFilter(grouppattern.filters[i].root, this->filter_exists_grouppattern_resultset_record, *r, this->stringindex, grouppattern.grouppattern_subject_object_maximal_varset);

			temp->release();
			delete temp;

			temp = r;
		}

		if (temp->results[0].res.size() > 0 && grouppattern.optionals.size() > 0)
		{
			this->result_filter.changeResultHashTable(this->expansion_evaluation_stack[dep].sparql_query, 1);

			for (int i = 0; i < (int)grouppattern.optionals.size(); i++)
			{
				if ((int)this->expansion_evaluation_stack.size() > dep + 1)
					this->expansion_evaluation_stack[dep + 1] = ExpansionEvaluationStackUnit();
				else
					this->expansion_evaluation_stack.push_back(ExpansionEvaluationStackUnit());

				this->expansion_evaluation_stack[dep + 1].grouppattern = grouppattern.optionals[i].grouppattern;
				queryRewriteEncodeRetrieveJoin(dep + 1);

				TempResultSet *r = new TempResultSet();
				temp->doOptional(*this->expansion_evaluation_stack[dep + 1].result, *r);

				this->expansion_evaluation_stack[dep + 1].result->release();
				temp->release();
				delete this->expansion_evaluation_stack[dep + 1].result;
				delete temp;

				temp = r;
			}

			this->result_filter.changeResultHashTable(this->expansion_evaluation_stack[dep].sparql_query, -1);
		}

		TempResultSet *r = new TempResultSet();
		this->expansion_evaluation_stack[dep].result->doUnion(*temp, *r);

		this->expansion_evaluation_stack[dep].result->release();
		temp->release();
		delete this->expansion_evaluation_stack[dep].result;
		delete temp;

		this->expansion_evaluation_stack[dep].result = r;

		for (int i = 0; i < (int)grouppattern.filters.size(); i++)
		if (!grouppattern.filters[i].done)
		{
			grouppattern.filters[i].done = true;

			TempResultSet *r = new TempResultSet();
			this->expansion_evaluation_stack[dep].result->doFilter(grouppattern.filters[i].root, this->filter_exists_grouppattern_resultset_record, *r, this->stringindex, grouppattern.grouppattern_subject_object_maximal_varset);

			this->expansion_evaluation_stack[dep].result->release();
			delete this->expansion_evaluation_stack[dep].result;

			this->expansion_evaluation_stack[dep].result = r;
		}
	}
}

bool GeneralEvaluation::needOutputAnswer()
{
	return this->need_output_answer;
}

void GeneralEvaluation::setNeedOutputAnswer()
{
	this->need_output_answer = true;
}

void GeneralEvaluation::getFinalResult(ResultSet &result_str)
{
	if (this->semantic_evaluation_result_stack.empty())		return;

	TempResultSet *results_id = this->semantic_evaluation_result_stack.top();
	this->semantic_evaluation_result_stack.pop();

	Varset &proj = this->query_tree.getProjection();

	if (this->query_tree.getQueryForm() == QueryTree::Select_Query)
	{
		if (this->query_tree.checkProjectionAsterisk())
		{
			for (int i = 0 ; i < (int)results_id->results.size(); i++)
				proj = proj + results_id->results[i].var;
		}

		if (this->query_tree.getProjectionModifier() == QueryTree::Modifier_Distinct)
		{
			TempResultSet *results_id_distinct = new TempResultSet();

			results_id->doDistinct(proj, *results_id_distinct);

			results_id->release();
			delete results_id;

			results_id = results_id_distinct;
		}

		int var_num = proj.varset.size();
		result_str.select_var_num = var_num;
		result_str.setVar(proj.varset);

		for (int i = 0; i < (int)results_id->results.size(); i++)
			result_str.ansNum += (int)results_id->results[i].res.size();

#ifdef STREAM_ON
		if ((long)result_str.ansNum * (long)result_str.select_var_num > 10000000 || (int)this->query_tree.getOrder().size() > 0 || this->query_tree.getOffset() != 0 || this->query_tree.getLimit() != -1)
			result_str.setUseStream();
#endif

		if (!result_str.checkUseStream())
		{
			result_str.answer = new string* [result_str.ansNum];
			for (int i = 0; i < result_str.ansNum; i++)
				result_str.answer[i] = NULL;
		}
		else
		{
			vector <int> keys;
			vector <bool> desc;
			for (int i = 0; i < (int)this->query_tree.getOrder().size(); i++)
			{
				int var_id = Varset(this->query_tree.getOrder()[i].var).mapTo(proj)[0];
				if (var_id != -1)
				{
					keys.push_back(var_id);
					desc.push_back(this->query_tree.getOrder()[i].descending);
				}
			}
			result_str.openStream(keys, desc, this->query_tree.getOffset(), this->query_tree.getLimit());
		}

		int current_result = 0;
		for (int i = 0; i < (int)results_id->results.size(); i++)
		{
			vector<int> result_str2id = proj.mapTo(results_id->results[i].var);
			int size = results_id->results[i].res.size();
			for (int j = 0; j < size; ++j)
			{
				if (!result_str.checkUseStream())
				{
					result_str.answer[current_result] = new string[var_num];
				}
				for (int v = 0; v < var_num; ++v)
				{
					int ans_id = -1;
					if (result_str2id[v] != -1)
						ans_id =  results_id->results[i].res[j][result_str2id[v]];

					if (!result_str.checkUseStream())
					{
						result_str.answer[current_result][v] = "";
						if (ans_id != -1)
						{
							if (this->query_tree.getGroupPattern().grouppattern_subject_object_maximal_varset.findVar(proj.varset[v]))
								this->stringindex->addRequest(ans_id, &result_str.answer[current_result][v], true);
							else
								this->stringindex->addRequest(ans_id, &result_str.answer[current_result][v], false);
						}
					}
					else
					{
						string tmp_ans = "";
						if (ans_id != -1)
						{
							if (this->query_tree.getGroupPattern().grouppattern_subject_object_maximal_varset.findVar(proj.varset[v]))
								this->stringindex->randomAccess(ans_id, &tmp_ans, true);
							else
								this->stringindex->randomAccess(ans_id, &tmp_ans, false);
						}

						result_str.writeToStream(tmp_ans);
					}
				}
				current_result++;
			}
		}
		if (!result_str.checkUseStream())
		{
			this->stringindex->trySequenceAccess();
		}
		else
		{
			result_str.resetStream();
		}
	}
	else if (this->query_tree.getQueryForm() == QueryTree::Ask_Query)
	{
		result_str.select_var_num = 1;
		result_str.setVar(vector<string>(1, "__ask_retval"));
		result_str.ansNum = 1;

		if (!result_str.checkUseStream())
		{
			result_str.answer = new string* [result_str.ansNum];
			result_str.answer[0] = new string[result_str.select_var_num];

			result_str.answer[0][0] = "false";
			for (int i = 0; i < (int)results_id->results.size(); i++)
				if ((int)results_id->results[i].res.size() > 0)
					result_str.answer[0][0] = "true";
		}
	}

	results_id->release();
	delete results_id;
}

void GeneralEvaluation::releaseResultStack()
{
	if (this->semantic_evaluation_result_stack.empty())		return;

	TempResultSet *results_id = this->semantic_evaluation_result_stack.top();
	this->semantic_evaluation_result_stack.pop();
	results_id->release();
	delete results_id;
}

void GeneralEvaluation::prepareUpdateTriple(QueryTree::GroupPattern &update_pattern, TripleWithObjType *&update_triple, int &update_triple_num)
{
	update_pattern.getVarset();

	if (update_triple != NULL)
	{
		delete[] update_triple;
		update_triple = NULL;
	}
	update_triple_num = 0;

	if (this->semantic_evaluation_result_stack.empty())	return;
	TempResultSet *results_id = this->semantic_evaluation_result_stack.top();

	for (int i = 0; i < (int)update_pattern.patterns.size(); i++)
		for (int j = 0 ; j < (int)results_id->results.size(); j++)
			if (update_pattern.patterns[i].varset.belongTo(results_id->results[j].var))
				update_triple_num += results_id->results[j].res.size();

	update_triple  = new TripleWithObjType[update_triple_num];

	int update_triple_count = 0;
	for (int i = 0; i < (int)update_pattern.patterns.size(); i++)
		for (int j = 0 ; j < (int)results_id->results.size(); j++)
			if (update_pattern.patterns[i].varset.belongTo(results_id->results[j].var))
			{
				int subject_id = -1, predicate_id = -1, object_id = -1;
				if (update_pattern.patterns[i].subject.value[0] == '?')
					subject_id = Varset(update_pattern.patterns[i].subject.value).mapTo(results_id->results[j].var)[0];
				if (update_pattern.patterns[i].predicate.value[0] == '?')
					predicate_id = Varset(update_pattern.patterns[i].predicate.value).mapTo(results_id->results[j].var)[0];
				if (update_pattern.patterns[i].object.value[0] == '?')
					object_id = Varset(update_pattern.patterns[i].object.value).mapTo(results_id->results[j].var)[0];

				string subject, predicate, object;
				if (subject_id == -1)
					subject = update_pattern.patterns[i].subject.value;
				if (predicate_id == -1)
					predicate =update_pattern.patterns[i].predicate.value;
				if (object_id == -1)
					object = update_pattern.patterns[i].object.value;

				for (int k = 0; k < (int)results_id->results[j].res.size(); k++)
				{
					if (subject_id != -1)
						this->stringindex->randomAccess(results_id->results[j].res[k][subject_id], &subject, true);
					if (predicate_id != -1)
						this->stringindex->randomAccess(results_id->results[j].res[k][predicate_id], &predicate, false);
					if (object_id != -1)
						this->stringindex->randomAccess(results_id->results[j].res[k][object_id], &object, true);

					update_triple[update_triple_count++] = TripleWithObjType(subject, predicate, object);
				}
			}
}