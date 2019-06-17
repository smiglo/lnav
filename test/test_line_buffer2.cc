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
 */

#include "config.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "auto_fd.hh"
#include "line_buffer.hh"

static const char *TEST_DATA =
    "Hello, World!\n"
    "Goodbye, World!\n";

static void single_line(const char *data)
{
    line_buffer lb;
    auto_fd pi[2];
    off_t off = 0;

    assert(auto_fd::pipe(pi) == 0);
    log_perror(write(pi[1], data, strlen(data)));
    pi[1].reset();
    
    lb.set_fd(pi[0]);
    auto load_result = lb.load_next_line({off});
    auto li = load_result.unwrap();
    assert(data[strlen(data) - 1] == '\n' || li.li_partial);
    assert(li.li_file_range.next_offset() == (off_t)strlen(data));
    assert(li.li_file_range.fr_size == strlen(data));
    
    auto next_load_result = lb.load_next_line(li.li_file_range);
    assert(next_load_result.isOk());
    assert(next_load_result.unwrap().li_file_range.empty());
    assert(lb.get_file_size() != -1);
}

int main(int argc, char *argv[])
{
    int retval = EXIT_SUCCESS;

    single_line("Dexter Morgan");
    single_line("Rudy Morgan\n");

    {
        char fn_template[] = "test_line_buffer.XXXXXX";

        auto fd = auto_fd(mkstemp(fn_template));
        auto lb = line_buffer();

        write(fd, TEST_DATA, strlen(TEST_DATA));
        lseek(fd, SEEK_SET, 0);

        lb.set_fd(fd);

        shared_buffer_ref sbr;

        auto result = lb.read_range({0, 1024});

        assert(result.isErr());
    }

    return retval;
}
