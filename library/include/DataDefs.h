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

#include <string>
#include <sstream>
#include <vector>
#include <map>

#include "Core.h"
#include "BitArray.h"

// Stop some MS stupidity
#ifdef interface
	#undef interface
#endif

/*
 * Definitions of DFHack namespace structs used by generated headers.
 */

namespace DFHack
{
    class virtual_class {};

#ifdef _MSC_VER
    typedef void *virtual_ptr;
#else
    typedef virtual_class *virtual_ptr;
#endif

    class DFHACK_EXPORT virtual_identity {
        static virtual_identity *list;
        static std::map<void*, virtual_identity*> known;
        
        virtual_identity *prev, *next;
        const char *dfhack_name;
        const char *original_name;
        virtual_identity *parent;
        std::vector<virtual_identity*> children;
        
        void *vtable_ptr;
        bool has_children;

    protected:
        virtual_identity(const char *dfhack_name, const char *original_name, virtual_identity *parent);

        static void *get_vtable(virtual_ptr instance_ptr) { return *(void**)instance_ptr; }

    public:
        const char *getName() { return dfhack_name; }
        const char *getOriginalName() { return original_name ? original_name : dfhack_name; }

        virtual_identity *getParent() { return parent; }
        const std::vector<virtual_identity*> &getChildren() { return children; }

    public:
        static virtual_identity *get(virtual_ptr instance_ptr);
        
        bool is_subclass(virtual_identity *subtype);
        bool is_instance(virtual_ptr instance_ptr) {
            if (!instance_ptr) return false;
            if (vtable_ptr) {
                void *vtable = get_vtable(instance_ptr);
                if (vtable == vtable_ptr) return true;
                if (!has_children) return false;
            }
            return is_subclass(get(instance_ptr));
        }

        bool is_direct_instance(virtual_ptr instance_ptr) {
            if (!instance_ptr) return false;
            return vtable_ptr ? (vtable_ptr == get_vtable(instance_ptr)) 
                              : (this == get(instance_ptr));
        }

    public:
        bool can_instantiate() { return (vtable_ptr != NULL); }
        virtual_ptr instantiate() { return can_instantiate() ? do_instantiate() : NULL; }
        static virtual_ptr clone(virtual_ptr obj);

    protected:
        virtual virtual_ptr do_instantiate() = 0;
        virtual void do_copy(virtual_ptr tgt, virtual_ptr src) = 0;
    public:
        static void Init(Core *core);

        // Strictly for use in virtual class constructors
        void adjust_vtable(virtual_ptr obj, virtual_identity *main);
    };

    template<class T>
    inline T *virtual_cast(virtual_ptr ptr) {
        return T::_identity.is_instance(ptr) ? static_cast<T*>(ptr) : NULL;
    }

#define VIRTUAL_CAST_VAR(var,type,input) type *var = virtual_cast<type>(input)

    template<class T>
    inline T *strict_virtual_cast(virtual_ptr ptr) {
        return T::_identity.is_direct_instance(ptr) ? static_cast<T*>(ptr) : NULL;
    }

#define STRICT_VIRTUAL_CAST_VAR(var,type,input) type *var = strict_virtual_cast<type>(input)

    void InitDataDefGlobals(Core *core);

    template<class T>
    T *ifnull(T *a, T *b) { return a ? a : b; }

    template<class T>
    struct enum_list_attr {
        size_t size;
        const T *items;
    };

    // Bitfields
    struct bitfield_item_info {
        const char *name;
        int size;
    };
}

template<class T>
int linear_index(const DFHack::enum_list_attr<T> &lst, T val) {
    for (int i = 0; i < lst.size; i++)
        if (lst.items[i] == val)
            return i;
    return -1;
}

inline int linear_index(const DFHack::enum_list_attr<const char*> &lst, const std::string &val) {
    for (int i = 0; i < lst.size; i++)
        if (lst.items[i] == val)
            return i;
    return -1;
}

/*
 * Definitions of df namespace structs used by generated headers.
 */

namespace df
{
    using DFHack::virtual_ptr;
    using DFHack::virtual_identity;
    using DFHack::virtual_class;
    using DFHack::bitfield_item_info;
    using DFHack::enum_list_attr;
    using DFHack::BitArray;
    using DFHack::DfArray;

    template<class T>
    struct enum_traits {};

    template<class T>
    struct bitfield_traits {};

    template<class T>
    class class_virtual_identity : public virtual_identity {
    public:
        class_virtual_identity(const char *dfhack_name, const char *original_name, virtual_identity *parent)
            : virtual_identity(dfhack_name, original_name, parent) {};

        T *instantiate() { return static_cast<T*>(virtual_identity::instantiate()); }
        T *clone(T* obj) { return static_cast<T*>(virtual_identity::clone(obj)); }

    protected:
        virtual virtual_ptr do_instantiate() { return new T(); }
        virtual void do_copy(virtual_ptr tgt, virtual_ptr src) { *static_cast<T*>(tgt) = *static_cast<T*>(src); }
    };

    template<class EnumType, class IntType = int32_t>
    struct enum_field {
        IntType value;

        enum_field() {}
        enum_field(EnumType ev) : value(IntType(ev)) {}
        template<class T>
        enum_field(enum_field<EnumType,T> ev) : value(IntType(ev.value)) {}

        operator EnumType () { return EnumType(value); }
        enum_field<EnumType,IntType> &operator=(EnumType ev) {
            value = IntType(ev); return *this;
        }
    };

    template<class EnumType, class IntType1, class IntType2>
    inline bool operator== (enum_field<EnumType,IntType1> a, enum_field<EnumType,IntType2> b)
    {
        return EnumType(a) == EnumType(b);
    }

    template<class EnumType, class IntType1, class IntType2>
    inline bool operator!= (enum_field<EnumType,IntType1> a, enum_field<EnumType,IntType2> b)
    {
        return EnumType(a) != EnumType(b);
    }

    namespace enums {}
}

/*
 * Templates for access to enum and bitfield traits.
 */

DFHACK_EXPORT std::string join_strings(const std::string &separator, const std::vector<std::string> &items);

namespace DFHack {
    /*
     * Enum trait tools.
     */

    /**
     * Return the next item in the enum, wrapping to the first one at the end.
     */
    template<class T>
    inline typename df::enum_traits<T>::enum_type next_enum_item(T v) {
        typedef df::enum_traits<T> traits;
        typedef typename traits::base_type base_type;
        base_type iv = base_type(v);
        return (iv < traits::last_item_value) ? T(iv+1) : traits::first_item;
    }

    /**
     * Check if the value is valid for its enum type.
     */
    template<class T>
    inline bool is_valid_enum_item(T v) {
        return df::enum_traits<T>::is_valid(v);
    }

    /**
     * Return the enum item key string pointer, or NULL if none.
     */
    template<class T>
    inline const char *enum_item_raw_key(T val) {
        typedef df::enum_traits<T> traits;
        return traits::is_valid(val) ? traits::key_table[val - traits::first_item_value] : NULL;
    }

    /**
     * Return the enum item key string pointer, or "?" if none.
     */
    template<class T>
    inline const char *enum_item_key_str(T val) {
        return ifnull(enum_item_raw_key(val), "?");
    }

    template<class BaseType>
    std::string format_key(const char *keyname, BaseType val) {
        if (keyname) return std::string(keyname);
        std::stringstream ss; ss << "?" << val << "?"; return ss.str();
    }

    /**
     * Return the enum item key string, or ?123? (using the numeric value) if unknown.
     */
    template<class T>
    inline std::string enum_item_key(T val) {
        typedef typename df::enum_traits<T>::base_type base_type;
        return format_key<base_type>(enum_item_raw_key(val), base_type(val));
    }

    DFHACK_EXPORT int findEnumItem(const std::string &name, int size, const char *const *items);

    /**
     * Find an enum item by key string. Returns success code.
     */
    template<class T>
    inline bool find_enum_item(T *var, const std::string &name) {
        typedef df::enum_traits<T> traits;
        int size = traits::last_item_value-traits::first_item_value+1;
        int idx = findEnumItem(name, size, traits::key_table);
        if (idx < 0) return false;
        *var = T(traits::first_item_value+idx);
        return true;
    }

    /*
     * Bitfield tools.
     */

    DFHACK_EXPORT bool findBitfieldField(unsigned *idx, const std::string &name,
                                         unsigned size, const bitfield_item_info *items);
    DFHACK_EXPORT void setBitfieldField(void *p, unsigned idx, unsigned size, int value);
    DFHACK_EXPORT int getBitfieldField(const void *p, unsigned idx, unsigned size);

    /**
     * Find a bitfield item by key string. Returns success code.
     */
    template<class T>
    inline bool find_bitfield_field(unsigned *idx, const std::string &name, const T* = NULL) {
        typedef df::bitfield_traits<T> traits;
        return findBitfieldField(&idx, name, traits::bit_count, traits::bits);
    }

    /**
     * Find a bitfield item by key and set its value. Returns success code.
     */
    template<class T>
    inline bool set_bitfield_field(T *bitfield, const std::string &name, int value)
    {
        typedef df::bitfield_traits<T> traits;
        unsigned idx;
        if (!findBitfieldField(&idx, name, traits::bit_count, traits::bits)) return false;
        setBitfieldField(&bitfield->whole, idx, traits::bits[idx].size, value);
        return true;
    }

    /**
     * Find a bitfield item by key and retrieve its value. Returns success code.
     */
    template<class T>
    inline bool get_bitfield_field(int *value, const T &bitfield, const std::string &name)
    {
        typedef df::bitfield_traits<T> traits;
        unsigned idx;
        if (!findBitfieldField(&idx, name, traits::bit_count, traits::bits)) return false;
        *value = getBitfieldField(&bitfield.whole, idx, traits::bits[idx].size);
        return true;
    }

    DFHACK_EXPORT void bitfieldToString(std::vector<std::string> *pvec, const void *p,
                                        unsigned size, const bitfield_item_info *items);

    /**
     * Represent bitfield bits as strings in a vector.
     */
    template<class T>
    inline void bitfield_to_string(std::vector<std::string> *pvec, const T &val) {
        typedef df::bitfield_traits<T> traits;
        bitfieldToString(pvec, &val.whole, traits::bit_count, traits::bits);
    }

    /**
     * Represent bitfield bits as a string, using sep as join separator.
     */
    template<class T>
    inline std::string bitfield_to_string(const T &val, const std::string &sep = " ") {
        std::vector<std::string> tmp;
        bitfield_to_string<T>(&tmp, val);
        return join_strings(sep, tmp);
    }

    /*
     * BitArray tools
     */

    /**
     * Find a flag array item by key string. Returns success code.
     */
    template<class T>
    inline bool find_flagarray_field(unsigned *idx, const std::string &name, const BitArray<T>*) {
        T tmp;
        if (!find_enum_item(&tmp, name) || tmp < 0) return false;
        *idx = unsigned(tmp);
        return true;
    }

    /**
     * Find a flag array item by key and set its value. Returns success code.
     */
    template<class T>
    inline bool set_flagarray_field(BitArray<T> *bitfield, const std::string &name, int value)
    {
        T tmp;
        if (!find_enum_item(&tmp, name) || tmp < 0) return false;
        bitfield->set(tmp, value!=0);
        return true;
    }

    /**
     * Find a flag array item by key and retrieve its value. Returns success code.
     */
    template<class T>
    inline bool get_flagarray_field(int *value, const BitArray<T> &bitfield, const std::string &name)
    {
        T tmp;
        if (!find_enum_item(&tmp, name) || tmp < 0) return false;
        *value = (bitfield->is_set(tmp) ? 1 : 0);
        return true;
    }

    DFHACK_EXPORT void flagarrayToString(std::vector<std::string> *pvec, const void *p,
                                         int bytes, int base, int size, const char *const *items);

    /**
     * Represent flag array bits as strings in a vector.
     */
    template<class T>
    inline void flagarray_to_string(std::vector<std::string> *pvec, const BitArray<T> &val) {
        typedef df::enum_traits<T> traits;
        int size = traits::last_item_value-traits::first_item_value+1;
        flagarrayToString(pvec, val.bits, val.size,
                          (int)traits::first_item_value, size, traits::key_table);
    }

    /**
     * Represent flag array bits as a string, using sep as join separator.
     */
    template<class T>
    inline std::string bitfield_to_string(const BitArray<T> &val, const std::string &sep = " ") {
        std::vector<std::string> tmp;
        flagarray_to_string<T>(&tmp, val);
        return join_strings(sep, tmp);
    }
}

#define ENUM_ATTR(enum,attr,val) (df::enum_traits<df::enum>::attrs(val).attr)
#define ENUM_ATTR_STR(enum,attr,val) DFHack::ifnull(ENUM_ATTR(enum,attr,val),"?")
#define ENUM_KEY_STR(enum,val) (DFHack::enum_item_key<df::enum>(val))
#define ENUM_FIRST_ITEM(enum) (df::enum_traits<df::enum>::first_item)
#define ENUM_LAST_ITEM(enum) (df::enum_traits<df::enum>::last_item)

#define ENUM_NEXT_ITEM(enum,val) \
    (DFHack::next_enum_item<df::enum>(val))
#define FOR_ENUM_ITEMS(enum,iter) \
    for(df::enum iter = ENUM_FIRST_ITEM(enum); iter <= ENUM_LAST_ITEM(enum); iter = df::enum(1+int(iter)))

/*
 * Include mandatory generated headers.
 */

// Global object pointers
#include "df/global_objects.h"

// A couple of headers that have to be included at once
#include "df/coord2d.h"
#include "df/coord.h"
