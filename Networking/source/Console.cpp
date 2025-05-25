#include "Console.hpp"
#include <string>

namespace ignite {

Console::Console(std::string_view title)
{
    m_InputThread = std::thread([this]() { InputThreadFunc(); });
}

Console::~Console()
{
    m_InputThreadRunning = false;
    if (m_InputThread.joinable())
        m_InputThread.join();
}

void Console::ClearLog()
{
    m_MessageHistory.clear();
}

void Console::SetMessageSendCallback(const MessageSendCallback &callback)
{
    m_MessageSendCallback = callback;
}

void Console::InputThreadFunc()
{
    m_InputThreadRunning = true;
    while (m_InputThreadRunning)
    {
        std::string line;
        std::getline(std::cin, line);
        m_MessageSendCallback(line);
    }
}

}
