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
#ifndef CL_MOD_WORLD
#define CL_MOD_WORLD

/**
 * \defgroup grp_world World: all kind of stuff related to the current world state
 * @ingroup grp_modules
 */

#include "Export.h"
#include "Module.h"
#include <ostream>

namespace DFHack
{
    /**
     * \ingroup grp_world
     */
    enum WeatherType
    {
        CLEAR,
        RAINING,
        SNOWING
    };
    typedef unsigned char weather_map [5][5];
    /**
     * \ingroup grp_world
     */
    enum GameMode
    {
        GAMEMODE_DWARF,
        GAMEMODE_ADVENTURE,
        GAMEMODENUM,
        GAMEMODE_NONE
    };
    /**
     * \ingroup grp_world
     */
    enum GameType
    {
        GAMETYPE_DWARF_MAIN,
        GAMETYPE_ADVENTURE_MAIN,
        GAMETYPE_VIEW_LEGENDS,
        GAMETYPE_DWARF_RECLAIM,
        GAMETYPE_DWARF_ARENA,
        GAMETYPE_ADVENTURE_ARENA,
        GAMETYPENUM,
        GAMETYPE_NONE
    };
    /**
     * \ingroup grp_world
     */
    struct t_gamemodes
    {
        GameMode g_mode;
        GameType g_type;
    };
    class DFContextShared;

    class DFHACK_EXPORT PersistentDataItem {
        friend class World;

        int id;
        std::string key_value;

        std::string *str_value;
        int *int_values;
    public:
        static const int NumInts = 7;

        bool isValid() { return id != 0; }

        const std::string &key() { return key_value; }

        std::string &val() { return *str_value; }
        int &ival(int i) { return int_values[i]; }

        PersistentDataItem() : id(0), str_value(0), int_values(0) {}
        PersistentDataItem(int id, const std::string &key, std::string *sv, int *iv)
            : id(id), key_value(key), str_value(sv), int_values(iv) {}
    };

    /**
     * The World module
     * \ingroup grp_modules
     * \ingroup grp_world
     */
    class DFHACK_EXPORT World : public Module
    {
        public:
        weather_map * wmap;
        World();
        ~World();
        bool Start();
        bool Finish();

        ///true if paused, false if not
        bool ReadPauseState();
        ///true if paused, false if not
        void SetPauseState(bool paused);

        uint32_t ReadCurrentTick();
        uint32_t ReadCurrentYear();
        uint32_t ReadCurrentMonth();
        uint32_t ReadCurrentDay();
        uint8_t ReadCurrentWeather();
        void SetCurrentWeather(uint8_t weather);
        bool ReadGameMode(t_gamemodes& rd);
        bool WriteGameMode(const t_gamemodes & wr); // this is very dangerous
        std::string ReadWorldFolder();

        // Store data in fake historical figure names.
        // This ensures that the values are stored in save games.
        PersistentDataItem AddPersistentData(const std::string &key);
        PersistentDataItem GetPersistentData(const std::string &key);
        void GetPersistentData(std::vector<PersistentDataItem> *vec, const std::string &key);
        void DeletePersistentData(const PersistentDataItem &item);

        private:
        struct Private;
        Private *d;
    };
}
#endif

