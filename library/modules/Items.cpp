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


#include "Internal.h"

#include <string>
#include <sstream>
#include <vector>
#include <cstdio>
#include <map>
#include <set>
using namespace std;

#include "Types.h"
#include "VersionInfo.h"
#include "MemAccess.h"
#include "modules/Materials.h"
#include "modules/Items.h"
#include "modules/Units.h"
#include "ModuleFactory.h"
#include "Core.h"
#include "Virtual.h"

using namespace DFHack;

Module* DFHack::createItems()
{
    return new Items();
}

class Items::Private
{
    public:
        Process * owner;
        std::map<int32_t, df_item *> idLookupTable;
        uint32_t refVectorOffset;
        uint32_t idFieldOffset;
        void * itemVectorAddress;

        ClassNameCheck isOwnerRefClass;
        ClassNameCheck isContainerRefClass;
        ClassNameCheck isContainsRefClass;

        // Similar to isOwnerRefClass.  Value is unique to each creature, but
        // different than the creature's id.
        ClassNameCheck isUnitHolderRefClass;

        // One of these is present for each creature contained in a cage.
        // The value is similar to that for isUnitHolderRefClass, different
        // than the creature's ID but unique for each creature.
        ClassNameCheck isCagedUnitRefClass;

        // ID of bulding containing/holding the item.
        ClassNameCheck isBuildingHolderRefClass;

        // Building ID of lever/etc which triggers bridge/etc holding
        // this mechanism.
        ClassNameCheck isTriggeredByRefClass;

        // Building ID of bridge/etc which is triggered by lever/etc holding
        // this mechanism.
        ClassNameCheck isTriggerTargetRefClass;

        // Civilization ID of owner of item, for items not owned by the
        // fortress.
        ClassNameCheck isEntityOwnerRefClass;

        // Item has been offered to the caravan.  The value is the
        // civilization ID of
        ClassNameCheck isOfferedRefClass;

        // Item is in a depot for trade.  Purpose of value is unknown, but is
        // different for each item, even in the same depot at the same time.
        ClassNameCheck isTradingRefClass;

        // Item is flying or falling through the air.  The value seems to
        // be the ID for a "projectile information" object.
        ClassNameCheck isProjectileRefClass;

        std::set<std::string> knownItemRefTypes;
};

Items::Items()
{
    Core & c = Core::getInstance();
    d = new Private;
    d->owner = c.p;

    DFHack::OffsetGroup* itemGroup = c.vinfo->getGroup("Items");
    d->itemVectorAddress = itemGroup->getAddress("items_vector");
    d->idFieldOffset = itemGroup->getOffset("id");
    d->refVectorOffset = itemGroup->getOffset("item_ref_vector");

    d->isOwnerRefClass = ClassNameCheck("general_ref_unit_itemownerst");
    d->isContainerRefClass = ClassNameCheck("general_ref_contained_in_itemst");
    d->isContainsRefClass = ClassNameCheck("general_ref_contains_itemst");
    d->isUnitHolderRefClass = ClassNameCheck("general_ref_unit_holderst");
    d->isCagedUnitRefClass = ClassNameCheck("general_ref_contains_unitst");
    d->isBuildingHolderRefClass
        = ClassNameCheck("general_ref_building_holderst");
    d->isTriggeredByRefClass = ClassNameCheck("general_ref_building_triggerst");
    d->isTriggerTargetRefClass
        = ClassNameCheck("general_ref_building_triggertargetst");
    d->isEntityOwnerRefClass = ClassNameCheck("general_ref_entity_itemownerst");
    d->isOfferedRefClass = ClassNameCheck("general_ref_entity_offeredst");
    d->isTradingRefClass = ClassNameCheck("general_ref_unit_tradebringerst");
    d->isProjectileRefClass = ClassNameCheck("general_ref_projectilest");

    std::vector<std::string> known_names;
    ClassNameCheck::getKnownClassNames(known_names);

    for (size_t i = 0; i < known_names.size(); i++)
    {
        if (known_names[i].find("general_ref_") == 0)
            d->knownItemRefTypes.insert(known_names[i]);
    }
}

bool Items::Start()
{
    d->idLookupTable.clear();
    return true;
}

bool Items::Finish()
{
    return true;
}

bool Items::readItemVector(std::vector<df_item *> &items)
{
    std::vector <df_item *> *p_items = (std::vector <df_item *> *) d->itemVectorAddress;

    d->idLookupTable.clear();
    items.resize(p_items->size());

    for (unsigned i = 0; i < p_items->size(); i++)
    {
        df_item * ptr = p_items->at(i);
        items[i] = ptr;
        d->idLookupTable[ptr->id] = ptr;
    }

    return true;
}

df_item * Items::findItemByID(int32_t id)
{
    if (id < 0)
        return 0;

    if (d->idLookupTable.empty())
    {
        std::vector<df_item *> tmp;
        readItemVector(tmp);
    }

    return d->idLookupTable[id];
}

Items::~Items()
{
    Finish();
    delete d;
}

bool Items::copyItem(df_item * itembase, DFHack::dfh_item &item)
{
    if(!itembase)
        return false;
    df_item * itreal = (df_item *) itembase;
    item.origin = itembase;
    item.x = itreal->x;
    item.y = itreal->y;
    item.z = itreal->z;
    item.id = itreal->id;
    item.age = itreal->age;
    item.flags = itreal->flags;
    item.matdesc.itemType = itreal->getType();
    item.matdesc.subType = itreal->getSubtype();
    item.matdesc.material = itreal->getMaterial();
    item.matdesc.index = itreal->getMaterialIndex();
    item.wear_level = itreal->getWear();
    item.quality = itreal->getQuality();
    item.quantity = itreal->getStackSize();
    return true;
}

int32_t Items::getItemOwnerID(const DFHack::df_item * item)
{
    std::vector<int32_t> vals;
    if (readItemRefs(item, d->isOwnerRefClass, vals))
        return vals[0];
    else
        return -1;
}

int32_t Items::getItemContainerID(const DFHack::df_item * item)
{
    std::vector<int32_t> vals;
    if (readItemRefs(item, d->isContainerRefClass, vals))
        return vals[0];
    else
        return -1;
}

bool Items::getContainedItems(const DFHack::df_item * item, std::vector<int32_t> &items)
{
    return readItemRefs(item, d->isContainsRefClass, items);
}

bool Items::readItemRefs(const df_item * item, const ClassNameCheck &classname, std::vector<int32_t> &values)
{
    const std::vector <t_itemref *> &p_refs = item->itemrefs;
    values.clear();

    for (uint32_t i=0; i<p_refs.size(); i++)
    {
        if (classname(d->owner, p_refs[i]->vptr))
            values.push_back(int32_t(p_refs[i]->value));
    }

    return !values.empty();
}

bool Items::unknownRefs(const df_item * item, std::vector<std::pair<std::string, int32_t> >& refs)
{
    refs.clear();

    const std::vector <t_itemref *> &p_refs = item->itemrefs;

    for (uint32_t i=0; i<p_refs.size(); i++)
    {
        std::string name = p_refs[i]->getClassName();

        if (d->knownItemRefTypes.find(name) == d->knownItemRefTypes.end())
        {
            refs.push_back(pair<string, int32_t>(name, p_refs[i]->value));
        }
    }

    return (refs.size() > 0);
}

bool Items::removeItemOwner(df_item * item, Units *creatures)
{
    std::vector <t_itemref *> &p_refs = item->itemrefs;
    for (uint32_t i=0; i<p_refs.size(); i++)
    {
        if (!d->isOwnerRefClass(d->owner, p_refs[i]->vptr))
            continue;

        int32_t & oid = p_refs[i]->value;
        int32_t ix = creatures->FindIndexById(oid);

        if (ix < 0 || !creatures->RemoveOwnedItemByIdx(ix, item->id))
        {
            cerr << "RemoveOwnedItemIdx: CREATURE " << ix << " ID " << item->id << " FAILED!" << endl;
            return false;
        }
        p_refs.erase(p_refs.begin() + i--);
    }

    item->flags.owned = 0;

    return true;
}

std::string Items::getItemClass(const df_item * item)
{
    const t_virtual * virt = (t_virtual *) item;
    return virt->getClassName();
}

