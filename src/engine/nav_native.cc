// UCNavNative.Route: the native function of the package DeusEdixNav (DeusEdix's mapkit/native/DeusEdixNav.cpp was the
// Windows DLL that answered it before). The script calls
//
//     class'DeusEdixNav.UCNavNative'.static.Route(self, a, b)
//
// with self a UCNavMesh: its polygons (NumPolys, Sides, CornerX/Y/Z, Across) are read where the object keeps them, and
// the route's corners are written into its Corner array as vectors. The number of corners is returned; 0 means there
// is no way on the walkable area.
//
// The class itself is a script class of the DeusEdixNav package (DeusEdix ships its source); the engine finds this
// function by the class's name and the function's name (vm::bindNative), so nothing here depends on a DLL.
#include "engine/nav_native.h"

#include "core/object.h"
#include "core/vm.h"
#include "engine/nav_route.h"

#include <cstdio>

namespace dx {
    namespace {
        struct Layout {
            UProperty* numPolys = nullptr;
            UProperty* sides = nullptr;
            UProperty* cornerX = nullptr;
            UProperty* cornerY = nullptr;
            UProperty* cornerZ = nullptr;
            UProperty* across = nullptr;
            UProperty* corner = nullptr;
        };

        bool arrayOf(const UProperty* property, PropType type, int32_t count)
        {
            return property && property->type == type && property->arrayDim >= count;
        }

        // Where UCNavMesh keeps the walkable area, by the names of its properties (the DLL read the same arrays at
        // offsets it found the same way).
        bool layoutOf(UClass* cls, Layout& l)
        {
            if (!cls) {
                return false;
            }
            l.numPolys = cls->findProperty(Name("NumPolys"));
            l.sides = cls->findProperty(Name("Sides"));
            l.cornerX = cls->findProperty(Name("CornerX"));
            l.cornerY = cls->findProperty(Name("CornerY"));
            l.cornerZ = cls->findProperty(Name("CornerZ"));
            l.across = cls->findProperty(Name("Across"));
            l.corner = cls->findProperty(Name("Corner"));
            bool vectors = l.corner && l.corner->type == PropType::Struct && l.corner->structType
                && l.corner->structType->name == Name("Vector") && l.corner->arrayDim >= nav::MaxCorners;
            return arrayOf(l.numPolys, PropType::Int, 1) && arrayOf(l.sides, PropType::Byte, nav::MaxPolys)
                && arrayOf(l.cornerX, PropType::Float, nav::MaxPolys * nav::MaxSides)
                && arrayOf(l.cornerY, PropType::Float, nav::MaxPolys * nav::MaxSides)
                && arrayOf(l.cornerZ, PropType::Float, nav::MaxPolys * nav::MaxSides)
                && arrayOf(l.across, PropType::Int, nav::MaxPolys * nav::MaxSides) && vectors;
        }

        int32_t route(const UObject* mesh, const Vector& from, const Vector& to)
        {
            static bool warned = false;
            if (!mesh || !mesh->data) {
                return 0;
            }
            Layout l;
            if (!layoutOf(mesh->cls, l)) {
                if (!warned) {
                    warned = true;
                    std::fprintf(stderr, "UCNavNative: %s has no walkable area arrays\n",
                        mesh->cls ? mesh->cls->pathName().c_str() : "?");
                }
                return 0;
            }
            nav::Polygons polys = {
                *reinterpret_cast<const int32_t*>(mesh->propertyAddress(Name("NumPolys"))),
                mesh->propertyAddress(Name("Sides")),
                reinterpret_cast<const float*>(mesh->propertyAddress(Name("CornerX"))),
                reinterpret_cast<const float*>(mesh->propertyAddress(Name("CornerY"))),
                reinterpret_cast<const float*>(mesh->propertyAddress(Name("CornerZ"))),
                reinterpret_cast<const int*>(mesh->propertyAddress(Name("Across"))),
            };
            nav::Vec out[nav::MaxCorners];
            int32_t n;
            try {
                nav::Router router(polys);
                n = router.Route(nav::Vec{from.x, from.y, from.z}, nav::Vec{to.x, to.y, to.z}, out);
            } catch (...) {                                     // no memory for the search: no route, the game goes on
                std::fprintf(stderr, "UCNavNative: the search failed\n");
                return 0;
            }
            uint8_t* corners = mesh->propertyAddress(Name("Corner"));
            size_t stride = l.corner->elementSize();
            for (int32_t i = 0; i < n; i++) {
                *reinterpret_cast<Vector*>(corners + static_cast<size_t>(i) * stride) = Vector{out[i].X, out[i].Y, out[i].Z};
            }
            return n;
        }

        void nativeRoute(UObject*, Frame& stack, void* result)
        {
            UObject* mesh = vm::get<UObject*>(stack);
            Vector from = vm::get<Vector>(stack);
            Vector to = vm::get<Vector>(stack);
            vm::finish(stack);
            vm::ret<int32_t>(result, route(mesh, from, to));
        }
    }  // namespace

    void registerNavNatives()
    {
        // The class lives in the script package DeusEdixNav; bindNative keys a native by the class's own name and the
        // function's, so both the bare and the qualified name find the same function.
        vm::registerNative("UCNavNative", "Route", nativeRoute);
        vm::registerNative("DeusEdixNav.UCNavNative", "Route", nativeRoute);
    }
}  // namespace dx
