/**
 * Copyright (c) 2018, Timothy Stack
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

#ifndef _log_gutter_source_hh
#define _log_gutter_source_hh

#include "logfile_sub_source.hh"
#include "command_executor.hh"

class log_gutter_source : public list_gutter_source {
public:
    void listview_gutter_value_for_range(
        const listview_curses &lv, int start, int end, chtype &ch,
        view_colors::role_t &role_out, view_colors::role_t &bar_role_out) {
        textview_curses *tc = (textview_curses *)&lv;
        vis_bookmarks &bm = tc->get_bookmarks();
        vis_line_t next;
        bool search_hit = false;

        start -= 1;

        next = bm[&textview_curses::BM_SEARCH].next(vis_line_t(start));
        search_hit = (next != -1 && next <= end);

        next = bm[&BM_QUERY].next(vis_line_t(start));
        search_hit = search_hit || (next != -1 && next <= end);

        next = bm[&textview_curses::BM_USER].next(vis_line_t(start));
        if (next == -1) {
            next = bm[&textview_curses::BM_META].next(vis_line_t(start));
        }
        if (next != -1 && next <= end) {
            ch = search_hit ? ACS_PLUS : ACS_LTEE;
        }
        else {
            ch = search_hit ? ACS_RTEE : ACS_VLINE;
        }
        next = bm[&logfile_sub_source::BM_ERRORS].next(vis_line_t(start));
        if (next != -1 && next <= end) {
            role_out = view_colors::VCR_ERROR;
            bar_role_out = view_colors::VCR_ALERT_STATUS;
        }
        else {
            next = bm[&logfile_sub_source::BM_WARNINGS].next(vis_line_t(start));
            if (next != -1 && next <= end) {
                role_out = view_colors::VCR_WARNING;
                bar_role_out = view_colors::VCR_WARN_STATUS;
            }
        }
    };
};

#endif
