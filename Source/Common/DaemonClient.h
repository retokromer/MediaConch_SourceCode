/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a GPLv3+/MPLv2+ license that can
 *  be found in the License.html file in the root of the source tree.
 */

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// JSON functions
//
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//---------------------------------------------------------------------------
#ifndef DaemonClientH
#define DaemonClientH

//---------------------------------------------------------------------------
#include <string>
#include <map>
#include <bitset>
#include "MediaConchLib.h"

//---------------------------------------------------------------------------
namespace MediaConch
{

class Http;
class Core;

//***************************************************************************
// Class DaemonClient
//***************************************************************************

class DaemonClient
{
public:
    //Constructor/Destructor
    DaemonClient(Core* c);
    ~DaemonClient();

    // General
    int init();
    int close();

    // Analyze
    int analyze(const std::string& file, bool& registered);

    // Status
    bool is_done(const std::string& file, double& done);

    // Report
    int get_report(const std::bitset<MediaConchLib::report_Max>& report_set, MediaConchLib::format f,
                   const std::vector<std::string>& files,
                   const std::vector<std::string>& policies_names,
                   const std::vector<std::string>& policies_contents,
                   std::string& report);
    int validate_policy(const std::string& file, const std::string& policy,
                        std::string& report);

private:
    Core *core;
    Http *http_client;

    DaemonClient(const DaemonClient&);
    DaemonClient& operator=(const DaemonClient&);

    std::map<std::string, int> file_ids;
};

}

#endif // !DaemonClientH
