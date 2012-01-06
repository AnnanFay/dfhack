// Quick Dumper : Moves items marked as "dump" to cursor
// FIXME: local item cache in map blocks needs to be fixed after teleporting items
#include <iostream>
#include <iomanip>
#include <sstream>
#include <climits>
#include <vector>
#include <set>
using namespace std;

#include "Core.h"
#include <Console.h>
#include <Export.h>
#include <PluginManager.h>
#include <vector>
#include <string>
#include <algorithm>
#include <modules/Maps.h>

#include <modules/Gui.h>
#include <modules/Items.h>
#include <modules/Materials.h>
#include <modules/MapCache.h>

using namespace DFHack;
using MapExtras::Block;
using MapExtras::MapCache;

DFhackCExport command_result df_autodump (Core * c, vector <string> & parameters);

DFhackCExport const char * plugin_name ( void )
{
    return "autodump";
}

DFhackCExport command_result plugin_init ( Core * c, std::vector <PluginCommand> &commands)
{
    commands.clear();
    commands.push_back(PluginCommand("autodump",
                                     "Teleport items marked for dumping to the cursor.",
                                     df_autodump));
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( Core * c )
{
    return CR_OK;
}

typedef std::map <DFCoord, uint32_t> coordmap;

DFhackCExport command_result df_autodump (Core * c, vector <string> & parameters)
{
    // Command line options
    bool destroy = false;
    if(parameters.size() > 0)
    {
        string & p = parameters[0];
        if(p == "destroy")
            destroy = true;
        else if(p == "?" || p == "help")
        {
            c->con.print(
                "This utility lets you quickly move all items designated to be dumped.\n"
                "Items are instantly moved to the cursor position, the dump flag is unset,\n"
                "and the forbid flag is set, as if it had been dumped normally.\n"
                "Be aware that any active dump item tasks still point at the item.\n\n"
                "Options:\n"
                "destroy  - instead of dumping, destroy the items instantly.\n"
                        );
            return CR_OK;
        }
    }
    c->Suspend();
    DFHack::occupancies40d * occupancies;
    DFHack::VersionInfo *mem = c->vinfo;
    DFHack::Gui * Gui = c->getGui();
    DFHack::Items * Items = c->getItems();
    DFHack::Maps *Maps = c->getMaps();

    vector <df_item*> p_items;
    if(!Items->readItemVector(p_items))
    {
        c->con.printerr("Can't access the item vector.\n");
        c->Resume();
        return CR_FAILURE;
    }
    std::size_t numItems = p_items.size();

    // init the map
    if(!Maps->Start())
    {
        c->con.printerr("Can't initialize map.\n");
        c->Resume();
        return CR_FAILURE;
    }
    MapCache MC (Maps);
    int i = 0;
    int dumped_total = 0;

    int cx, cy, cz;
    DFCoord pos_cursor;
    if(!destroy)
    {
        if (!Gui->getCursorCoords(cx,cy,cz))
        {
            c->con.printerr("Cursor position not found. Please enabled the cursor.\n");
            c->Resume();
            return CR_FAILURE;
        }
        pos_cursor = DFCoord(cx,cy,cz);
        {
            Block * b = MC.BlockAt(pos_cursor / 16);
            if(!b)
            {
                c->con.printerr("Cursor is in an invalid/uninitialized area. Place it over a floor.\n");
                c->Resume();
                return CR_FAILURE;
            }
            uint16_t ttype = MC.tiletypeAt(pos_cursor);
            if(!DFHack::isFloorTerrain(ttype))
            {
                c->con.printerr("Cursor should be placed over a floor.\n");
                c->Resume();
                return CR_FAILURE;
            }
        }
    }
    coordmap counts;
    // proceed with the dumpification operation
    for(std::size_t i=0; i< numItems; i++)
    {
        df_item * itm = p_items[i];
        DFCoord pos_item(itm->x, itm->y, itm->z);

        // keep track how many items are at places. all items.
        coordmap::iterator it = counts.find(pos_item);
        if(it == counts.end())
        {
            std::pair< coordmap::iterator, bool > inserted = counts.insert(std::make_pair(pos_item,1));
            it = inserted.first;
        }
        else
        {
            it->second ++;
        }
        // iterator is valid here, we use it later to decrement the pile counter if the item is moved

        // only dump the stuff marked for dumping and laying on the ground
        if (   !itm->flags.dump
            || !itm->flags.on_ground
            ||  itm->flags.construction
            ||  itm->flags.hidden
            ||  itm->flags.in_building
            ||  itm->flags.in_chest
            ||  itm->flags.in_inventory
            ||  itm->flags.construction
        )
            continue;

        if(!destroy) // move to cursor
        {
            // Change flags to indicate the dump was completed, as if by super-dwarfs
            itm->flags.dump = false;
            itm->flags.forbid = true;

            // Don't move items if they're already at the cursor
            if (pos_cursor == pos_item)
                continue;

            // Do we need to fix block-local item ID vector?
            if(pos_item/16 != pos_cursor/16)
            {
                // yes...
                cerr << "Moving from block to block!" << endl;
                df_block * bl_src = Maps->getBlock(itm->x /16, itm->y/16, itm->z);
                df_block * bl_tgt = Maps->getBlock(cx /16, cy/16, cz);
                if(bl_src)
                {
                    std::remove(bl_src->items.begin(), bl_src->items.end(),itm->id);
                }
                else
                {
                    cerr << "No source block" << endl;
                }
                if(bl_tgt)
                {
                    bl_tgt->items.push_back(itm->id);
                }
                else
                {
                    cerr << "No target block" << endl;
                }
            }

            // Move the item
            itm->x = pos_cursor.x;
            itm->y = pos_cursor.y;
            itm->z = pos_cursor.z;
        }
        else // destroy
        {
            itm->flags.garbage_colect = true;
        }
        // keeping track of item pile sizes ;)
        it->second --;
        dumped_total++;
    }
    if(!destroy) // TODO: do we have to do any of this when destroying items?
    {
        // for each item pile, see if it reached zero. if so, unset item flag on the tile it's on
        coordmap::iterator it = counts.begin();
        coordmap::iterator end = counts.end();
        while(it != end)
        {
            if(it->second == 0)
            {
                t_occupancy occ = MC.occupancyAt(it->first);
                occ.bits.item = false;
                MC.setOccupancyAt(it->first, occ);
            }
            it++;
        }
        // Set "item here" flag on target tile, if we moved any items to the target tile.
        if (dumped_total > 0)
        {
            // assume there is a possibility the cursor points at some weird location with missing block data
            Block * b = MC.BlockAt(pos_cursor / 16);
            if(b)
            {
                t_occupancy occ = MC.occupancyAt(pos_cursor);
                occ.bits.item = 1;
                MC.setOccupancyAt(pos_cursor,occ);
            }
        }
        // write map changes back to DF.
        MC.WriteAll();
        // Is this necessary?  Is "forbid" a dirtyable attribute like "dig" is?
        Maps->WriteDirtyBit(cx/16, cy/16, cz, true);
    }
    c->Resume();
    c->con.print("Done. %d items %s.\n", dumped_total, destroy ? "marked for desctruction" : "quickdumped");
    return CR_OK;
}