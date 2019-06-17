/**
 * Copyright (c) 2015, Timothy Stack
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * * Neither the name of Timothy Stack nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <pcrecpp.h>

#include <string>

#include "lnav.hh"
#include "sql_util.hh"
#include "data_parser.hh"
#include "sysclip.hh"
#include "yajlpp_def.hh"
#include "lnav_config.hh"

#include "readline_possibilities.hh"

using namespace std;

static int handle_collation_list(void *ptr,
                                 int ncols,
                                 char **colvalues,
                                 char **colnames)
{
    if (lnav_data.ld_rl_view != nullptr) {
        lnav_data.ld_rl_view->add_possibility(LNM_SQL, "*", colvalues[1]);
    }

    return 0;
}

static int handle_db_list(void *ptr,
                          int ncols,
                          char **colvalues,
                          char **colnames)
{
    if (lnav_data.ld_rl_view != nullptr) {
        lnav_data.ld_rl_view->add_possibility(LNM_SQL, "*", colvalues[1]);
    }

    return 0;
}

static int handle_table_list(void *ptr,
                             int ncols,
                             char **colvalues,
                             char **colnames)
{
    if (lnav_data.ld_rl_view != nullptr) {
        lnav_data.ld_rl_view->add_possibility(LNM_SQL, "*", colvalues[0]);

        lnav_data.ld_table_ddl[colvalues[0]] = colvalues[1];
    }

    return 0;
}

static int handle_table_info(void *ptr,
                             int ncols,
                             char **colvalues,
                             char **colnames)
{
    if (lnav_data.ld_rl_view != nullptr) {
        auto_mem<char, sqlite3_free> quoted_name;

        quoted_name = sql_quote_ident(colvalues[1]);
        lnav_data.ld_rl_view->add_possibility(LNM_SQL, "*",
                                              string(quoted_name));
    }
    if (strcmp(colvalues[5], "1") == 0) {
        lnav_data.ld_db_key_names.emplace_back(colvalues[1]);
    }
    return 0;
}

static int handle_foreign_key_list(void *ptr,
                                   int ncols,
                                   char **colvalues,
                                   char **colnames)
{
    lnav_data.ld_db_key_names.emplace_back(colvalues[3]);
    lnav_data.ld_db_key_names.emplace_back(colvalues[4]);
    return 0;
}

struct sqlite_metadata_callbacks lnav_sql_meta_callbacks = {
        handle_collation_list,
        handle_db_list,
        handle_table_list,
        handle_table_info,
        handle_foreign_key_list,
};

void add_text_possibilities(int context, const string &type, const std::string &str)
{
    static pcrecpp::RE re_escape("([.\\^$*+?()\\[\\]{}\\\\|])");
    static pcrecpp::RE re_escape_no_dot("([\\^$*+?()\\[\\]{}\\\\|])");

    readline_curses *rlc = lnav_data.ld_rl_view;
    pcre_context_static<30> pc;
    data_scanner ds(str);
    data_token_t dt;

    while (ds.tokenize2(pc, dt)) {
        if (pc[0]->length() < 4) {
            continue;
        }

        switch (dt) {
            case DT_DATE:
            case DT_TIME:
            case DT_WHITE:
                continue;
            default:
                break;
        }

        switch (context) {
            case LNM_SEARCH:
            case LNM_COMMAND:
            case LNM_EXEC: {
                string token_value, token_value_no_dot;

                token_value_no_dot = token_value =
                        ds.get_input().get_substr(pc.all());
                re_escape.GlobalReplace("\\\\\\1", &token_value);
                re_escape_no_dot.GlobalReplace("\\\\\\1", &token_value_no_dot);
                rlc->add_possibility(context, type, token_value);
                if (token_value != token_value_no_dot) {
                    rlc->add_possibility(context, type, token_value_no_dot);
                }
                break;
            }
            case LNM_SQL: {
                string token_value = ds.get_input().get_substr(pc.all());
                auto_mem<char, sqlite3_free> quoted_token;

                quoted_token = sqlite3_mprintf("%Q", token_value.c_str());
                rlc->add_possibility(context, type, std::string(quoted_token));
                break;
            }
        }

        switch (dt) {
            case DT_QUOTED_STRING:
                add_text_possibilities(context, type, ds.get_input().get_substr(pc[0]));
                break;
            default:
                break;
        }
    }
}

void add_view_text_possibilities(int context, const string &type, textview_curses *tc)
{
    text_sub_source *tss = tc->get_sub_source();
    readline_curses *rlc = lnav_data.ld_rl_view;

    rlc->clear_possibilities(context, type);

    // XXX We might want to remove this since it can be kinda slow.
    {
        auto_mem<FILE> pfile(pclose);

        pfile = open_clipboard(CT_FIND, CO_READ);
        if (pfile.in() != nullptr) {
            char buffer[64];

            if (fgets(buffer, sizeof(buffer), pfile) != nullptr) {
                char *nl;

                buffer[sizeof(buffer) - 1] = '\0';
                if ((nl = strchr(buffer, '\n')) != nullptr) {
                    *nl = '\0';
                }
                rlc->add_possibility(context, type, std::string(buffer));
            }
        }
    }

    for (vis_line_t curr_line = tc->get_top();
         curr_line <= tc->get_bottom();
         ++curr_line) {
        string line;

        tss->text_value_for_line(*tc, curr_line, line, text_sub_source::RF_RAW);

        add_text_possibilities(context, type, line);
    }
    
    rlc->add_possibility(context, type, bookmark_metadata::KNOWN_TAGS);
}

void add_env_possibilities(int context)
{
    extern char **environ;
    readline_curses *rlc = lnav_data.ld_rl_view;

    for (char **var = environ; *var != nullptr; var++) {
        rlc->add_possibility(context, "*", "$" + string(*var, strchr(*var, '=')));
    }

    exec_context &ec = lnav_data.ld_exec_context;

    if (!ec.ec_local_vars.empty()) {
        for (const auto &iter : ec.ec_local_vars.top()) {
            rlc->add_possibility(context, "*", "$" + iter.first);
        }
    }

    for (const auto &iter : ec.ec_global_vars) {
        rlc->add_possibility(context, "*", "$" + iter.first);
    }
}

void add_filter_possibilities(textview_curses *tc)
{
    readline_curses *rc = lnav_data.ld_rl_view;
    text_sub_source *tss = tc->get_sub_source();
    filter_stack &fs = tss->get_filters();

    rc->clear_possibilities(LNM_COMMAND, "all-filters");
    rc->clear_possibilities(LNM_COMMAND, "disabled-filter");
    rc->clear_possibilities(LNM_COMMAND, "enabled-filter");
    for (const auto &tf : fs) {
        rc->add_possibility(LNM_COMMAND, "all-filters", tf->get_id());
        if (tf->is_enabled()) {
            rc->add_possibility(LNM_COMMAND, "enabled-filter", tf->get_id());
        }
        else {
            rc->add_possibility(LNM_COMMAND, "disabled-filter", tf->get_id());
        }
    }
}

void add_mark_possibilities()
{
    readline_curses *rc = lnav_data.ld_rl_view;

    rc->clear_possibilities(LNM_COMMAND, "mark-type");
    for (auto iter = bookmark_type_t::type_begin();
         iter != bookmark_type_t::type_end();
         ++iter) {
        bookmark_type_t *bt = (*iter);

        if (bt->get_name().empty()) {
            continue;
        }
        rc->add_possibility(LNM_COMMAND, "mark-type", bt->get_name());
    }
}

void add_config_possibilities()
{
    readline_curses *rc = lnav_data.ld_rl_view;
    vector<string> config_options;

    rc->clear_possibilities(LNM_COMMAND, "config-option");

    for (int lpc = 0; lnav_config_handlers[lpc].jph_path[0]; lpc++) {
        lnav_config_handlers[lpc].possibilities(config_options, &lnav_config);
    }
    rc->add_possibility(LNM_COMMAND, "config-option", config_options);
}

void add_tag_possibilities()
{
    readline_curses *rc = lnav_data.ld_rl_view;

    rc->clear_possibilities(LNM_COMMAND, "tag");
    rc->clear_possibilities(LNM_COMMAND, "line-tags");
    rc->add_possibility(LNM_COMMAND, "tag", bookmark_metadata::KNOWN_TAGS);
    if (lnav_data.ld_view_stack.back() == &lnav_data.ld_views[LNV_LOG]) {
        logfile_sub_source &lss = lnav_data.ld_log_source;
        content_line_t cl = lss.at(lnav_data.ld_views[LNV_LOG].get_top());
        const map<content_line_t, bookmark_metadata> &user_meta =
            lss.get_user_bookmark_metadata();
        auto meta_iter = user_meta.find(cl);

        if (meta_iter != user_meta.end()) {
            rc->add_possibility(LNM_COMMAND,
                                "line-tags",
                                meta_iter->second.bm_tags);
        }
    }
}
