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
#include "Core.h"
#include "Common/Schema.h"
#include "Database.h"
#include "NoDatabase.h"
#include "SQLLite.h"
#include "Configuration.h"
#include "Json.h"
#include "Common/Schematron.h"
#include "Common/Xslt.h"
#include "Common/JS_Tree.h"
#include "Common/ImplementationReportXsl.h"
#if defined(_WIN32) || defined(WIN32)
#include "Common/ImplementationReportDisplayTextXsl.h"
#else //defined(_WIN32) || defined(WIN32)
#include "Common/ImplementationReportDisplayTextUnicodeXsl.h"
#endif //defined(_WIN32) || defined(WIN32)
#include "Common/ImplementationReportDisplayHtmlXsl.h"
#include "ZenLib/Ztring.h"
#include "ZenLib/File.h"
#include <sstream>
#include <fstream>
#include <sys/stat.h>
//---------------------------------------------------------------------------

#if defined(UNIX)
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif

//---------------------------------------------------------------------------
const MediaInfoNameSpace::Char* MEDIAINFO_TITLE=__T("MediaArea.net/MediaConch");
ZenLib::Ztring Text_Temp;
using namespace ZenLib;
using namespace std;
//---------------------------------------------------------------------------

namespace MediaConch {


//***************************************************************************
// Constructor/Destructor
//***************************************************************************

//---------------------------------------------------------------------------
Core::Core() : policies(this)
{
    MI=new MediaInfoNameSpace::MediaInfoList;

    config = NULL;
    db = NULL;
    scheduler = new Scheduler(this);
    policies.create_values_from_csv();
}

Core::~Core()
{
    if (scheduler)
        delete scheduler;
    if (db)
        delete db;
    delete MI;
    if (config)
        delete config;
}

//***************************************************************************
// Menu
//***************************************************************************

//---------------------------------------------------------------------------
void Core::Menu_Option_Preferences_Inform (const String& Inform)
{
    MI->Option(__T("Inform"), Inform);
}

//---------------------------------------------------------------------------
String Core::Menu_Option_Preferences_Option (const String& Param, const String& Value)
{
    return MI->Option(Param, Value);
}

//***************************************************************************
// General
//***************************************************************************

//---------------------------------------------------------------------------
void Core::load_configuration()
{
    config = new Configuration;
    std::string config_path = get_config_path();
    config->set_path(config_path);
    config->parse();

    long scheduler_max_threads = 1;
    if (scheduler && !config->get("Scheduler_Max_Threads", scheduler_max_threads))
    {
        if (scheduler_max_threads <= 0)
            scheduler_max_threads = 1;
        scheduler->set_max_threads((size_t)scheduler_max_threads);
    }
}

//---------------------------------------------------------------------------
void Core::load_database()
{
#ifdef HAVE_SQLITE
    std::string db_path;
    if (!config || config->get("SQLite_Path", db_path))
        db_path = std::string(".");
    db_path += Path_Separator;

    db = new SQLLite;

    db->set_database_directory(db_path);
    db->init();
#else
    db = new NoDatabase;
    db->init();
#endif
}

//***************************************************************************
// Tools
//***************************************************************************

//---------------------------------------------------------------------------
void Core::Close ()
{
    MI->Close();
}

//---------------------------------------------------------------------------
void Core::Run (std::string file)
{
    // Parsing
    bool registered = false;
    if (file.length())
        open_file(file, registered);
}

//---------------------------------------------------------------------------
int  Core::open_file(const std::string& file, bool& registered)
{
    if (!ZenLib::File::Exists(Ztring().From_UTF8(file)))
        return -1;

    registered = file_is_registered(file);
    if (!registered)
    {
        if (get_db())
            scheduler->add_element_to_queue(file);
        else
            MI->Open(Ztring().From_UTF8(file));
    }
    return 0;
}

//---------------------------------------------------------------------------
bool Core::is_done(const std::string& file, double& percent_done)
{
    return scheduler->element_is_finished(file, percent_done);
}

//---------------------------------------------------------------------------
int Core::get_report(const std::bitset<MediaConchLib::report_Max>& report_set, MediaConchLib::format f,
                     const std::vector<std::string>& files,
                     const std::vector<std::string>& policies_names,
                     const std::vector<std::string>& policies_contents,
                     std::string& report)
{
    if (!policies_names.empty() || !policies_contents.empty())
        return policies_check(f, files, policies_names, policies_contents, report);

    for (size_t i = 0; i < files.size(); ++i)
    {
        switch (f)
        {
            case MediaConchLib::format_Text:
            case MediaConchLib::format_Xml:
            case MediaConchLib::format_MaXml:
                get_reports_output(files[i], f, report_set, report);
                break;
            case MediaConchLib::format_JsTree:
                get_reports_output_JStree(files[i], report_set, report);
                break;
            case MediaConchLib::format_Html:
                get_reports_output_Html(files[i], report_set, report);
                break;
            default:
                return -1;
        }
    }
    return 0;
}

//---------------------------------------------------------------------------
int Core::remove_report(const std::vector<std::string>& files)
{
    if (!get_db())
        return false;

    for (size_t i = 0; i < files.size(); ++i)
        db->remove_report(files[i]);
    return 0;
}

//---------------------------------------------------------------------------
int Core::get_reports_output_Text(const std::string& file,
                                  const std::bitset<MediaConchLib::report_Max>& report_set,
                                  std::string& report)
{
    get_reports_output(file, MediaConchLib::format_Text, report_set, report);
    return 0;
}

//---------------------------------------------------------------------------
int Core::get_reports_output_Xml(const std::string& file,
                                         const std::bitset<MediaConchLib::report_Max>& report_set,
                                         std::string& report)
{
    get_reports_output(file, MediaConchLib::format_Xml, report_set, report);
    return 0;
}

//---------------------------------------------------------------------------
int Core::get_reports_output_JStree(const std::string& file,
                                    const std::bitset<MediaConchLib::report_Max>& report_set,
                                    std::string& report)
{
    JsTree js;
    if (report_set[MediaConchLib::report_MediaInfo])
    {
        std::string ret = get_report_saved(file, MediaConchLib::report_MediaInfo,
                                           MediaConchLib::format_Xml);
        report += js.format_from_inform_XML(ret);
    }

    if (report_set[MediaConchLib::report_MediaTrace])
    {
        std::string ret = get_report_saved(file, MediaConchLib::report_MediaTrace,
                                           MediaConchLib::format_Xml);
        report += js.format_from_trace_XML(ret);
    }

    return 0;
}

//---------------------------------------------------------------------------
int Core::get_reports_output_Html(const std::string& file,
                                  const std::bitset<MediaConchLib::report_Max>& report_set,
                                  std::string& report)
{
    if (report_set[MediaConchLib::report_MediaConch])
    {
        std::string tmp;
        get_implementation_report(file, tmp);

        // Apply an XSLT to have HTML
        std::string memory(implementation_report_display_html_xsl);
        report += transform_with_xslt_memory(tmp, memory);
    }
    return 0;
}

//---------------------------------------------------------------------------
int Core::get_reports_output_Text_Implementation(const std::string& file, std::string& report)
{
    std::string tmp;
    get_implementation_report(file, tmp);

    // Apply an XSLT to have Text
#if defined(_WIN32) || defined(WIN32)
    std::string memory(implementation_report_display_text_xsl);
#else //defined(_WIN32) || defined(WIN32)
    std::string memory(implementation_report_display_textunicode_xsl);
#endif //defined(_WIN32) || defined(WIN32)
    report = transform_with_xslt_memory(tmp, memory);
    return 0;
}

//---------------------------------------------------------------------------
int Core::get_reports_output_Xml_Implementation (const std::string& file, std::string& report)
{
    get_implementation_report(file, report);

    // Apply an XSLT to have XML
    if (xslt_display.length())
        report = transform_with_xslt_file(report, xslt_display);

    return 0;
}

//---------------------------------------------------------------------------
int Core::policies_check_contents(MediaConchLib::format f, const std::string& file,
                                  const std::vector<std::string>& policies_contents,
                                  std::stringstream& Out)
{
    for (size_t i = 0; i < policies_contents.size(); ++i)
    {
        std::string tmp;
        if (!validate_xslt_policy_from_memory(file, policies_contents[i], tmp))
        {
            std::string retain = tmp;
            tmp = std::string();
            if (!validate_schematron_policy_from_memory(file, policies_contents[i], tmp))
                Out << retain;
            else
                Out << tmp;
        }
        else
            Out << tmp;
    }
    return 0;
}

//---------------------------------------------------------------------------
int Core::policies_check_files(MediaConchLib::format f, const std::string& file,
                               const std::vector<std::string>& policies_names,
                               std::stringstream& Out)
{
    Out << file << ": ";
    for (size_t i = 0; i < policies_names.size(); ++i)
    {
        std::string tmp;
        if (is_schematron_file(policies_names[i]))
        {
            if (!validate_schematron_policy_from_file(file, policies_names[i], tmp))
            {
                std::string retain = tmp;
                tmp = std::string();
                if (!validate_xslt_policy_from_file(file, policies_names[i], tmp))
                    Out << retain;
                else
                    Out << tmp;
            }
            else
                Out << tmp;
        }
        else
        {
            if (!validate_xslt_policy_from_file(file, policies_names[i], tmp))
            {
                std::string retain = tmp;
                tmp = std::string();
                if (!validate_schematron_policy_from_file(file, policies_names[i], tmp))
                    Out << retain;
                else
                    Out << tmp;
            }
            else
                Out << tmp;
        }
    }
    return 0;
}

//---------------------------------------------------------------------------
int Core::policies_check(MediaConchLib::format f,
                         const std::vector<std::string>& files,
                         const std::vector<std::string>& policies_names,
                         const std::vector<std::string>& policies_contents,
                         std::string& report)
{
    if (!files.size())
    {
        report = "No file to validate";
        return -1;
    }

    if (!policies_names.size() && !policies_contents.size())
    {
        report = "No policy to apply";
        return -1;
    }

    std::stringstream Out;
    for (size_t i = 0; i < files.size(); ++i)
    {
        if (policies_names.size())
            policies_check_files(f, files[i], policies_names, Out);
        if (policies_contents.size())
            policies_check_contents(f, files[i], policies_contents, Out);
    }
    report = Out.str();
    return 0;
}

//---------------------------------------------------------------------------
bool Core::policy_schematron(const std::string& file, const std::string& policy,
                             MediaConchLib::format, std::stringstream& Out)
{
    Schema* S = new Schematron;
    bool valid = false;

    if (S->register_schema_from_file(policy.c_str()))
    {
        std::string report;
        valid = validation(file, S, report);

        if (!valid)
        {
            Out << "NOT VALID\n";
            Out << report;
        }
        else
            Out << "VALID";
    }
    else
    {
        std::vector<std::string> errors = S->get_errors();
        Out << "internal error for parsing file" << std::endl;
        for (size_t pos = 0; pos < errors.size(); pos++)
            Out << "\t" << errors[pos].c_str();
    }
    delete S;
    return valid;
}

//---------------------------------------------------------------------------
bool Core::policy_xslt(const std::string& file, const std::string& policy, MediaConchLib::format f, std::stringstream& Out)
{
    Schema *S = new Xslt;
    bool valid = false;

    if (S->register_schema_from_file(policy.c_str()))
    {
        std::string report;
        valid = validation(file, S, report);
        if (xslt_display.length())
            report = Core::transform_with_xslt_file(report, xslt_display);
        else 
            switch (f)
            {
                case MediaConchLib::format_Text:
                                    {
                                        // Apply an XSLT to have Text
                                    #if defined(_WIN32) || defined(WIN32)
                                        std::string memory(implementation_report_display_text_xsl);
                                    #else //defined(_WIN32) || defined(WIN32)
                                        std::string memory(implementation_report_display_textunicode_xsl);
                                    #endif //defined(_WIN32) || defined(WIN32)
                                        report = transform_with_xslt_memory(report, memory);
                                    }
                                    break;
                case MediaConchLib::format_Html:
                                    {
                                        // Apply an XSLT to have Text
                                        std::string memory(implementation_report_display_html_xsl);
                                        report = transform_with_xslt_memory(report, memory);
                                    }
                                    break;
                default:            ;
            }

        Out << report;
    }
    else
    {
        std::vector<std::string> errors = S->get_errors();
        Out << file << ": ";
        Out << "internal error for parsing file" << endl;
        for (size_t pos = 0; pos < errors.size(); pos++)
            Out << "\t" << errors[pos].c_str();
    }
    delete S;
    return valid;
}

//***************************************************************************
// API
//***************************************************************************

//---------------------------------------------------------------------------
bool Core::validate_policy(const std::string& file, int policy, std::string& report)
{
    if (policy == -1)
    {
        report = "Policy not found";
        return false;
    }

    if (policies.policies[policy]->type == Policies::POLICY_SCHEMATRON)
        return validate_schematron_policy(file, policy, report);
    return validate_xslt_policy(file, policy, report);
}

//---------------------------------------------------------------------------
bool Core::validate_policy_file(const std::string& file, const std::string& policy, std::string& report)
{
    if (!policy.length())
    {
        report = "Policy invalid";
        return false;
    }

    bool valid = validate_schematron_policy_from_file(file, policy, report);
    if (!valid)
    {
        std::string tmp = report;
        valid = validate_xslt_policy_from_file(file, policy, report);
        if (!valid)
            report = tmp;
    }
    return valid;
}

//---------------------------------------------------------------------------
bool Core::validate_policy_memory(const std::string& file, const std::string& policy, std::string& report)
{
    if (!policy.length())
    {
        report = "Policy invalid";
        return false;
    }

    bool valid = validate_schematron_policy_from_memory(file, policy, report);
    if (!valid)
    {
        std::string tmp = report;
        valid = validate_xslt_policy_from_memory(file, policy, report);
        if (!valid)
            report = tmp;
    }
    return valid;
}

//---------------------------------------------------------------------------
std::string Core::transform_with_xslt_file(const std::string& report, const std::string& xslt)
{
    Schema *S = new Xslt;

    if (!S->register_schema_from_file(xslt.c_str()))
        return report;

    int valid = S->validate_xml(report);
    if (valid < 0)
        return report;

    std::string r = S->get_report();
    delete S;
    return r;
}

//---------------------------------------------------------------------------
std::string Core::transform_with_xslt_memory(const std::string& report, const std::string& memory)
{
    Schema *S = new Xslt;

    if (!S->register_schema_from_memory(memory))
        return report;

    int valid = S->validate_xml(report);
    if (valid < 0)
        return report;
    std::string r = S->get_report();
    delete S;
    return r;
}

//***************************************************************************
// HELPER
//***************************************************************************

//---------------------------------------------------------------------------
bool Core::validate_schematron_policy(const std::string& file, int pos, std::string& report)
{
    std::string policyFile;
    xmlDocPtr doc = NULL;
    Schema *S = new Schematron;
    bool valid = true;

    if (pos >= 0)
        doc = policies.create_doc(pos);

    if (doc && S->register_schema_from_doc(doc))
        valid = validation(file, S, report);
    else
    {
        valid = false;

        std::stringstream Out;
        std::vector<std::string> errors = S->get_errors();

        Out << "internal error for parsing Policy" << endl;
        for (size_t i = 0; i < errors.size(); i++)
            Out << "\t" << errors[i].c_str();
        report = Out.str();
    }
    xmlFreeDoc(doc);
    delete S;
    return valid;
}

//---------------------------------------------------------------------------
bool Core::validate_schematron_policy_from_file(const std::string& file, const std::string& policy, std::string& report)
{
    bool valid = true;
    Schema *S = new Schematron;

    if (S->register_schema_from_file(policy.c_str()))
        valid = validation(file, S, report);
    else
    {
        valid = false;

        std::stringstream Out;
        std::vector<std::string> errors = S->get_errors();

        Out << "internal error for parsing Policy" << endl;
        for (size_t i = 0; i < errors.size(); i++)
            Out << "\t" << errors[i].c_str();
        report = Out.str();
    }
    delete S;
    return valid;
}

//---------------------------------------------------------------------------
bool Core::validate_schematron_policy_from_memory(const std::string& file, const std::string& memory, std::string& report)
{
    bool valid = true;
    Schema *S = new Schematron;

    if (S->register_schema_from_memory(memory))
        valid = validation(file, S, report);
    else
    {
        valid = false;

        std::stringstream Out;
        std::vector<std::string> errors = S->get_errors();

        Out << "internal error for parsing Policy" << endl;
        for (size_t i = 0; i < errors.size(); i++)
            Out << "\t" << errors[i].c_str();
        report = Out.str();
    }
    delete S;
    return valid;
}

//---------------------------------------------------------------------------
bool Core::validate_xslt_policy(const std::string& file, int pos, std::string& report)
{
    bool valid = true;
    std::string policyFile;
    xmlDocPtr doc = NULL;
    Schema *S = new Xslt;

    if (pos >= 0)
        doc = policies.create_doc(pos);

    if (doc && S->register_schema_from_doc(doc))
        valid = validation(file, S, report);
    else
    {
        valid = false;

        std::stringstream Out;
        std::vector<std::string> errors = S->get_errors();

        Out << "internal error for parsing Policy" << endl;
        for (size_t i = 0; i < errors.size(); i++)
            Out << "\t" << errors[i].c_str();
        report = Out.str();
    }
    delete S;
    return valid;
}

//---------------------------------------------------------------------------
bool Core::validate_xslt_policy_from_file(const std::string& file, const std::string& policy, std::string& report)
{
    bool valid = true;
    Schema *S = new Xslt;

    if (S->register_schema_from_file(policy.c_str()))
        valid = validation(file, S, report);
    else
    {
        valid = false;

        std::stringstream Out;
        std::vector<std::string> errors = S->get_errors();

        Out << "internal error for parsing Policy" << endl;
        for (size_t i = 0; i < errors.size(); i++)
            Out << "\t" << errors[i].c_str();
        report = Out.str();
    }
    delete S;
    return valid;
}

//---------------------------------------------------------------------------
bool Core::validate_xslt_policy_from_memory(const std::string& file, const std::string& memory, std::string& report)
{
    bool valid = true;
    Schema *S = new Xslt;

    if (S->register_schema_from_memory(memory))
        valid = validation(file, S, report);
    else
    {
        valid = false;

        std::stringstream Out;
        std::vector<std::string> errors = S->get_errors();

        Out << "internal error for parsing Policy" << endl;
        for (size_t i = 0; i < errors.size(); i++)
            Out << "\t" << errors[i].c_str();
        report = Out.str();
    }
    delete S;
    return valid;
}

//---------------------------------------------------------------------------
bool Core::validation(const std::string& file, Schema* S, std::string& report)
{
    std::string xml = get_report_saved(file, MediaConchLib::report_MediaConch,
                                       MediaConchLib::format_MaXml);
    bool valid = true;

    int ret = S->validate_xml(xml);
    if (ret < 0)
    {
        report = "Validation generated an internal error";
        valid = false;
    }
    else
    {
        report = S->get_report();
        valid = ret == 0 ? true : false;
    }
    return valid;
}

bool Core::is_schematron_file(const std::string& file)
{
    if (file.compare(file.length() - 4, 4, ".sch"))
        return false;

    return true;
}

//---------------------------------------------------------------------------
std::string Core::get_last_modification_file(const std::string& filename)
{
    Ztring time = ZenLib::File::Modified_Get(Ztring().From_UTF8(filename));
    return time.To_UTF8();
}

//---------------------------------------------------------------------------
void Core::register_report_mediainfo_text_to_database(std::string& file, const std::string& time,
                                                      MediaInfoNameSpace::MediaInfoList* curMI)
{
    curMI->Option(__T("Details"), __T("0"));
    curMI->Option(__T("Inform"), String());
    std::string report = Ztring(curMI->Inform()).To_UTF8();
    db->save_report(MediaConchLib::report_MediaInfo, MediaConchLib::format_Text, file, time, report);
}

//---------------------------------------------------------------------------
void Core::register_report_mediainfo_xml_to_database(std::string& file, const std::string& time,
                                                     MediaInfoNameSpace::MediaInfoList* curMI)
{
    curMI->Option(__T("Details"), __T("0"));
    curMI->Option(__T("Inform"), __T("MIXML"));
    std::string report = Ztring(curMI->Inform()).To_UTF8();
    db->save_report(MediaConchLib::report_MediaInfo, MediaConchLib::format_Xml, file, time, report);
}

//---------------------------------------------------------------------------
void Core::register_report_mediatrace_text_to_database(std::string& file, const std::string& time,
                                                       MediaInfoNameSpace::MediaInfoList* curMI)
{
    curMI->Option(__T("Details"), __T("1"));
    curMI->Option(__T("Inform"), String());
    std::string report = Ztring(curMI->Inform()).To_UTF8();
    db->save_report(MediaConchLib::report_MediaTrace, MediaConchLib::format_Text, file, time, report);
}

//---------------------------------------------------------------------------
void Core::register_report_mediatrace_xml_to_database(std::string& file, const std::string& time,
                                                      MediaInfoNameSpace::MediaInfoList* curMI)
{
    curMI->Option(__T("Details"), __T("1"));
    curMI->Option(__T("Inform"), __T("XML"));
    std::string report = Ztring(curMI->Inform()).To_UTF8();
    db->save_report(MediaConchLib::report_MediaTrace, MediaConchLib::format_Xml, file, time, report);
}

//---------------------------------------------------------------------------
void Core::register_report_implementation_xml_to_database(const std::string& file, const std::string& time,
                                                          std::string& report)
{
    db->save_report(MediaConchLib::report_MediaConch, MediaConchLib::format_Xml, file, time, report);
}

//---------------------------------------------------------------------------
void Core::get_content_of_media_in_xml(std::string& report)
{
    std::string media_start("<media ref");
    size_t start = report.find(media_start);

    if (start == std::string::npos)
        return;

    start += media_start.length();
    media_start = std::string(">");
    start = report.find(media_start, start);
    size_t end = report.rfind("</media>");
    if (start == std::string::npos || end == std::string::npos)
        return;

    report = report.substr(start + media_start.length(), end - start - media_start.length());
}

//---------------------------------------------------------------------------
void Core::register_file_to_database(std::string& filename, MediaInfoNameSpace::MediaInfoList* curMI)
{
    const std::string& time = get_last_modification_file(filename);

    // MediaInfo
    register_report_mediainfo_text_to_database(filename, time, curMI);
    register_report_mediainfo_xml_to_database(filename, time, curMI);

    // MediaTrace
    register_report_mediatrace_text_to_database(filename, time, curMI);
    register_report_mediatrace_xml_to_database(filename, time, curMI);
}

//---------------------------------------------------------------------------
void Core::register_file_to_database(std::string& filename)
{
    const std::string& time = get_last_modification_file(filename);

    // MediaInfo
    register_report_mediainfo_text_to_database(filename, time, MI);
    register_report_mediainfo_xml_to_database(filename, time, MI);

    // MediaTrace
    register_report_mediatrace_text_to_database(filename, time, MI);
    register_report_mediatrace_xml_to_database(filename, time, MI);
}

//---------------------------------------------------------------------------
void Core::create_report_mi_xml(const std::string& filename, std::string& report)
{
    report += get_report_saved(filename, MediaConchLib::report_MediaInfo, MediaConchLib::format_Xml);
}

//---------------------------------------------------------------------------
void Core::create_report_mt_xml(const std::string& filename, std::string& report)
{
    report += get_report_saved(filename, MediaConchLib::report_MediaTrace, MediaConchLib::format_Xml);
}

//---------------------------------------------------------------------------
void Core::create_report_ma_xml(const std::string& filename, std::string& report,
                                std::bitset<MediaConchLib::report_Max> reports)
{

    std::string start("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                      "<MediaArea\n"
                      "xmlns=\"https://mediaarea.net/mediaarea\"\n"
                      "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
                      "xsi:schemaLocation=\"https://mediaarea.net/mediaarea https://mediaarea.net/mediaarea/mediaarea_0_1.xsd\"\n"
                      "version=\"0.1\">\n"
                      "<!-- Work in progress, not for production -->\n"
                      "<creatingLibrary version=\"0.7.79\" url=\"https://mediaarea.net/MediaInfo\">MediaInfoLib</creatingLibrary>\n"
                      "<media ref=\"");
    start += filename + "\">\n";
    report += start;

    if (reports[MediaConchLib::report_MediaInfo])
    {
        std::string info = get_report_saved(filename, MediaConchLib::report_MediaInfo, MediaConchLib::format_Xml);
        get_content_of_media_in_xml(info);
        if (info.length())
        {
            info = "<MediaInfo xmlns=\"https://mediaarea.net/mediainfo\" version=\"2.0beta1\">" + info + "</MediaInfo>\n";
            report +=info;
        }
    }

    if (reports[MediaConchLib::report_MediaTrace])
    {
        std::string trace = get_report_saved(filename, MediaConchLib::report_MediaTrace,
                                             MediaConchLib::format_Xml);
        get_content_of_media_in_xml(trace);
        if (trace.length())
        {
            trace = "<MediaTrace xmlns=\"https://mediaarea.net/mediatrace\" version=\"0.1\">"
                + trace + "</MediaTrace>\n";
            report += trace;
        }
    }

    if (reports[MediaConchLib::report_MediaConch])
    {
        std::string implem;
        get_implementation_report(filename, implem);
        get_content_of_media_in_xml(implem);
        if (implem.length())
        {
            implem = "<MediaConch xmlns=\"https://mediaarea.net/mediaconch\" version=\"0.1\">"
            + implem + "</MediaConch>\n";
            report += implem;
        }
    }
    std::string end("</media>\n"
                    "</MediaArea>");
    report += end;
}

//---------------------------------------------------------------------------
std::string Core::get_report_saved(const std::string& filename, MediaConchLib::report reportKind,
                                   MediaConchLib::format f)
{
    if (f == MediaConchLib::format_MaXml)
    {
        std::string report;
        create_report_ma_xml(filename, report, get_bitset_with_mi_mt());
        return report;
    }

    if (reportKind >= MediaConchLib::report_Max || f >= MediaConchLib::format_Max)
        return std::string();

    if (!get_db())
        return std::string();

    std::string time = get_last_modification_file(filename);
    return db->get_report(reportKind, f, filename, time);
}

//---------------------------------------------------------------------------
void Core::get_reports_output(const std::string& file, MediaConchLib::format f,
                              std::bitset<MediaConchLib::report_Max> report_set,
                              std::string& report)
{
    if (f == MediaConchLib::format_MaXml)
    {
        create_report_ma_xml(file, report, report_set);
        report += "\r\n";
    }
    else
    {
        if (report_set[MediaConchLib::report_MediaInfo])
        {
            if (f == MediaConchLib::format_Xml)
                create_report_mi_xml(file, report);
            else
                report += get_report_saved(file, MediaConchLib::report_MediaInfo, f);
            report += "\r\n";
        }
        if (report_set[MediaConchLib::report_MediaTrace])
        {
            if (f == MediaConchLib::format_Xml)
                create_report_mt_xml(file, report);
            else
                report += get_report_saved(file, MediaConchLib::report_MediaTrace, f);
            report += "\r\n";
        }
        if (report_set[MediaConchLib::report_MediaConch])
        {
            std::string tmp;
            if (f == MediaConchLib::format_Xml)
                get_reports_output_Xml_Implementation(file, tmp);
            else
                get_reports_output_Text_Implementation(file, tmp);
            report += tmp;
            report += "\r\n";
        }
    }
}

//---------------------------------------------------------------------------
void Core::get_implementation_report(const std::string& file, std::string& report)
{
    std::string r = get_report_saved(file, MediaConchLib::report_MediaConch,
                                     MediaConchLib::format_Xml);

    if (!r.length())
    {
        bool valid;
        std::string memory(implementation_report_xsl);
        valid = validate_xslt_policy_from_memory(file, memory, r);
        if (valid)
        {
            std::string time = get_last_modification_file(file);
            register_report_implementation_xml_to_database(file, time, r);
        }
    }
    report += r;
}

//---------------------------------------------------------------------------
bool Core::file_is_registered_in_db(const std::string& filename)
{
    if (!get_db())
        return false;

    std::string time = get_last_modification_file(filename);

    return db->file_is_registered(MediaConchLib::report_MediaInfo, MediaConchLib::format_Xml, filename, time);
}

//---------------------------------------------------------------------------
bool Core::file_is_registered_in_queue(const std::string& filename)
{
    if (!scheduler)
        return false;

    return scheduler->element_exists(filename);;
}

//---------------------------------------------------------------------------
bool Core::file_is_registered(const std::string& filename)
{
    if (file_is_registered_in_queue(filename))
        return true;
    return file_is_registered_in_db(filename);
}

//---------------------------------------------------------------------------
std::string Core::get_config_path()
{
    if (configuration_path.length())
        return configuration_path;

    std::string config_path(".");
#if defined(WINDOWS)
    char username[UNLEN+1];
    DWORD username_len = UNLEN+1;
    GetUserName(username, &username_len);

    std::string wuser(username, username_len);
    std::string user_name(wuser.begin(), wuser.end());
    std::stringstream path;

    path << "C:/Users/" << user_name << "/AppData/Local/MediaConch/";
    config_path = path.str();
#elif defined(UNIX)
    const char* home = NULL;

    if ((home = getenv("HOME")) == NULL)
    {
        struct passwd *pw = getpwuid(getuid());
        if (pw)
            home = pw->pw_dir;
    }
    config_path = std::string(home) + Path_Separator + std::string(".config/");
#elif defined(MACOS) || defined(MACOSX)
    config_path = std::string("~/Library/Preferences/");
#endif

    std::ifstream ifile(config_path.c_str());
    if (!ifile)
        config_path = std::string(".") + Path_Separator;
    return config_path;
}

//---------------------------------------------------------------------------
void Core::set_configuration_path(const std::string& path)
{
    configuration_path = path;
}

//---------------------------------------------------------------------------
const std::string& Core::get_configuration_path() const
{
    return configuration_path;
}

//---------------------------------------------------------------------------
bool Core::database_is_enabled() const
{
    if (!config)
        return false;
    bool enabled = false;
    if (config->get("Database_Enabled", enabled))
        return false;
    return enabled;
}

//---------------------------------------------------------------------------
Database *Core::get_db()
{
    if (!db)
        load_database();
    return db;
}

//---------------------------------------------------------------------------
void Core::WaitRunIsFinished()
{
    while (1)
    {
        if (scheduler->is_finished())
            break;
        usleep(50000);
    }
}

//---------------------------------------------------------------------------
std::bitset<MediaConchLib::report_Max> Core::get_bitset_with_mi_mt()
{
    std::bitset<MediaConchLib::report_Max> bits;

    bits.set(MediaConchLib::report_MediaInfo);
    bits.set(MediaConchLib::report_MediaTrace);
    return bits;
}

//---------------------------------------------------------------------------
bool Core::is_using_daemon() const
{
    bool use_daemon = false;
    if (config->get("Use_Daemon", use_daemon) < 0)
        return false;
    return use_daemon;
}

//---------------------------------------------------------------------------
void Core::get_daemon_address(std::string& addr, int& port) const
{
    config->get("Daemon_Address", addr);
    long int port_l;
    if (config->get("Daemon_Port", port_l) >= 0)
        port = (int)port_l;
}

}
