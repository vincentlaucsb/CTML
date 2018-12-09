/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Maxwell Flynn
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef CTML_HPP_
#define CTML_HPP_

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <sstream>
#include <algorithm>

namespace CTML
{
    /**
     * An enum representing the types of HTML nodes that can be constructed.
     */
    enum class NodeType : uint8_t
    {
        COMMENT       = 0x00,
        DOCUMENT_TYPE = 0x01,
        ELEMENT       = 0x02,
        TEXT          = 0x03
    };

    /**
     * An enum representing the state of the parser for a Node name, akin
     * to an Emmet abbreviation.
     */
    enum class NodeParserState : uint8_t
    {
        NONE  = 0x00,
        CLASS = 0x01,
        ID    = 0x02
    };

    /**
     * An enum representing the formatting of the ToString for a Node.
     */
    enum class StringFormatting : uint8_t
    {
        SINGLE_LINE    = 0x00,
        MULTIPLE_LINES = 0x01
    };

    class Node
    {
    public:
        Node() = default;

        /**
         * Create an empty node of the type specified.
         * 
         * Optionally, you can specify a name as well as content
         * which will be added according to the type.
         */
        Node(
            const NodeType& type,
            const std::string& name="",
            const std::string& content=""
        )
            :
            m_type(type)
        {
            // Use the name of the Node for the content as content should be ignored
            if (type == NodeType::COMMENT)
                m_content = name;
            else if (type == NodeType::DOCUMENT_TYPE)
                m_content = name;
            else if (type == NodeType::TEXT)
            {
                m_content = name;

                // escape all of the content text for common characters
                m_content = _ReplaceAllOccurrences(m_content, "&", "&amp;");
                m_content = _ReplaceAllOccurrences(m_content, "<", "&lt;");
                m_content = _ReplaceAllOccurrences(m_content, ">", "&gt;");
                m_content = _ReplaceAllOccurrences(m_content, "\"", "&quot;");
                m_content = _ReplaceAllOccurrences(m_content, "'", "&apos;");
            }
            else if (type == NodeType::ELEMENT)
            {
                this->SetName(name);

                if (!m_content.empty())
                    this->AppendText(content);
            }
        }

        /**
         * Create an element node with the name specified.
         */
        Node(const std::string& name)
            :
            m_type(NodeType::ELEMENT)
        {
            this->SetName(name);
        }

        /**
         * Create an element node with the specified name and containing a text node
         * with the content specified.
         */
        Node(const std::string& name, const std::string& content)
            :
            m_type(NodeType::ELEMENT)
        {
            this->SetName(name);
            this->AppendText(content);
        }

        /**
         * Generate a string for this Node instance.
         * 
         * This function is recursive, so ToString is called for every Node that is a
         * child of this node.
         *
         * You may optionally specify a StringFormatting enum for how to format the string.
         */
        std::string ToString(const StringFormatting& formatting=StringFormatting::SINGLE_LINE) const
        {
            std::stringstream output;

            // format a comment node with only the set content
            if (m_type == NodeType::COMMENT)
            {
                output << "<!--" << m_content << "-->";

                if (formatting == StringFormatting::MULTIPLE_LINES)
                    output << "\n";
            }
            // format a special document type node with the content
            // as the specified type to use
            else if (m_type == NodeType::DOCUMENT_TYPE)
            {
                output << "<!DOCTYPE " << m_content << ">";

                if (formatting == StringFormatting::MULTIPLE_LINES)
                    output << "\n";
            }
            // format a text node with just the content, this node doesn't
            // follow StringFormatting as it could potentially alter the
            // document output
            else if (m_type == NodeType::TEXT)
            {
                output << m_content;
            }
            else if (m_type == NodeType::ELEMENT)
            {
                output << "<" << m_name << " ";

                // output classes if there are any to output
                if (!m_classes.empty())
                {
                    output << "class=\"";

                    for (size_t index = 0; index < m_classes.size(); index++)
                    {
                        output << m_classes.at(index);

                        if (index != m_classes.size() - 1)
                            output << " ";
                    }

                    output << "\" ";
                }

                // output the ID of the class if one is specified
                if (!m_id.empty())
                    output << "id=\"" << m_id << "\"";

                for (const auto& attr : m_attributes)
                    output << " " << attr.first + "=\"" << attr.second + "\"";

                output << ">";

                if (formatting == StringFormatting::MULTIPLE_LINES)
                    output << "\n";

                // if we have a closing tag, then add children as well
                // as the closing tag to the output
                if (m_closeTag)
                {
                    for (const auto& child : m_children)
                        output << child.ToString(formatting);

                    output << "</" << m_name << ">";

                    if (formatting == StringFormatting::MULTIPLE_LINES)
                        output << "\n";
                }
            }

            return output.str();
        }

        /**
         * Set the name of this element.
         * 
         * Allows for Emmet-like abbreviations such as "div.className#id".
         */
        Node& SetName(const std::string& name)
        {
            const auto periodIndex = name.find('.');
            const auto poundIndex  = name.find('#');

            std::string elementName = name;
            
            // check and see if the name contains either a period or a pound symbol
            // which means that this is an element name that can use emmet abbrivations
            if (periodIndex != std::string::npos || poundIndex != std::string::npos)
            {
                size_t startIndex = std::min(periodIndex, poundIndex);

                elementName = name.substr(0, startIndex);

                _ParseClassesAndIDS(name.substr(startIndex));
            }

            this->m_name = elementName;

            return *this;
        }

        /**
         * Return the element name for this Node.
         */
        std::string const& Name() const
        {
            return m_name;
        }

        /**
         * Get the value of an attribute associated with this element.
         * 
         * Returns blank if there is no attribute of that name.
         */
        std::string GetAttribute(const std::string& name) const
        {
            if (name == "class")
            {
                std::stringstream output;

                for (size_t index = 0; index < m_classes.size(); index++)
                {
                    output << m_classes.at(index);

                    if (index != m_classes.size() - 1)
                        output << " ";
                }

                return output.str();
            }

            if (name == "id")
                return m_id;

            if (m_attributes.count(name) <= 0)
                return "";

            return m_attributes.at(name);
        }

        /**
         * Get a CSS selector like string for this element.
         */
        std::string GetSelector() const
        {
            std::stringstream output;

            output << m_name;

            // convert the class array to a string with dots
            for (auto& element : m_classes)
                output << "." << element;
            
            output << "#" << m_id;

            return output.str();
        }

        /**
         * Set a single attribute for a Node to a value.
         */
        Node& SetAttribute(const std::string& name, const std::string& value)
        {
            if (name == "id")
            {
                m_id = value;
                
                return *this;
            }

            if (name == "class")
            {
                m_classes.clear();

                // create a stringstream from the value for use in splitting
                std::istringstream stream(value);

                std::string temp = "";

                // getline also works with a specific delimeter, allowing us
                // to split by spaces easily
                while (std::getline(
                    stream,
                    temp,
                    ' '
                ))
                    m_classes.push_back(temp);

                return *this;
            }

            m_attributes[name] = value;
            
            return *this;
        }

        /**
         * Set the node type for this Node instance.
         */
        Node& SetType(NodeType type)
        {
            this->m_type = type;
        
            return *this;
        }

        /**
         * Sets the content of a non-element node.
         */
        Node& SetContent(const std::string& text)
        {
            this->m_content = text;
        
            return *this;
        }

        /**
         * Toggle the state of a class based on its name.
         */
        Node& ToggleClass(const std::string& className)
        {
            std::vector<std::string>::iterator find = std::find(
                m_classes.begin(),
                m_classes.end(),
                className
            );
            
            // if the class exists, remove it, otherwise add it
            if (find != m_classes.end())
                m_classes.erase(find);
            else
                m_classes.push_back(className);

            return *this;
        }
        
        Node& AppendChild(Node child)
        {
            m_children.push_back(child);

            return *this;
        }

        /**
         * Append a single text node to the element.
         * 
         * This is the recommended way to set content now
         * as opposed to using the SetContent method.
         */
        Node& AppendText(const std::string& text)
        {
            Node textNode;

            textNode.SetType(NodeType::TEXT)
                    .SetContent(text);

            m_children.push_back(textNode);

            return *this;
        }

        /**
         * Get a single child by its element name.
         */
        Node& GetChildByName(const std::string& name)
        {
            auto it = std::find_if(
                m_children.begin(),
                m_children.end(),
                [&name](Node const& child) { return child.Name() == name; }
            );

            return *it;
        }

        /**
         * Set whether or not this element should have a closing tag or not.
         */
        Node& UseClosingTag(bool close)
        {
            this->m_closeTag = close;

            return *this;
        }

    private:
        /**
         * The type of Node for this instance.
         * 
         * Defaults to an ELEMENT.
         */
        NodeType m_type = NodeType::ELEMENT;

        /**
         * The name of the current node.
         * 
         * Only used in elements, and represents a tag name such as `div`.
         */
        std::string m_name = "";

        /**
         * A list of classes for the current Node.
         * 
         * Only used with an element type node.
         */
        std::vector<std::string> m_classes;

        /**
         * A singular ID for this element.
         */
        std::string m_id = "";

        /**
         * The content for this Node instance.
         * 
         * Only used for text and document type nodes. For text it is the actual
         * content that is sent when converting to a string. For a document type
         * it is the actual type to set, such as `html`.
         */
        std::string m_content = "";

        /**
         * Whether or not to close this current Node if it is an element.
         * 
         * If this is false, no children are to be printed after the node
         * as there would be nowhere to place them.
         * 
         * Defaults to true.
         */
        bool m_closeTag = true;

        /**
         * The child nodes of this Node instance.
         */
        std::vector<Node> m_children;

        /**
         * A map of attribute keys to values for a node.
         * 
         * This is only used for elements.
         */
        std::unordered_map<std::string, std::string> m_attributes;

        /**
         * Replace every occurrence of a string within a string using the specified replace.
         */
        std::string _ReplaceAllOccurrences(std::string& replacer, const std::string& replacable, const std::string& replace) const
        {
            size_t start = 0;

            // while we are not at the end of the string, find the replacable string
            while ((start = replacer.find(replacable, start)) != std::string::npos)
            {
                replacer.replace(start, replacable.length(), replace);
                start += replace.length();
            }
            
            return replacer;
        }

        size_t _CountOccurrences(std::string& finder, const std::string& findable) const 
        {
            size_t occurrences = 0;
            size_t start       = 0;
            
            // while not at the end of the string, count up one occurrence
            while ((start = finder.find(findable, start)) != std::string::npos)
            {
                occurrences++;
                start += findable.length();
            }
            
            return occurrences;
        }

        void _ParseClassesAndIDS(const std::string& input)
        {
            NodeParserState state = NodeParserState::NONE;
            std::string     temp  = "";

            for (unsigned int i = 0; i < input.size(); i++)
            {
                // the current character being iterated
                char current = input[i];

                if (state == NodeParserState::NONE)
                {
                    if (current == '.')
                        state = NodeParserState::CLASS;
                    else if (current == '#')
                        state = NodeParserState::ID;

                    continue;
                }
                else
                {
                    // we are currently set up to parse something, so add it to
                    // the instance with its respective type and set up another
                    // parse for an ID or class
                    if (current == '.' || current == '#')
                    {
                        if (state == NodeParserState::CLASS)
                            m_classes.push_back(temp);
                        else
                            m_id = temp;

                        temp.clear();
                        
                        state = ((current == '.') ? NodeParserState::CLASS : NodeParserState::ID);

                        continue;
                    }
                }

                temp += current;
            }

            // if temp is not empty, then add the final class or ID to the element
            if (!temp.empty())
            {
                if (state == NodeParserState::CLASS)
                    m_classes.push_back(temp);
                else if (state == NodeParserState::ID)
                    m_id = temp;
            }
        }
    };

    class Document {
    public:
        // the default constructor for a document
        Document()
            :
            m_doctype(NodeType::DOCUMENT_TYPE),
            m_html("html")
        {
            // append a head and body tag to the html
            this->m_html.AppendChild(Node("head"));
            this->m_html.AppendChild(Node("body"));
        }

        // add a node to the head element
        void AddNodeToHead(const Node& node)
        {
            this->head().AppendChild(node);
        }

        // add a node to the body element
        void AddNodeToBody(const Node& node)
        {
            this->body().AppendChild(node);
        }

        // gets the current document as a string
        std::string ToString(const StringFormatting& formatting=StringFormatting::SINGLE_LINE) const
        {
            std::stringstream output;

            output << m_doctype.ToString(formatting);

            output << m_html.ToString(formatting);

            return output.str();
        }

        /**
         * Return the root HTML document node.
         */
        Node& html()
        {
            return this->m_html;
        }

        /**
         * Return the head element for the document.
         */
        Node& head()
        {
            return m_html.GetChildByName("head");
        }

        /**
         * Return the body element for the document.
         */
        Node& body()
        {
            return m_html.GetChildByName("body");
        }

    private:
        /**
         * The doctype node for this document.
         * 
         * Defaults to be a HTML5 doctype.
         */
        Node m_doctype;

        /**
         * The root HTML tag for this document.
         */
        Node m_html;
        
    };
}
#endif