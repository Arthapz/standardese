// Copyright (C) 2016-2017 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#include <standardese/markup/paragraph.hpp>

#include "html_helper.hpp"

using namespace standardese::markup;

void paragraph::do_append_html(std::string& result) const
{
    detail::append_newl(result);
    detail::append_html_open(result, "p", id(), "");

    detail::append_container(result, *this);

    result += "</p>\n";
}
