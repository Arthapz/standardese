// Copyright (C) 2016 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#include <standardese/generator.hpp>

#include <cmark.h>

#include <standardese/comment.hpp>
#include <standardese/cpp_class.hpp>
#include <standardese/cpp_enum.hpp>
#include <standardese/cpp_function.hpp>
#include <standardese/cpp_namespace.hpp>
#include <standardese/cpp_template.hpp>
#include <standardese/index.hpp>
#include <standardese/md_blocks.hpp>
#include <standardese/md_inlines.hpp>
#include <standardese/parser.hpp>

using namespace standardese;

namespace
{
    template <class Entity>
    cpp_access_specifier_t get_default_access(const Entity&)
    {
        return cpp_public;
    }

    cpp_access_specifier_t get_default_access(const cpp_class& e)
    {
        return e.get_class_type() == cpp_class_t ? cpp_private : cpp_public;
    }

    cpp_access_specifier_t get_default_access(const cpp_class_template& e)
    {
        return get_default_access(e.get_class());
    }

    cpp_access_specifier_t get_default_access(const cpp_class_template_full_specialization& e)
    {
        return get_default_access(e.get_class());
    }

    cpp_access_specifier_t get_default_access(const cpp_class_template_partial_specialization& e)
    {
        return get_default_access(e.get_class());
    }

    bool is_blacklisted(const parser& p, const cpp_entity& e)
    {
        auto& blacklist = p.get_output_config().get_blacklist();
        if (e.get_entity_type() != cpp_entity::namespace_t
            && blacklist.is_set(entity_blacklist::require_comment) && e.has_comment()
            && e.get_comment()
                   .empty()) // only valid for entities which can have comments except namespaces
            return true;
        else if (blacklist.is_blacklisted(entity_blacklist::documentation, e))
            return true;
        else if (e.has_comment() && e.get_comment().is_excluded())
            return true;

        return false;
    }

    using standardese::index; // to force standardese::index instead of ::index

    void dispatch(const parser& p, const index& i, md_document& output, unsigned level,
                  const cpp_entity& e);

    template <class Entity, class Container>
    void handle_container(const parser& p, const index& i, md_document& out, unsigned level,
                          const doc_entity& doc, const Container& container)
    {
        if (is_blacklisted(p, doc.get_cpp_entity()))
            return;

        generate_doc_entity(p, i, out, level, doc);

        auto  cur_access = get_default_access(static_cast<const Entity&>(doc.get_cpp_entity()));
        auto& blacklist  = p.get_output_config().get_blacklist();
        for (auto& child : container)
        {
            if (child.get_entity_type() == cpp_entity::access_specifier_t)
                cur_access =
                    static_cast<const cpp_access_specifier&>(static_cast<const cpp_entity&>(child))
                        .get_access();
            else if (blacklist.is_set(entity_blacklist::extract_private)
                     || cur_access != cpp_private || detail::is_virtual(child))
                dispatch(p, i, out, level + 1, child);
        }

        out.add_entity(md_thematic_break::make(doc.get_comment()));
    }

    void dispatch(const parser& p, const index& i, md_document& output, unsigned level,
                  const cpp_entity& e)
    {
        if (is_blacklisted(p, e))
            return;

        switch (e.get_entity_type())
        {
        case cpp_entity::namespace_t:
            for (auto& child : static_cast<const cpp_namespace&>(e))
                dispatch(p, i, output, level, child);
            break;

#define STANDARDESE_DETAIL_HANDLE(name, ...)                                                       \
    case cpp_entity::name##_t:                                                                     \
        handle_container<cpp_##name>(p, i, output, level, e,                                       \
                                     static_cast<const cpp_##name&>(e) __VA_ARGS__);               \
        break;

#define STANDARDESE_DETAIL_NOTHING

            STANDARDESE_DETAIL_HANDLE(class, STANDARDESE_DETAIL_NOTHING)
            STANDARDESE_DETAIL_HANDLE(class_template, .get_class())
            STANDARDESE_DETAIL_HANDLE(class_template_full_specialization, .get_class())
            STANDARDESE_DETAIL_HANDLE(class_template_partial_specialization, .get_class())

            STANDARDESE_DETAIL_HANDLE(enum, STANDARDESE_DETAIL_NOTHING)

#undef STANDARDESE_DETAIL_HANDLE
#undef STANDARDESE_DETAIL_NOTHING

        default:
            generate_doc_entity(p, i, output, level, e);
            break;
        }
    }
}

md_ptr<md_document> md_document::make(std::string name)
{
    return detail::make_md_ptr<md_document>(cmark_node_new(CMARK_NODE_DOCUMENT), std::move(name));
}

md_entity_ptr md_document::do_clone(const md_entity* parent) const
{
    assert(!parent);

    auto result = make(name_);
    for (auto& child : *this)
        result->add_entity(child.clone(*result));
    return std::move(result);
}

const char* standardese::get_entity_type_spelling(cpp_entity::type t)
{
    switch (t)
    {
    case cpp_entity::file_t:
        return "header file";

    case cpp_entity::inclusion_directive_t:
        return "inclusion directive";
    case cpp_entity::macro_definition_t:
        return "macro";

    case cpp_entity::language_linkage_t:
        return "language linkage";
    case cpp_entity::namespace_t:
        return "namespace";
    case cpp_entity::namespace_alias_t:
        return "namespace alias";
    case cpp_entity::using_directive_t:
        return "using directive";
    case cpp_entity::using_declaration_t:
        return "using declaration";

    case cpp_entity::type_alias_t:
        return "type alias";
    case cpp_entity::alias_template_t:
        return "alias template";

    case cpp_entity::enum_t:
        return "enumeration";
    case cpp_entity::signed_enum_value_t:
    case cpp_entity::unsigned_enum_value_t:
        return "enumeration constant";

    case cpp_entity::variable_t:
    case cpp_entity::member_variable_t:
    case cpp_entity::bitfield_t:
        return "variable";

    case cpp_entity::function_parameter_t:
        return "parameter";
    case cpp_entity::function_t:
    case cpp_entity::member_function_t:
        return "function";
    case cpp_entity::conversion_op_t:
        return "conversion operator";
    case cpp_entity::constructor_t:
        return "constructor";
    case cpp_entity::destructor_t:
        return "destructor";

    case cpp_entity::template_type_parameter_t:
    case cpp_entity::non_type_template_parameter_t:
    case cpp_entity::template_template_parameter_t:
        return "template parameter";

    case cpp_entity::function_template_t:
    case cpp_entity::function_template_specialization_t:
        return "function template";

    case cpp_entity::class_t:
        return "class";
    case cpp_entity::class_template_t:
    case cpp_entity::class_template_full_specialization_t:
    case cpp_entity::class_template_partial_specialization_t:
        return "class template";

    case cpp_entity::base_class_t:
        return "base class";
    case cpp_entity::access_specifier_t:
        return "access specifier";

    case cpp_entity::invalid_t:
        break;
    }

    return "should never get here";
}

namespace
{
    md_ptr<md_heading> make_heading(const cpp_entity& e, const md_entity& parent, unsigned level)
    {
        auto heading = md_heading::make(parent, level);

        auto type     = get_entity_type_spelling(e.get_entity_type());
        auto text_str = fmt::format("{}{} ", char(std::toupper(type[0])), &type[1]);
        auto text     = md_text::make(*heading, text_str.c_str());
        heading->add_entity(std::move(text));

        auto code = md_code::make(*heading, e.get_full_name().c_str());
        heading->add_entity(std::move(code));

        return heading;
    }
}

void standardese::generate_doc_entity(const parser& p, const index& i, md_document& document,
                                      unsigned level, const doc_entity& doc)
{
    // write heading + anchor
    auto heading = make_heading(doc.get_cpp_entity(), document, level);
    if (doc.has_comment())
    {
        auto anchor = md_anchor::make(*heading, doc.get_unique_name().c_str());
        heading->add_entity(std::move(anchor));
    }
    document.add_entity(std::move(heading));

    // write synopsis
    write_synopsis(p, document, doc);

    // write comment + register
    if (doc.has_comment())
    {
        auto& comment = static_cast<md_comment&>(document.add_entity(doc.get_comment().clone()));
        // need to register the comment that is part of the document
        i.register_entity(doc_entity(doc.get_cpp_entity(), comment));
    }
    else
        // can keep doc as it, doesn't have a comment anyway
        i.register_entity(doc);
}

md_ptr<md_document> standardese::generate_doc_file(const parser& p, const index& i, cpp_file& f,
                                                   std::string name)
{
    auto doc = md_document::make(std::move(name));
    f.set_output_name(doc->get_output_name());

    generate_doc_entity(p, i, *doc, 1, f);

    for (auto& e : f)
        dispatch(p, i, *doc, 2, e);

    return doc;
}

md_ptr<md_document> standardese::generate_file_index(index& i, std::string name)
{
    auto doc = md_document::make(std::move(name));

    auto list = md_list::make(*doc, md_list_type::bullet, md_list_delimiter::none, 0, false);
    i.for_each_file([&](const cpp_file& f) {
        auto& paragraph = make_list_item_paragraph(*list);

        auto link = md_link::make(paragraph, "", f.get_unique_name().c_str());
        link->add_entity(md_text::make(*link, f.get_name().c_str()));
        paragraph.add_entity(std::move(link));

    });
    doc->add_entity(std::move(list));

    return doc;
}

namespace
{
    std::string get_name_signature(const cpp_entity& e)
    {
        auto result = std::string(e.get_name().c_str());
        if (auto base = get_function(e))
            result += base->get_signature().c_str();
        return result;
    }

    void make_index_item(md_list& list, const cpp_entity& e)
    {
        auto& paragraph = make_list_item_paragraph(list);

        // add link to entity
        auto link = md_link::make(paragraph, "", e.get_unique_name().c_str());
        link->add_entity(md_text::make(*link, get_name_signature(e).c_str()));
        paragraph.add_entity(std::move(link));

        // add brief comment to it
        if (e.has_comment() && !e.get_comment().get_brief().empty())
        {
            paragraph.add_entity(md_text::make(paragraph, " - "));
            for (auto& child : e.get_comment().get_brief())
                paragraph.add_entity(child.clone(paragraph));
        }
    }

    md_ptr<md_list_item> make_namespace_item(const md_list& list, const cpp_name& ns_name)
    {
        auto item = md_list_item::make(list);

        auto paragraph = md_paragraph::make(*item);
        paragraph->add_entity(md_code::make(*item, ns_name.c_str()));
        item->add_entity(std::move(paragraph));
        item->add_entity(md_list::make_bullet(*paragraph));

        return item;
    }
}

md_ptr<md_document> standardese::generate_entity_index(index& i, std::string name)
{
    auto doc  = md_document::make(std::move(name));
    auto list = md_list::make_bullet(*doc);

    std::map<std::string, md_ptr<md_list_item>> ns_lists;
    i.for_each_namespace_member([&](const cpp_namespace* ns, const cpp_entity& e) {
        if (!ns)
            make_index_item(*list, e);
        else
        {
            auto ns_name = ns->get_full_name();
            auto iter    = ns_lists.find(ns_name.c_str());
            if (iter == ns_lists.end())
            {
                auto item = make_namespace_item(*list, ns_name);
                iter      = ns_lists.emplace(ns_name.c_str(), std::move(item)).first;
            }

            auto& item = *iter->second;
            assert(std::next(item.begin())->get_entity_type() == md_entity::list_t);
            make_index_item(static_cast<md_list&>(*std::next(item.begin())), e);
        }
    });

    for (auto& p : ns_lists)
        list->add_entity(std::move(p.second));
    doc->add_entity(std::move(list));

    return doc;
}
