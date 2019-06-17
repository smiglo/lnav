/**
 * Copyright (c) 2007-2012, Timothy Stack
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
 *
 * @file logfile_sub_source.hh
 */

#ifndef __logfile_sub_source_hh
#define __logfile_sub_source_hh

#include <limits.h>

#include <map>
#include <list>
#include <sstream>
#include <utility>
#include <vector>
#include <algorithm>

#include "lnav_log.hh"
#include "log_accel.hh"
#include "strong_int.hh"
#include "logfile.hh"
#include "bookmarks.hh"
#include "big_array.hh"
#include "textview_curses.hh"
#include "filter_observer.hh"

STRONG_INT_TYPE(uint64_t, content_line);

class logfile_sub_source;

class index_delegate {
public:
    virtual ~index_delegate() { };

    virtual void index_start(logfile_sub_source &lss) {

    };

    virtual void index_line(logfile_sub_source &lss, logfile *lf, logfile::iterator ll) {

    };

    virtual void index_complete(logfile_sub_source &lss) {

    };
};

/**
 * Delegate class that merges the contents of multiple log files into a single
 * source of data for a text view.
 */
class logfile_sub_source
    : public text_sub_source,
      public text_time_translator,
      public list_input_delegate {
public:

    static bookmark_type_t BM_ERRORS;
    static bookmark_type_t BM_WARNINGS;
    static bookmark_type_t BM_FILES;

    virtual void text_filters_changed();

    logfile_sub_source();
    virtual ~logfile_sub_source();

    void toggle_time_offset() {
        this->lss_flags ^= F_TIME_OFFSET;
        this->clear_line_size_cache();
    };

    void increase_line_context() {
        auto old_flags = this->lss_flags;

        if (this->lss_flags & F_FILENAME) {
            // Nothing to do
        } else if (this->lss_flags & F_BASENAME) {
            this->lss_flags &= ~F_NAME_MASK;
            this->lss_flags |= F_FILENAME;
        } else {
            this->lss_flags |= F_BASENAME;
        }
        if (old_flags != this->lss_flags) {
            this->clear_line_size_cache();
        }
    };

    bool decrease_line_context() {
        auto old_flags = this->lss_flags;

        if (this->lss_flags & F_FILENAME) {
            this->lss_flags &= ~F_NAME_MASK;
            this->lss_flags |= F_BASENAME;
        } else if (this->lss_flags & F_BASENAME) {
            this->lss_flags &= ~F_NAME_MASK;
        }
        if (old_flags != this->lss_flags) {
            this->clear_line_size_cache();

            return true;
        }

        return false;
    };

    size_t get_filename_offset() const {
        if (this->lss_flags & F_FILENAME) {
            return this->lss_filename_width;
        } else if (this->lss_flags & F_BASENAME) {
            return this->lss_basename_width;
        }

        return 0;
    }

    void set_time_offset(bool enabled) {
        if (enabled)
            this->lss_flags |= F_TIME_OFFSET;
        else
            this->lss_flags &= ~F_TIME_OFFSET;
        this->clear_line_size_cache();
    };

    bool is_time_offset_enabled() const {
        return (bool) (this->lss_flags & F_TIME_OFFSET);
    };

    bool is_filename_enabled() const {
        return (bool) (this->lss_flags & F_FILENAME);
    };

    bool is_basename_enabled() const {
        return (bool) (this->lss_flags & F_BASENAME);
    };

    log_level_t get_min_log_level() const {
        return this->lss_min_log_level;
    };

    void set_min_log_level(log_level_t level) {
        if (this->lss_min_log_level != level) {
            this->lss_min_log_level = level;
            this->lss_force_rebuild = true;
        }
    };

    bool get_min_log_time(struct timeval &tv_out) const {
        tv_out = this->lss_min_log_time;
        return (this->lss_min_log_time.tv_sec != 0 ||
                this->lss_min_log_time.tv_usec != 0);
    };

    void set_min_log_time(const struct timeval &tv) {
        if (this->lss_min_log_time != tv) {
            this->lss_min_log_time = tv;
            this->lss_force_rebuild = true;
        }
    };

    bool get_max_log_time(struct timeval &tv_out) const {
        tv_out = this->lss_max_log_time;
        return (this->lss_max_log_time.tv_sec != std::numeric_limits<time_t>::max() ||
                this->lss_max_log_time.tv_usec != 0);
    };

    void set_max_log_time(struct timeval &tv) {
        if (this->lss_max_log_time != tv) {
            this->lss_max_log_time = tv;
            this->lss_force_rebuild = true;
        }
    };

    void clear_min_max_log_times() {
        memset(&this->lss_min_log_time, 0, sizeof(this->lss_min_log_time));
        this->lss_max_log_time.tv_sec = std::numeric_limits<time_t>::max();
        this->lss_max_log_time.tv_usec = 0;
        this->lss_force_rebuild = true;
    };

    bool list_input_handle_key(listview_curses &lv, int ch);

    void set_marked_only(bool val) {
        if (this->lss_marked_only != val) {
            this->lss_marked_only = val;
            this->lss_force_rebuild = true;
        }
    };

    bool get_marked_only() {
        return this->lss_marked_only;
    };

    size_t text_line_count()
    {
        return this->lss_filtered_index.size();
    };

    size_t text_line_width(textview_curses &curses) {
        return this->lss_longest_line;
    };

    size_t file_count() const {
        size_t retval = 0;
        const_iterator iter;

        for (iter = this->cbegin(); iter != this->cend(); ++iter) {
            if (*iter != NULL && (*iter)->get_file() != NULL) {
                retval += 1;
            }
        }

        return retval;
    }

    bool empty() const { return this->lss_filtered_index.empty(); };

    void text_value_for_line(textview_curses &tc,
                             int row,
                             std::string &value_out,
                             line_flags_t flags);

    void text_attrs_for_line(textview_curses &tc,
                             int row,
                             string_attrs_t &value_out);

    size_t text_size_for_line(textview_curses &tc, int row, line_flags_t flags) {
        size_t index = row % LINE_SIZE_CACHE_SIZE;

        if (this->lss_line_size_cache[index].first != row) {
            std::string value;

            this->text_value_for_line(tc, row, value, flags);
            this->lss_line_size_cache[index].second = value.size();
            this->lss_line_size_cache[index].first = row;
        }
        return this->lss_line_size_cache[index].second;
    };

    void text_mark(bookmark_type_t *bm, vis_line_t line, bool added)
    {
        if (line >= (int) this->lss_index.size()) {
            return;
        }

        content_line_t cl = this->at(line);
        std::vector<content_line_t>::iterator lb;

        if (bm == &textview_curses::BM_USER) {
            logline *ll = this->find_line(cl);

            ll->set_mark(added);
        }
        lb = std::lower_bound(this->lss_user_marks[bm].begin(),
                              this->lss_user_marks[bm].end(),
                              cl);
        if (added) {
            if (lb == this->lss_user_marks[bm].end() || *lb != cl) {
                this->lss_user_marks[bm].insert(lb, cl);
            }
        }
        else if (lb != this->lss_user_marks[bm].end() && *lb == cl) {
            require(lb != this->lss_user_marks[bm].end());

            this->lss_user_marks[bm].erase(lb);
        }
        if (bm == &textview_curses::BM_META &&
            this->lss_meta_grepper.gps_proc != nullptr) {
            this->lss_meta_grepper.gps_proc->queue_request(line, line + 1_vl);
        }
    };

    void text_clear_marks(bookmark_type_t *bm)
    {
        std::vector<content_line_t>::iterator iter;

        if (bm == &textview_curses::BM_USER) {
            for (iter = this->lss_user_marks[bm].begin();
                 iter != this->lss_user_marks[bm].end();) {
                auto bm_iter = this->lss_user_mark_metadata.find(*iter);
                if (bm_iter != this->lss_user_mark_metadata.end()) {
                    ++iter;
                    continue;
                }
                this->find_line(*iter)->set_mark(false);
                iter = this->lss_user_marks[bm].erase(iter);
            }
        } else {
            this->lss_user_marks[bm].clear();
        }
    };

    bool insert_file(std::shared_ptr<logfile> lf)
    {
        iterator existing;

        require(lf->size() < MAX_LINES_PER_FILE);

        existing = std::find_if(this->lss_files.begin(),
                                this->lss_files.end(),
                                logfile_data_eq(NULL));
        if (existing == this->lss_files.end()) {
            if (this->lss_files.size() >= MAX_FILES) {
                return false;
            }

            this->lss_files.push_back(new logfile_data(this->lss_files.size(), this->get_filters(), lf));
        }
        else {
            (*existing)->set_file(lf);
        }

        return true;
    };

    void remove_file(std::shared_ptr<logfile> lf)
    {
        iterator iter;

        iter = std::find_if(this->lss_files.begin(),
                            this->lss_files.end(),
                            logfile_data_eq(lf));
        if (iter != this->lss_files.end()) {
            bookmarks<content_line_t>::type::iterator mark_iter;
            int file_index = iter - this->lss_files.begin();

            (*iter)->clear();
            for (mark_iter = this->lss_user_marks.begin();
                 mark_iter != this->lss_user_marks.end();
                 ++mark_iter) {
                content_line_t mark_curr = content_line_t(
                    file_index * MAX_LINES_PER_FILE);
                content_line_t mark_end = content_line_t(
                    (file_index + 1) * MAX_LINES_PER_FILE);
                bookmark_vector<content_line_t>::iterator bv_iter;
                bookmark_vector<content_line_t> &         bv =
                    mark_iter->second;

                while ((bv_iter =
                            std::lower_bound(bv.begin(), bv.end(),
                                             mark_curr)) != bv.end()) {
                    if (*bv_iter >= mark_end) {
                        break;
                    }
                    mark_iter->second.erase(bv_iter);
                }
            }
        }
    };

    bool rebuild_index(bool force = false);

    void text_update_marks(vis_bookmarks &bm);

    void set_user_mark(bookmark_type_t *bm, content_line_t cl)
    {
        this->lss_user_marks[bm].insert_once(cl);
    };

    bookmarks<content_line_t>::type &get_user_bookmarks()
    {
        return this->lss_user_marks;
    };

    std::map<content_line_t, bookmark_metadata> &get_user_bookmark_metadata() {
        return this->lss_user_mark_metadata;
    };

    int get_filtered_count() const {
        return this->lss_index.size() - this->lss_filtered_index.size();
    };

    std::shared_ptr<logfile> find(const char *fn, content_line_t &line_base);

    std::shared_ptr<logfile> find(content_line_t &line)
    {
        std::shared_ptr<logfile> retval;

        retval = this->lss_files[line / MAX_LINES_PER_FILE]->get_file();
        line   = content_line_t(line % MAX_LINES_PER_FILE);

        return retval;
    };

    logline *find_line(content_line_t line)
    {
        logline *retval = nullptr;
        std::shared_ptr<logfile> lf     = this->find(line);

        if (lf != nullptr) {
            auto ll_iter = lf->begin() + line;

            retval = &(*ll_iter);
        }

        return retval;
    };

    vis_line_t find_from_time(const struct timeval &start);

    vis_line_t find_from_time(time_t start) {
        struct timeval tv = { start, 0 };

        return this->find_from_time(tv);
    };

    vis_line_t find_from_time(exttm &etm) {
        struct timeval tv;

        tv.tv_sec = timegm(&etm.et_tm);
        tv.tv_usec = etm.et_nsec / 1000;

        return this->find_from_time(tv);
    };

    struct timeval time_for_row(int row) {
        return this->find_line(this->at(vis_line_t(row)))->get_timeval();
    };

    int row_for_time(struct timeval time_bucket) {
        return this->find_from_time(time_bucket);
    };

    content_line_t at(vis_line_t vl) {
        return this->lss_index[this->lss_filtered_index[vl]];
    };

    content_line_t at_base(vis_line_t vl) {
        while (this->find_line(this->at(vl))->get_sub_offset() != 0) {
            --vl;
        }

        return this->at(vl);
    };

    log_accel::direction_t get_line_accel_direction(vis_line_t vl);

    /**
     * Container for logfile references that keeps of how many lines in the
     * logfile have been indexed.
     */
    struct logfile_data {
        logfile_data(size_t index, filter_stack &fs, std::shared_ptr<logfile> lf)
            : ld_file_index(index),
              ld_filter_state(fs, lf),
              ld_lines_indexed(0),
              ld_enabled(true) {
            lf->set_logline_observer(&this->ld_filter_state);
        };

        void clear()
        {
            this->ld_filter_state.lfo_filter_state.clear();
        };

        void set_enabled(bool enabled) {
            this->ld_enabled = enabled;
        }

        void set_file(const std::shared_ptr<logfile> &lf) {
            this->ld_filter_state.lfo_filter_state.tfs_logfile = lf;
            lf->set_logline_observer(&this->ld_filter_state);
        };

        std::shared_ptr<logfile> get_file() const {
            return this->ld_filter_state.lfo_filter_state.tfs_logfile;
        };

        size_t ld_file_index;
        line_filter_observer ld_filter_state;
        size_t ld_lines_indexed;
        bool ld_enabled;
    };

    typedef std::vector<logfile_data *>::iterator iterator;
    typedef std::vector<logfile_data *>::const_iterator const_iterator;

    iterator begin()
    {
        return this->lss_files.begin();
    };

    iterator end()
    {
        return this->lss_files.end();
    };

    const_iterator cbegin() const
    {
        return this->lss_files.begin();
    };

    const_iterator cend() const
    {
        return this->lss_files.end();
    };

    logfile_data *find_data(content_line_t line, uint64_t &offset_out)
    {
        logfile_data *retval;

        retval = this->lss_files[line / MAX_LINES_PER_FILE];
        offset_out = line % MAX_LINES_PER_FILE;

        return retval;
    };

    content_line_t get_file_base_content_line(iterator iter) {
        ssize_t index = std::distance(this->begin(), iter);

        return content_line_t(index * MAX_LINES_PER_FILE);
    };

    void set_index_delegate(index_delegate *id) {
        if (id != this->lss_index_delegate) {
            this->lss_index_delegate = id;
            this->reload_index_delegate();
        }
    };

    index_delegate *get_index_delegate() const {
        return this->lss_index_delegate;
    };

    void reload_index_delegate() {
        if (this->lss_index_delegate == nullptr) {
            return;
        }

        this->lss_index_delegate->index_start(*this);
        for (unsigned int index : this->lss_filtered_index) {
            content_line_t cl = (content_line_t) this->lss_index[index];
            uint64_t line_number;
            logfile_data *ld = this->find_data(cl, line_number);
            std::shared_ptr<logfile> lf = ld->get_file();

            this->lss_index_delegate->index_line(*this, lf.get(), lf->begin() + line_number);
        }
        this->lss_index_delegate->index_complete(*this);
    };

    class meta_grepper
        : public grep_proc_source<vis_line_t>,
          public grep_proc_sink<vis_line_t> {
    public:
        meta_grepper(logfile_sub_source &source)
            : lmg_source(source) {
        };

        bool grep_value_for_line(vis_line_t line, std::string &value_out) override {
            content_line_t cl = this->lmg_source.at(vis_line_t(line));
            std::map<content_line_t, bookmark_metadata> &user_mark_meta =
                lmg_source.get_user_bookmark_metadata();
            auto meta_iter = user_mark_meta.find(cl);

            if (meta_iter == user_mark_meta.end()) {
                value_out.clear();
            } else {
                bookmark_metadata &bm = meta_iter->second;

                value_out.append(bm.bm_comment);
                for (const auto &tag : bm.bm_tags) {
                    value_out.append(tag);
                }
            }

            return !this->lmg_done;
        };

        vis_line_t grep_initial_line(vis_line_t start, vis_line_t highest) override {
            vis_bookmarks &bm = this->lmg_source.tss_view->get_bookmarks();
            bookmark_vector<vis_line_t> &bv = bm[&textview_curses::BM_META];

            if (bv.empty()) {
                return -1_vl;
            }
            return *bv.begin();
        };

        void grep_next_line(vis_line_t &line) override {
            vis_bookmarks &bm = this->lmg_source.tss_view->get_bookmarks();
            bookmark_vector<vis_line_t> &bv = bm[&textview_curses::BM_META];

            line = bv.next(vis_line_t(line));
            if (line == -1) {
                this->lmg_done = true;
            }
        };

        void grep_begin(grep_proc<vis_line_t> &gp, vis_line_t start, vis_line_t stop) override {
            this->lmg_source.tss_view->grep_begin(gp, start, stop);
        };

        void grep_end(grep_proc<vis_line_t> &gp) override {
            this->lmg_source.tss_view->grep_end(gp);
        };

        void grep_match(grep_proc<vis_line_t> &gp,
                        vis_line_t line,
                        int start,
                        int end) override {
            this->lmg_source.tss_view->grep_match(gp, line, start, end);
        };

        logfile_sub_source &lmg_source;
        bool lmg_done{false};
    };

    meta_grepper &get_meta_grepper() {
        return this->lss_meta_grepper;
    }

    static const uint64_t MAX_CONTENT_LINES = (1ULL << 40) - 1;
    static const uint64_t MAX_LINES_PER_FILE = 256 * 1024 * 1024;
    static const uint64_t MAX_FILES          = (
        MAX_CONTENT_LINES / MAX_LINES_PER_FILE);

private:
    static const size_t LINE_SIZE_CACHE_SIZE = 512;

    enum {
        B_SCRUB,
        B_TIME_OFFSET,
        B_FILENAME,
        B_BASENAME,
    };

    enum {
        F_SCRUB       = (1UL << B_SCRUB),
        F_TIME_OFFSET = (1UL << B_TIME_OFFSET),
        F_FILENAME    = (1UL << B_FILENAME),
        F_BASENAME    = (1UL << B_BASENAME),

        F_NAME_MASK   = (F_FILENAME | F_BASENAME),
    };

    struct __attribute__((__packed__)) indexed_content {
        indexed_content() {

        };

        indexed_content(content_line_t cl) : ic_value(cl) {

        };

        operator content_line_t () const {
            return content_line_t(this->ic_value);
        };

        uint64_t ic_value : 40;
    };

    struct logline_cmp {
        logline_cmp(logfile_sub_source & lc)
            : llss_controller(lc) { };
        bool operator()(const content_line_t &lhs, const content_line_t &rhs) const
        {
            logline *ll_lhs = this->llss_controller.find_line(lhs);
            logline *ll_rhs = this->llss_controller.find_line(rhs);

            return (*ll_lhs) < (*ll_rhs);
        };

        bool operator()(const uint32_t &lhs, const uint32_t &rhs) const
        {
            content_line_t cl_lhs = (content_line_t)
                    llss_controller.lss_index[lhs];
            content_line_t cl_rhs = (content_line_t)
                    llss_controller.lss_index[rhs];
            logline *ll_lhs = this->llss_controller.find_line(cl_lhs);
            logline *ll_rhs = this->llss_controller.find_line(cl_rhs);

            return (*ll_lhs) < (*ll_rhs);
        };
#if 0
        bool operator()(const indexed_content &lhs, const indexed_content &rhs)
        {
            logline *ll_lhs = this->llss_controller.find_line(lhs.ic_value);
            logline *ll_rhs = this->llss_controller.find_line(rhs.ic_value);

            return (*ll_lhs) < (*ll_rhs);
        };
#endif
        bool operator()(const content_line_t &lhs, const time_t &rhs) const
        {
            logline *ll_lhs = this->llss_controller.find_line(lhs);

            return *ll_lhs < rhs;
        };
        bool operator()(const content_line_t &lhs, const struct timeval &rhs) const
        {
            logline *ll_lhs = this->llss_controller.find_line(lhs);

            return *ll_lhs < rhs;
        };

        logfile_sub_source & llss_controller;
    };

    struct filtered_logline_cmp {
        filtered_logline_cmp(logfile_sub_source & lc)
                : llss_controller(lc) { };
        bool operator()(const uint32_t &lhs, const uint32_t &rhs) const
        {
            content_line_t cl_lhs = (content_line_t)
                    llss_controller.lss_index[lhs];
            content_line_t cl_rhs = (content_line_t)
                    llss_controller.lss_index[rhs];
            logline *ll_lhs = this->llss_controller.find_line(cl_lhs);
            logline *ll_rhs = this->llss_controller.find_line(cl_rhs);

            return (*ll_lhs) < (*ll_rhs);
        };

        bool operator()(const uint32_t &lhs, const struct timeval &rhs) const
        {
            content_line_t cl_lhs = (content_line_t)
                    llss_controller.lss_index[lhs];
            logline *ll_lhs = this->llss_controller.find_line(cl_lhs);

            return (*ll_lhs) < rhs;
        };

        logfile_sub_source & llss_controller;
    };

    /**
     * Functor for comparing the ld_file field of the logfile_data struct.
     */
    struct logfile_data_eq {
        explicit logfile_data_eq(std::shared_ptr<logfile> lf) : lde_file(std::move(lf)) { };

        bool operator()(const logfile_data *ld)
        {
            return this->lde_file == ld->get_file();
        }

        std::shared_ptr<logfile> lde_file;
    };

    void clear_line_size_cache() {
        memset(this->lss_line_size_cache, 0, sizeof(this->lss_line_size_cache));
        this->lss_line_size_cache[0].first = -1;
    };

    bool check_extra_filters(const logline &ll) {
        if (this->lss_marked_only && !ll.is_marked()) {
            return false;
        }

        return (
            ll.get_msg_level() >= this->lss_min_log_level &&
            !(ll < this->lss_min_log_time) &&
            ll <= this->lss_max_log_time);
    };

    size_t                    lss_basename_width = 0;
    size_t                    lss_filename_width = 0;
    unsigned long             lss_flags;
    bool lss_force_rebuild;
    std::vector<logfile_data *> lss_files;

    big_array<indexed_content> lss_index;
    std::vector<uint32_t> lss_filtered_index;

    bookmarks<content_line_t>::type lss_user_marks;
    std::map<content_line_t, bookmark_metadata> lss_user_mark_metadata;

    line_flags_t lss_token_flags;
    std::shared_ptr<logfile> lss_token_file;
    std::string       lss_token_value;
    string_attrs_t    lss_token_attrs;
    std::vector<logline_value> lss_token_values;
    int lss_token_shift_start;
    int lss_token_shift_size;
    shared_buffer     lss_share_manager;
    logfile::iterator lss_token_line;
    std::pair<int, size_t> lss_line_size_cache[LINE_SIZE_CACHE_SIZE];
    log_level_t  lss_min_log_level;
    struct timeval    lss_min_log_time;
    struct timeval    lss_max_log_time;
    bool lss_marked_only;
    index_delegate    *lss_index_delegate;
    size_t            lss_longest_line;
    meta_grepper lss_meta_grepper;
};

#endif
