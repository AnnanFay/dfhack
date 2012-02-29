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
#ifndef MAPEXTRAS_H
#define MAPEXTRAS_H

#include "modules/Maps.h"
#include "TileTypes.h"
#include <stdint.h>
#include <cstring>
#include "df/map_block.h"
#include "df/block_square_event_mineralst.h"
using namespace DFHack;
using namespace DFHack::Simple;
namespace MapExtras
{
void SquashVeins (DFCoord bcoord, mapblock40d & mb, t_blockmaterials & materials)
{
    memset(materials,-1,sizeof(materials));
    std::vector <df::block_square_event_mineralst *> veins;
    Maps::SortBlockEvents(bcoord.x,bcoord.y,bcoord.z,&veins);
    //iterate through block rows
    for(uint32_t j = 0;j<16;j++)
    {
        //iterate through columns
        for (uint32_t k = 0; k< 16;k++)
        {
            df::tiletype tt = mb.tiletypes[k][j];
            if(DFHack::tileMaterial(tt) == tiletype_material::MINERAL)
            {
                for(int i = (int) veins.size() - 1; i >= 0;i--)
                {
                    if(!!(((1 << k) & veins[i]->tile_bitmask[j]) >> k))
                    {
                        materials[k][j] = veins[i]->inorganic_mat;
                        i = -1;
                    }
                }
            }
        }
    }
}

void SquashRocks ( std::vector< std::vector <uint16_t> > * layerassign, DFHack::mapblock40d & mb, DFHack::t_blockmaterials & materials)
{
    // get the layer materials
    for(uint32_t xx = 0;xx<16;xx++)
    {
        for (uint32_t yy = 0; yy< 16;yy++)
        {
            uint8_t test = mb.designation[xx][yy].bits.biome;
            if( test >= sizeof(mb.biome_indices))
            {
                materials[xx][yy] = -1;
                continue;
            }
            if (mb.biome_indices[test] >= layerassign->size())
            {
                materials[xx][yy] = -1;
                continue;
            }
            materials[xx][yy] =
            layerassign->at(mb.biome_indices[test])[mb.designation[xx][yy].bits.geolayer_index];
        }
    }
}

class Block
{
    public:
    Block(DFHack::DFCoord _bcoord, std::vector< std::vector <uint16_t> > * layerassign = 0)
    {
        dirty_designations = false;
        dirty_tiletypes = false;
        dirty_temperatures = false;
        dirty_blockflags = false;
        dirty_occupancies = false;
        valid = false;
        bcoord = _bcoord;
        if(Maps::ReadBlock40d(bcoord.x,bcoord.y,bcoord.z,&raw))
        {
            Maps::ReadTemperatures(bcoord.x,bcoord.y, bcoord.z,&temp1,&temp2);
            SquashVeins(bcoord,raw,veinmats);
            if(layerassign)
                SquashRocks(layerassign,raw,basemats);
            else
                memset(basemats,-1,sizeof(basemats));
            valid = true;
        }
    }
    int16_t veinMaterialAt(df::coord2d p)
    {
        return veinmats[p.x][p.y];
    }
    int16_t baseMaterialAt(df::coord2d p)
    {
        return basemats[p.x][p.y];
    }
    void ClearMaterialAt(df::coord2d p)
    {
        veinmats[p.x][p.y] = -1;
    }

    df::tiletype TileTypeAt(df::coord2d p)
    {
        return raw.tiletypes[p.x][p.y];
    }
    bool setTiletypeAt(df::coord2d p, df::tiletype tiletype)
    {
        if(!valid) return false;
        dirty_tiletypes = true;
        //printf("setting block %d/%d/%d , %d %d\n",x,y,z, p.x, p.y);
        raw.tiletypes[p.x][p.y] = tiletype;
        return true;
    }

    uint16_t temperature1At(df::coord2d p)
    {
        return temp1[p.x][p.y];
    }
    bool setTemp1At(df::coord2d p, uint16_t temp)
    {
        if(!valid) return false;
        dirty_temperatures = true;
        temp1[p.x][p.y] = temp;
        return true;
    }

    uint16_t temperature2At(df::coord2d p)
    {
        return temp2[p.x][p.y];
    }
    bool setTemp2At(df::coord2d p, uint16_t temp)
    {
        if(!valid) return false;
        dirty_temperatures = true;
        temp2[p.x][p.y] = temp;
        return true;
    }

    df::tile_designation DesignationAt(df::coord2d p)
    {
        return raw.designation[p.x][p.y];
    }
    bool setDesignationAt(df::coord2d p, df::tile_designation des)
    {
        if(!valid) return false;
        dirty_designations = true;
        //printf("setting block %d/%d/%d , %d %d\n",x,y,z, p.x, p.y);
        raw.designation[p.x][p.y] = des;
        if(des.bits.dig)
        {
            dirty_blockflags = true;
            raw.blockflags.bits.designated = true;
        }
        return true;
    }

    df::tile_occupancy OccupancyAt(df::coord2d p)
    {
        return raw.occupancy[p.x][p.y];
    }
    bool setOccupancyAt(df::coord2d p, df::tile_occupancy des)
    {
        if(!valid) return false;
        dirty_occupancies = true;
        raw.occupancy[p.x][p.y] = des;
        return true;
    }

    DFHack::t_blockflags BlockFlags()
    {
        return raw.blockflags;
    }
    bool setBlockFlags(DFHack::t_blockflags des)
    {
        if(!valid) return false;
        dirty_blockflags = true;
        //printf("setting block %d/%d/%d , %d %d\n",x,y,z, p.x, p.y);
        raw.blockflags = des;
        return true;
    }

    bool Write ()
    {
        if(!valid) return false;
        if(dirty_designations)
        {
            Maps::WriteDesignations(bcoord.x,bcoord.y,bcoord.z, &raw.designation);
            Maps::WriteDirtyBit(bcoord.x,bcoord.y,bcoord.z,true);
            dirty_designations = false;
        }
        if(dirty_tiletypes)
        {
            Maps::WriteTileTypes(bcoord.x,bcoord.y,bcoord.z, &raw.tiletypes);
            dirty_tiletypes = false;
        }
        if(dirty_temperatures)
        {
            Maps::WriteTemperatures(bcoord.x,bcoord.y,bcoord.z, &temp1, &temp2);
            dirty_temperatures = false;
        }
        if(dirty_blockflags)
        {
            Maps::WriteBlockFlags(bcoord.x,bcoord.y,bcoord.z,raw.blockflags);
            dirty_blockflags = false;
        }
        if(dirty_occupancies)
        {
            Maps::WriteOccupancy(bcoord.x,bcoord.y,bcoord.z,&raw.occupancy);
            dirty_occupancies = false;
        }
        return true;
    }
    bool valid:1;
    bool dirty_designations:1;
    bool dirty_tiletypes:1;
    bool dirty_temperatures:1;
    bool dirty_blockflags:1;
    bool dirty_occupancies:1;
    DFHack::mapblock40d raw;
    DFHack::DFCoord bcoord;
    DFHack::t_blockmaterials veinmats;
    DFHack::t_blockmaterials basemats;
    DFHack::t_temperatures temp1;
    DFHack::t_temperatures temp2;
};

class MapCache
{
    public:
    MapCache()
    {
        valid = 0;
        Maps::getSize(x_bmax, y_bmax, z_max);
        validgeo = Maps::ReadGeology( layerassign );
        valid = true;
    };
    ~MapCache()
    {
        trash();
    }
    bool isValid ()
    {
        return valid;
    }
    /// get the map block at a *block* coord. Block coord = tile coord / 16
    Block * BlockAt (DFHack::DFCoord blockcoord)
    {
        if(!valid)
            return 0;
        std::map <DFHack::DFCoord, Block*>::iterator iter = blocks.find(blockcoord);
        if(iter != blocks.end())
        {
            return (*iter).second;
        }
        else
        {
            if(blockcoord.x < x_bmax && blockcoord.y < y_bmax && blockcoord.z < z_max)
            {
                Block * nblo;
                if(validgeo)
                    nblo = new Block(blockcoord, &layerassign);
                else
                    nblo = new Block(blockcoord);
                blocks[blockcoord] = nblo;
                return nblo;
            }
            return 0;
        }
    }
    df::tiletype tiletypeAt (DFHack::DFCoord tilecoord)
    {
        Block * b= BlockAt(tilecoord / 16);
        if(b && b->valid)
        {
            return b->TileTypeAt(tilecoord % 16);
        }
        return tiletype::Void;
    }
    bool setTiletypeAt(DFHack::DFCoord tilecoord, df::tiletype tiletype)
    {
        Block * b= BlockAt(tilecoord / 16);
        if(b && b->valid)
        {
            b->setTiletypeAt(tilecoord % 16, tiletype);
            return true;
        }
        return false;
    }

    uint16_t temperature1At (DFHack::DFCoord tilecoord)
    {
        Block * b= BlockAt(tilecoord / 16);
        if(b && b->valid)
        {
            return b->temperature1At(tilecoord % 16);
        }
        return 0;
    }
    bool setTemp1At(DFHack::DFCoord tilecoord, uint16_t temperature)
    {
        Block * b= BlockAt(tilecoord / 16);
        if(b && b->valid)
        {
            b->setTemp1At(tilecoord % 16, temperature);
            return true;
        }
        return false;
    }

    uint16_t temperature2At (DFHack::DFCoord tilecoord)
    {
        Block * b= BlockAt(tilecoord / 16);
        if(b && b->valid)
        {
            return b->temperature2At(tilecoord % 16);
        }
        return 0;
    }
    bool setTemp2At(DFHack::DFCoord tilecoord, uint16_t temperature)
    {
        Block * b= BlockAt(tilecoord / 16);
        if(b && b->valid)
        {
            b->setTemp2At(tilecoord % 16, temperature);
            return true;
        }
        return false;
    }

    int16_t veinMaterialAt (DFHack::DFCoord tilecoord)
    {
        Block * b= BlockAt(tilecoord / 16);
        if(b && b->valid)
        {
            return b->veinMaterialAt(tilecoord % 16);
        }
        return 0;
    }
    int16_t baseMaterialAt (DFHack::DFCoord tilecoord)
    {
        Block * b= BlockAt(tilecoord / 16);
        if(b && b->valid)
        {
            return b->baseMaterialAt(tilecoord % 16);
        }
        return 0;
    }
    bool clearMaterialAt (DFHack::DFCoord tilecoord)
    {
        Block * b= BlockAt(tilecoord / 16);
        if(b && b->valid)
        {
            b->ClearMaterialAt(tilecoord % 16);
        }
        return 0;
    }
    
    df::tile_designation designationAt (DFHack::DFCoord tilecoord)
    {
        Block * b= BlockAt(tilecoord / 16);
        if(b && b->valid)
        {
            return b->DesignationAt(tilecoord % 16);
        }
        df::tile_designation temp;
        temp.whole = 0;
        return temp;
    }
    bool setDesignationAt (DFHack::DFCoord tilecoord, df::tile_designation des)
    {
        Block * b= BlockAt(tilecoord / 16);
        if(b && b->valid)
        {
            b->setDesignationAt(tilecoord % 16, des);
            return true;
        }
        return false;
    }
    
    df::tile_occupancy occupancyAt (DFHack::DFCoord tilecoord)
    {
        Block * b= BlockAt(tilecoord / 16);
        if(b && b->valid)
        {
            return b->OccupancyAt(tilecoord % 16);
        }
        df::tile_occupancy temp;
        temp.whole = 0;
        return temp;
    }
    bool setOccupancyAt (DFHack::DFCoord tilecoord, df::tile_occupancy occ)
    {
        Block * b= BlockAt(tilecoord / 16);
        if(b && b->valid)
        {
            b->setOccupancyAt(tilecoord % 16, occ);
            return true;
        }
        return false;
    }
    
    bool testCoord (DFHack::DFCoord tilecoord)
    {
        Block * b= BlockAt(tilecoord / 16);
        if(b && b->valid)
        {
            return true;
        }
        return false;
    }
    bool WriteAll()
    {
        std::map<DFHack::DFCoord, Block *>::iterator p;
        for(p = blocks.begin(); p != blocks.end(); p++)
        {
            p->second->Write();
        }
        return true;
    }
    void trash()
    {
        std::map<DFHack::DFCoord, Block *>::iterator p;
        for(p = blocks.begin(); p != blocks.end(); p++)
        {
            delete p->second;
        }
        blocks.clear();
    }
    private:
    volatile bool valid;
    volatile bool validgeo;
    uint32_t x_bmax;
    uint32_t y_bmax;
    uint32_t x_tmax;
    uint32_t y_tmax;
    uint32_t z_max;
    std::vector< std::vector <uint16_t> > layerassign;
    std::map<DFHack::DFCoord, Block *> blocks;
};
}
#endif