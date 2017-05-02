// Copyright (C) 2016-2017 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#include <standardese/markup/documentation.hpp>

#include <standardese/markup/entity_kind.hpp>

using namespace standardese::markup;

entity_kind file_documentation::do_get_kind() const noexcept
{
    return entity_kind::file_documentation;
}

entity_kind entity_documentation::do_get_kind() const noexcept
{
    return entity_kind::entity_documentation;
}
