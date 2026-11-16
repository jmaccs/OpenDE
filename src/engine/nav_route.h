// The routes on the walkable area of a map (UCNavMesh, in the editor's package): A* over its polygons, then the funnel
// (the straight line that cuts the corners, as Detour does).
//
// This is the editor's own algorithm (DeusEdix's mapkit/native/NavRoute.h, GPLv3), moved here so the engine can answer
// a route natively on every platform instead of through a 32-bit Windows DLL. The steps are the ones the UnrealScript
// version had, in the same order and in 32-bit floats as UnrealScript computes, so a character takes the same route as
// before. The polygons are read where UCNavMesh keeps them: polygon p has Sides[p] corners (3 to 6), corner k at
// p * MaxSides + k, and Across[p * MaxSides + k] is the polygon across its edge from corner k to k + 1, or -1.
#pragma once

#include <vector>

namespace dx {
    namespace nav {
        constexpr int MaxSides = 6;         // the most corners a polygon has
        constexpr int MaxPolys = 2048;      // the most polygons a walkable area has (UCNavMesh.Sides[2048])
        constexpr int MaxCorners = 64;      // the most corners a route has (UCNavMesh.Corner[64])
        constexpr int MaxSteps = 4096;      // the most polygons the search takes from its open list

        struct Vec {
            float X, Y, Z;
        };

        // A walkable area where UCNavMesh keeps it (the arrays are its own, read in place)
        struct Polygons {
            int Count;
            const unsigned char* Sides;
            const float* CornerX;
            const float* CornerY;
            const float* CornerZ;
            const int* Across;
        };

        class Router {
        public:
            explicit Router(const Polygons& mesh);

            // The route from a to b: its corners into out (at most MaxCorners, b the last); 0 when there is no way on
            // the walkable area (a or b off it, or no polygons between them).
            int Route(const Vec& a, const Vec& b, Vec* out);

        private:
            const Polygons& m;
            int count;
            std::vector<int> sides;             // 0: a polygon left out
            std::vector<Vec> centre, lo, hi;    // the middle of each polygon and its box
            // the search: the cost of the way to a polygon, the estimate of the whole way through it, where it was
            // reached from, whether it was seen and closed; the open polygons in a heap by estimate
            std::vector<float> cost, estimate;
            std::vector<int> cameFrom, heap, heapPos;
            std::vector<char> seen, closed;
            int heapN = 0;
            std::vector<int> way;               // the polygons from the start to the end
            std::vector<Vec> left, right;       // the portals between them: the ends of the edge each one crosses

            float cornerX(int at) const;
            float cornerY(int at) const;
            Vec corner(int at) const;

            // x, y inside polygon i (seen from above; either winding)
            bool inside(int i, float x, float y) const;

            // how far x, y is from polygon i, seen from above (0: inside it)
            float polyDist(int i, float x, float y) const;

            // The polygon under p (a character's feet, or a place up to 140 units over its floor), or -1. Just off the
            // mesh (it keeps 20 units from the walls: a place by a wall), the nearest polygon within 200 units.
            int findPoly(const Vec& p) const;

            float centreDist(int a, int b) const;

            void heapUp(int i);
            void heapPush(int p);
            int heapPop();

            // the polygons from s to t into way, or false when t cannot be reached from s
            bool findWay(int s, int t);

            static float area2(float px, float py, float qx, float qy, float sx, float sy);

            // portal i: the edge shared by way[i - 1] and way[i], its left and right ends as one walks through it
            bool portal(int i);

            // a corner of the route at an end of portal i (atLeft: its left end), 20 units into the portal (off the wall)
            void addCorner(int i, bool atLeft, Vec* out, int& n) const;

            // the corners of the way from a to b (b the last one; the others on the floor)
            int makeCorners(const Vec& a, const Vec& b, Vec* out);
        };
    }  // namespace nav
}  // namespace dx
