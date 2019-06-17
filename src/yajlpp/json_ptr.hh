/**
 * Copyright (c) 2014-2019, Timothy Stack
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
 * THIS SOFTWARE IS PROVIDED BY TIMOTHY STACK AND CONTRIBUTORS ''AS IS'' AND ANY
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
 * @file json_ptr.hh
 */

#ifndef __json_ptr_hh
#define __json_ptr_hh

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include "auto_mem.hh"
#include "yajl/api/yajl_parse.h"
#include "yajl/api/yajl_tree.h"

class json_ptr_walk {
public:
    const static yajl_callbacks callbacks;

    json_ptr_walk() : jpw_handle(yajl_free), jpw_max_ptr_len(0) {
        this->jpw_handle = yajl_alloc(&callbacks, NULL, this);
    };

    yajl_status parse(const char *buffer, ssize_t len) {
        yajl_status retval;

        retval = yajl_parse(this->jpw_handle, (const unsigned char *)buffer, len);
        this->update_error_msg(retval, buffer, len);
        return retval;
    };

    yajl_status complete_parse() {
        yajl_status retval;

        retval = yajl_complete_parse(this->jpw_handle);
        this->update_error_msg(retval, NULL, -1);
        return retval;
    };

    void update_error_msg(yajl_status status, const char *buffer, ssize_t len) {
        switch (status) {
        case yajl_status_ok:
            break;
        case yajl_status_client_canceled:
            this->jpw_error_msg = "internal error";
            break;
        case yajl_status_error:
            this->jpw_error_msg = std::string((const char *)yajl_get_error(
                this->jpw_handle, 1, (const unsigned char *)buffer, len));
            break;
        }
    };

    void clear() {
        this->jpw_values.clear();
    };

    void inc_array_index() {
        if (!this->jpw_array_indexes.empty() &&
            this->jpw_array_indexes.back() != -1) {
            this->jpw_array_indexes.back() += 1;
        }
    };

    std::string current_ptr();

    struct walk_triple {
        walk_triple(std::string ptr, yajl_type type, std::string value)
                : wt_ptr(std::move(ptr)), wt_type(type), wt_value(
            std::move(value)) {

        };

        std::string wt_ptr;
        yajl_type wt_type;
        std::string wt_value;
    };

    typedef std::vector<walk_triple> walk_list_t;

    auto_mem<yajl_handle_t> jpw_handle;
    std::string jpw_error_msg;
    walk_list_t jpw_values;
    std::vector<std::string> jpw_keys;
    std::vector<int32_t> jpw_array_indexes;
    size_t jpw_max_ptr_len;
};

class json_ptr {
public:
    enum match_state_t {
        MS_DONE,
        MS_VALUE,
        MS_ERR_INVALID_TYPE,
        MS_ERR_NO_SLASH,
        MS_ERR_INVALID_ESCAPE,
        MS_ERR_INVALID_INDEX,
    };

    static size_t encode(char *dst, size_t dst_len, const char *src, size_t src_len = -1);

    static size_t decode(char *dst, const char *src, ssize_t src_len = -1);

    json_ptr(const char *value)
        : jp_value(value), jp_pos(value), jp_depth(0), jp_array_index(-1),
          jp_state(MS_VALUE) {

    };

    bool expect_map(int32_t &depth, int32_t &index);

    bool at_key(int32_t depth, const char *component, ssize_t len = -1);

    void exit_container(int32_t &depth, int32_t &index);

    bool expect_array(int32_t &depth, int32_t &index);

    bool at_index(int32_t &depth, int32_t &index, bool primitive = true);

    bool reached_end() {
        return this->jp_pos[0] == '\0';
    };

    std::string error_msg() const {
        std::string retval;

        switch (this->jp_state) {
        case MS_ERR_INVALID_ESCAPE:
            retval = ("invalid escape sequence near -- " + std::string(this->jp_pos));
            break;
        case MS_ERR_INVALID_INDEX:
            retval = ("expecting array index at -- " + std::string(this->jp_pos));
            break;
        case MS_ERR_INVALID_TYPE:
            retval = ("expecting container at -- " + std::string(
                this->jp_value, this->jp_pos - this->jp_value));
            break;
        default:
            break;
        }

        return retval;
    };

    const char *jp_value;
    const char *jp_pos;
    int32_t jp_depth;
    int32_t jp_array_index;
    match_state_t jp_state;
};

#endif
