/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a GPLv3+/MPLv2+ license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Policies.h"
#include <iostream>
#include <string.h>
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------


//***************************************************************************
// Rule
//***************************************************************************

//***************************************************************************
// Constructor/Destructor
//***************************************************************************

//---------------------------------------------------------------------------
Rule::Rule(const Rule& r)
{
    if (&r == this) {
        return;
    }

    this->description = r.description;
    this->use_free_text = r.use_free_text;
    this->type = r.type;
    this->field = r.field;
    this->validator = r.validator;
    this->value = r.value;
    this->text = r.text;
}

//***************************************************************************
// Policies
//***************************************************************************

//---------------------------------------------------------------------------

//***************************************************************************
// Constructor/Destructor
//***************************************************************************

//---------------------------------------------------------------------------
Policies::Policies()
{
}

Policies::~Policies()
{
    map<string, vector<Rule *> >::iterator it = rules.begin();
    map<string, vector<Rule *> >::iterator ite = rules.end();

    for (; it != ite; ++it) {
        for (size_t i = 0; i < it->second.size(); ++i)
        {
            delete it->second[i];
        }
    }
}

String Policies::import_schematron(const char* filename)
{
    if (!filename || !strlen(filename)) {
        return String(__T("The schematron file does not exist"));
    }

    xmlDocPtr doc = xmlParseFile(filename);

    rules.clear();
    if (!doc) {
        return String(__T("The schematron file cannot be parsed"));
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root) {
        return String(__T("No root node, leaving"));
    }

    xmlNodePtr child = root->children;
    while (child) {
        find_pattern_node(child);
        child = child->next;
    }
    xmlFreeDoc(doc);
    return String();
}

void Policies::export_schematron(const char* filename)
{
    //TODO: format schematron from policies
}

void Policies::add_new_rule(string& name, Rule& rule)
{
    Rule *r = new Rule(rule);
    rules[name].push_back(r);
}

void Policies::find_pattern_node(xmlNodePtr node)
{
    string def("pattern");
    if (!node || node->type != XML_ELEMENT_NODE ||
        !node->name || def.compare((const char*)node->name))
    {
        return;
    }

    xmlNodePtr next = node->children;
    while (next) {
        string name = (const char*)xmlGetNoNsProp(node, (const unsigned char*)"name");
        find_rule_node(next, name);
        next = next->next;
    }
}

void Policies::find_rule_node(xmlNodePtr node, string pattern_name)
{
    string def("rule");
    if (!node || node->type != XML_ELEMENT_NODE ||
        !node->name || def.compare((const char*)node->name))
    {
        return;
    }

    xmlNodePtr next = node->children;
    while (next) {
        find_assert_node(next, pattern_name);
        //find_report_node(next);
        next = next->next;
    }
}

void Policies::find_assert_node(xmlNodePtr node, string pattern_name)
{
    string def("assert");
    if (!node->name || def.compare((const char*)node->name))
    {
        return;
    }
    string description((const char*)xmlNodeGetContent(node));
    string test((const char*)xmlGetNoNsProp(node, (const unsigned char*)"test"));
    Rule r = create_rule_from_data(description, test);
    add_new_rule(pattern_name, r);
}

Rule Policies::create_rule_from_data(string descr, string data)
{
    Rule r;

    r.description = descr;
    //TODO try to parse
    r.use_free_text = true;
    r.text = data;
    return r;
}

void Policies::dump_rules_to_stdout()
{
    map<string, vector<Rule *> >::iterator it = rules.begin();
    map<string, vector<Rule *> >::iterator ite = rules.end();

    for (size_t num = 0; it != ite; ++it, ++num) {
        cout << "#" << num;
        cout << " ==== Name:" << it->first << endl;
        for (size_t i = 0; i < it->second.size(); ++i)
        {
            Rule *r = it->second[i];

                if (r->use_free_text) {
                    cout << "Rule:" << r->text;
                    cout << " ==== Desc:" << r->description;
                    cout << endl;
                } else {
                    //TODO when parsed
                }
            delete it->second[i];
        }
    }
}
