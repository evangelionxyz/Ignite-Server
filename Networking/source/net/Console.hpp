#pragma once
#include <cstdint>
#include <string_view>
#include <functional>
#include <iostream>
#include <thread>

#include <format>

namespace ignite::net
{
    class Console
    {
    public:
        using MessageSendCallback = std::function<void(std::string_view)>;

        Console(std::string_view title = "Console");
        ~Console();

        void ClearLog();

        template<typename... Args>
        void AddMessage(const std::string_view format, Args&&... args)
        {
            std::string message_string = std::vformat(format, std::make_format_args(args...));
            m_MessageHistory.push_back(message_string);
            std::cout << message_string << std::endl;
        }

        template<typename... Args>
        void AddItalicMessage(const std::string_view format, Args&&... args)
        {
            std::string message_string = std::vformat(format, std::make_format_args(args...));
            MessageInfo info = message_string;
            info.Italic = true;
            m_MessageHistory.push_back(info);
            std::cout << message_string << std::endl;
        }

        template<typename... Args>
        void AddTaggedMessage(const std::string_view tag, const std::string_view format, Args&&... args)
        {
            std::string message_string = std::vformat(format, std::make_format_args(args...));
            m_MessageHistory.push_back(MessageInfo(std::string(tag), message_string));
            std::cout << '[' << tag << ']' << message_string << std::endl;
        }

        template<typename... Args>
        void AddMessageWithColor(const uint32_t color, const std::string_view format, Args&&... args)
        {
            std::string message_string = std::vformat(format, std::make_format_args(args...));
            m_MessageHistory.push_back(MessageInfo(message_string, color));
            std::cout << message_string;
        }

        template<typename... Args>
        void AddItalicMessageWithColor(const uint32_t color, const std::string_view format, Args&&... args)
        {
            std::string message_string = std::vformat(format, std::make_format_args(args...));
            MessageInfo info = MessageInfo(message_string, color);
            info.Italic = true;
            m_MessageHistory.push_back(info);
            std::cout << message_string << std::endl;
        }

        template<typename... Args>
        void AddTaggedMessageWithColor(const uint32_t color, const std::string_view tag, const std::string &format, Args&&... args)
        {
            std::string message_string = std::vformat(format, std::make_format_args(args...));
            m_MessageHistory.push_back(MessageInfo(std::string(tag), message_string, color));
            std::cout << '[' << tag << ']' << message_string << std::endl;
        }

        void SetMessageSendCallback(const MessageSendCallback &callback);

    private:
        void InputThreadFunc();

        struct MessageInfo
        {
            std::string Tag;
            std::string Message;
            bool Italic = false;
            uint32_t Color = 0xffffffff;

            MessageInfo(const std::string &message, uint32_t color = 0xffffffff)
                : Message(message), Color(color)
            {
            }

            MessageInfo(const std::string &tag, const std::string &message, uint32_t color = 0xffffffff)
                : Tag(tag), Message(message), Color(color)
            {
            }
        };

        std::string m_Title;
        std::vector<MessageInfo> m_MessageHistory;
        std::thread m_InputThread;
        bool m_InputThreadRunning = false;

        MessageSendCallback m_MessageSendCallback;
    };
}