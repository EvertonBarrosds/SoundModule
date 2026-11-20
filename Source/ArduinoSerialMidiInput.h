
#pragma once

#include <JuceHeader.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <cstdint>
#include <atomic>
#include <functional>
#include <thread>
#include <string>
#include <vector>

class ArduinoSerialMidiInput
{
public:
    using MidiCallback =
        std::function<void(uint8_t, uint8_t, uint8_t)>;

    ArduinoSerialMidiInput() = default;

    ~ArduinoSerialMidiInput()
    {
        close();
    }

    static juce::StringArray getAvailablePorts()
    {
        juce::StringArray ports;

        for (int i = 1; i <= 256; ++i)
        {
            const std::wstring portName =
                std::wstring(L"\\\\.\\COM") + std::to_wstring(i);

            HANDLE testHandle = CreateFileW(
                portName.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0,
                nullptr,
                OPEN_EXISTING,
                0,
                nullptr);

            if (testHandle != INVALID_HANDLE_VALUE)
            {
                ports.add("COM" + juce::String(i));
                CloseHandle(testHandle);
            }
        }

        return ports;
    }

    bool open(
        const juce::String& portName,
        MidiCallback callback)
    {
        close();

        const int portNumber =
            portName.fromFirstOccurrenceOf("COM", false, false).getIntValue();

        if (portNumber <= 0)
            return false;

        const std::wstring deviceName =
            std::wstring(L"\\\\.\\COM") + std::to_wstring(portNumber);

        serialHandle = CreateFileW(
            deviceName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (serialHandle == INVALID_HANDLE_VALUE)
            return false;

        DCB dcb = {};
        dcb.DCBlength = sizeof(DCB);

        if (!GetCommState(serialHandle, &dcb))
        {
            close();
            return false;
        }

        dcb.BaudRate = CBR_115200;
        dcb.ByteSize = 8;
        dcb.StopBits = ONESTOPBIT;
        dcb.Parity = NOPARITY;
        dcb.fBinary = TRUE;
        dcb.fDtrControl = DTR_CONTROL_ENABLE;
        dcb.fRtsControl = RTS_CONTROL_ENABLE;

        if (!SetCommState(serialHandle, &dcb))
        {
            close();
            return false;
        }

        COMMTIMEOUTS timeouts = {};
        timeouts.ReadIntervalTimeout = 1;
        timeouts.ReadTotalTimeoutConstant = 1;
        timeouts.ReadTotalTimeoutMultiplier = 1;

        SetCommTimeouts(serialHandle, &timeouts);
        PurgeComm(serialHandle, PURGE_RXCLEAR | PURGE_TXCLEAR);

        midiCallback = std::move(callback);

        running = true;

        readThread = std::thread(
            [this]()
            {
                readLoop();
            });

        return true;
    }

    void close()
    {
        running = false;

        if (readThread.joinable())
            readThread.join();

        if (serialHandle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(serialHandle);
            serialHandle = INVALID_HANDLE_VALUE;
        }

        midiCallback = nullptr;
    }

    bool isOpen() const
    {
        return serialHandle != INVALID_HANDLE_VALUE;
    }

private:
    HANDLE serialHandle = INVALID_HANDLE_VALUE;

    std::atomic<bool> running{ false };

    std::thread readThread;

    MidiCallback midiCallback;

    void readLoop()
    {
        uint8_t buffer[64];

        uint8_t status = 0;
        uint8_t data1 = 0;
        int dataCount = 0;

        while (running)
        {
            DWORD bytesRead = 0;

            if (!ReadFile(
                serialHandle,
                buffer,
                sizeof(buffer),
                &bytesRead,
                nullptr))
            {
                break;
            }

            for (DWORD i = 0; i < bytesRead; ++i)
            {
                const uint8_t value = buffer[i];

                if (value >= 0xF8)
                    continue;

                if (value & 0x80)
                {
                    if (value >= 0xF0)
                    {
                        status = 0;
                        dataCount = 0;
                        continue;
                    }

                    status = value;
                    dataCount = 0;
                    continue;
                }

                if (status == 0)
                    continue;

                if (dataCount == 0)
                {
                    data1 = value;
                    dataCount = 1;
                }
                else
                {
                    if (midiCallback)
                        midiCallback(status, data1, value);

                    dataCount = 0;
                }
            }
        }
    }
};
