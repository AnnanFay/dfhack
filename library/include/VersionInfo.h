/*
https://github.com/peterix/dfhack
Copyright (c) 2009-2011 Petr Mrázek (peterix@gmail.com)

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must
not claim that you wrote the original software. If you use this
software in a product, an acknowledgment in the product documentation
would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
*/


#pragma once

#include "Pragma.h"
#include "Export.h"
#include "Types.h"
#include <map>
#include <sys/types.h>
#include <vector>
#include <algorithm>

namespace DFHack
{
    /*
     * Version Info
     */
    enum OSType
    {
        OS_WINDOWS,
        OS_LINUX,
        OS_APPLE,
        OS_BAD
    };
    struct DFHACK_EXPORT VersionInfo
    {
    private:
        std::vector <std::string> md5_list;
        std::vector <uint32_t> PE_list;
        std::map <std::string, uint32_t> Addresses;
        uint32_t base;
        std::string version;
        OSType OS;
    public:
        VersionInfo()
        {
            base = 0;
            version = "invalid";
            OS = OS_BAD;
        };
        VersionInfo(const VersionInfo& rhs)
        {
            md5_list = rhs.md5_list;
            PE_list = rhs.PE_list;
            Addresses = rhs.Addresses;
            base = rhs.base;
            version = rhs.version;
            OS = rhs.OS;
        };

        uint32_t getBase () const { return base; };
        void setBase (const uint32_t _base) { base = _base; };
        void rebaseTo(const uint32_t new_base)
        {
            int64_t old = base;
            int64_t newx = new_base;
            int64_t rebase = newx - old;
            base = new_base;
            auto iter = Addresses.begin();
            while (iter != Addresses.end())
            {
                uint32_t & ref = (*iter).second;
                ref += rebase;
                iter ++;
            }
        };

        void addMD5 (const std::string & _md5)
        {
            md5_list.push_back(_md5);
        };
        bool hasMD5 (const std::string & _md5) const
        {
            return std::find(md5_list.begin(), md5_list.end(), _md5) != md5_list.end();
        };

        void addPE (uint32_t PE_)
        {
            PE_list.push_back(PE_);
        };
        bool hasPE (uint32_t PE_) const
        {
            return std::find(PE_list.begin(), PE_list.end(), PE_) != PE_list.end();
        };

        void setVersion(const std::string& v)
        {
            version = v;
        };
        std::string getVersion() const { return version; };

        void setAddress (const std::string& key, const uint32_t value)
        {
            Addresses[key] = value;
        };
        template <typename T>
        bool getAddress (const std::string& key, T & value)
        {
            auto i = Addresses.find(key);
            if(i == Addresses.end())
                return false;
            value = (T) (*i).second;
            return true;
        };
        uint32_t getAddress (const std::string& key) const
        {
            auto i = Addresses.find(key);
            if(i == Addresses.end())
                return 0;
            return (*i).second;
        }

        void setOS(const OSType os)
        {
            OS = os;
        };
        OSType getOS() const
        {
            return OS;
        };
    };
}
