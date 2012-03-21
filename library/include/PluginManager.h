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

#include "Export.h"
#include "Hooks.h"
#include "ColorText.h"
#include <map>
#include <string>
#include <vector>

#include "RemoteClient.h"

struct DFLibrary;
namespace tthread
{
    class mutex;
    class condition_variable;
}
namespace df
{
    struct viewscreen;
}
namespace DFHack
{
    class Core;
    class PluginManager;
    class virtual_identity;
    class RPCService;

    enum state_change_event
    {
        SC_GAME_LOADED,
        SC_GAME_UNLOADED,
        SC_VIEWSCREEN_CHANGED
    };
    struct DFHACK_EXPORT PluginCommand
    {
        typedef command_result (*command_function)(color_ostream &out, std::vector <std::string> &);
        typedef bool (*command_hotkey_guard)(df::viewscreen *);

        /// create a command with a name, description, function pointer to its code
        /// and saying if it needs an interactive terminal
        /// Most commands shouldn't require an interactive terminal!
        PluginCommand(const char * _name,
                      const char * _description,
                      command_function function_,
                      bool interactive_ = false,
                      const char * usage_ = ""
                     )
            : name(_name), description(_description),
              function(function_), interactive(interactive_),
              guard(NULL), usage(usage_)
        {
        }

        PluginCommand(const char * _name,
                      const char * _description,
                      command_function function_,
                      command_hotkey_guard guard_,
                      const char * usage_ = "")
            : name(_name), description(_description),
              function(function_), interactive(false),
              guard(guard_), usage(usage_)
        {
        }

        bool isHotkeyCommand() const { return guard != NULL; }

        std::string name;
        std::string description;
        command_function function;
        bool interactive;
        command_hotkey_guard guard;
        std::string usage;
    };
    class Plugin
    {
        struct RefLock;
        struct RefAutolock;
        enum plugin_state
        {
            PS_UNLOADED,
            PS_LOADED,
            PS_BROKEN
        };
        friend class PluginManager;
        friend class RPCService;
        Plugin(DFHack::Core* core, const std::string& filepath, const std::string& filename, PluginManager * pm);
        ~Plugin();
        command_result on_update(color_ostream &out);
        command_result on_state_change(color_ostream &out, state_change_event event);
        void detach_connection(RPCService *svc);
    public:
        bool load(color_ostream &out);
        bool unload(color_ostream &out);
        bool reload(color_ostream &out);

        command_result invoke(color_ostream &out, const std::string & command, std::vector <std::string> & parameters);
        bool can_invoke_hotkey(const std::string & command, df::viewscreen *top );
        plugin_state getState () const;

        RPCService *rpc_connect(color_ostream &out);

        const PluginCommand& operator[] (std::size_t index) const
        {
            return commands[index];
        };
        std::size_t size() const
        {
            return commands.size();
        }
        const std::string & getName() const
        {
            return name;
        }
    private:
        RefLock * access;
        std::vector <PluginCommand> commands;
        std::vector <RPCService*> services;
        std::string filename;
        std::string name;
        DFLibrary * plugin_lib;
        PluginManager * parent;
        plugin_state state;
        command_result (*plugin_init)(color_ostream &, std::vector <PluginCommand> &);
        command_result (*plugin_status)(color_ostream &, std::string &);
        command_result (*plugin_shutdown)(color_ostream &);
        command_result (*plugin_onupdate)(color_ostream &);
        command_result (*plugin_onstatechange)(color_ostream &, state_change_event);
        RPCService* (*plugin_rpcconnect)(color_ostream &);
    };
    class DFHACK_EXPORT PluginManager
    {
    // PRIVATE METHODS
        friend class Core;
        friend class Plugin;
        PluginManager(Core * core);
        ~PluginManager();
        void OnUpdate(color_ostream &out);
        void OnStateChange(color_ostream &out, state_change_event event);
        void registerCommands( Plugin * p );
        void unregisterCommands( Plugin * p );
    // PUBLIC METHODS
    public:
        Plugin *getPluginByName (const std::string & name);
        Plugin *getPluginByCommand (const std::string &command);
        command_result InvokeCommand(color_ostream &out, const std::string & command, std::vector <std::string> & parameters);
        bool CanInvokeHotkey(const std::string &command, df::viewscreen *top);
        Plugin* operator[] (std::size_t index)
        {
            if(index >= all_plugins.size())
                return 0;
            return all_plugins[index];
        };
        std::size_t size()
        {
            return all_plugins.size();
        }
    // DATA
    private:
        tthread::mutex * cmdlist_mutex;
        std::map <std::string, Plugin *> belongs;
        std::vector <Plugin *> all_plugins;
        std::string plugin_path;
    };

    namespace Gui
    {
        // Predefined hotkey guards
        DFHACK_EXPORT bool default_hotkey(df::viewscreen *);
        DFHACK_EXPORT bool dwarfmode_hotkey(df::viewscreen *);
        DFHACK_EXPORT bool cursor_hotkey(df::viewscreen *);
    }
};

/// You have to have this in every plugin you write - just once. Ideally on top of the main file.
#define DFHACK_PLUGIN(plugin_name) DFhackDataExport const char * version = DFHACK_VERSION;\
DFhackDataExport const char * name = plugin_name;
