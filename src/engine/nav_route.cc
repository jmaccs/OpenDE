// The routes on the walkable area of a map: the editor's own algorithm (DeusEdix's mapkit/native/NavRoute.h), moved
// into the engine. The arithmetic is the UnrealScript version's, step for step, in 32-bit floats: the corners must be
// the same floats, or characters walk differently than before. Nothing here may be "simplified": the order of the
// operations, the ints converted to floats, and the literals are deliberate.
//
// nav_route.h has the record of where the polygons are; this file is that record's code, unchanged.
#include "engine/nav_route.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace dx {
    namespace nav {
        namespace {
            // The arrays are indexed with ints, as the script did; a vector's index is written as size_t here.
            std::size_t ix(int i)
            {
                return static_cast<std::size_t>(i);
            }
        }  // namespace

        Router::Router(const Polygons& mesh) : m(mesh)
        {
            count = std::min(std::max(m.Count, 0), MaxPolys);
            sides.assign(static_cast<std::size_t>(count), 0);
            centre.assign(static_cast<std::size_t>(count), Vec{0, 0, 0});
            lo.assign(static_cast<std::size_t>(count), Vec{0, 0, 0});
            hi.assign(static_cast<std::size_t>(count), Vec{0, 0, 0});
            for (int p = 0; p < count; p++) {
                int n = m.Sides[p];
                if (n < 3 || n > MaxSides) {
                    continue;                       // left out of the search
                }
                sides[ix(p)] = n;
                int b = p * MaxSides;
                Vec c = {0, 0, 0};
                for (int k = 0; k < n; k++) {
                    c.X += m.CornerX[b + k];
                    c.Y += m.CornerY[b + k];
                    c.Z += m.CornerZ[b + k];
                    if (k == 0 || m.CornerX[b + k] < lo[ix(p)].X) lo[ix(p)].X = m.CornerX[b + k];
                    if (k == 0 || m.CornerX[b + k] > hi[ix(p)].X) hi[ix(p)].X = m.CornerX[b + k];
                    if (k == 0 || m.CornerY[b + k] < lo[ix(p)].Y) lo[ix(p)].Y = m.CornerY[b + k];
                    if (k == 0 || m.CornerY[b + k] > hi[ix(p)].Y) hi[ix(p)].Y = m.CornerY[b + k];
                    if (k == 0 || m.CornerZ[b + k] < lo[ix(p)].Z) lo[ix(p)].Z = m.CornerZ[b + k];
                    if (k == 0 || m.CornerZ[b + k] > hi[ix(p)].Z) hi[ix(p)].Z = m.CornerZ[b + k];
                }
                centre[ix(p)] = Vec{c.X / static_cast<float>(n), c.Y / static_cast<float>(n), c.Z / static_cast<float>(n)};
            }
        }

        int Router::Route(const Vec& a, const Vec& b, Vec* out)
        {
            int s = findPoly(a), t = findPoly(b);
            if (s < 0 || t < 0) {
                return 0;
            }
            if (s == t) {
                out[0] = b;
                return 1;
            }
            if (!findWay(s, t)) {
                return 0;
            }
            return makeCorners(a, b, out);
        }

        float Router::cornerX(int at) const
        {
            return m.CornerX[at];
        }

        float Router::cornerY(int at) const
        {
            return m.CornerY[at];
        }

        Vec Router::corner(int at) const
        {
            return Vec{m.CornerX[at], m.CornerY[at], m.CornerZ[at]};
        }

        bool Router::inside(int i, float x, float y) const
        {
            int b = i * MaxSides, n = sides[ix(i)], pos = 0, neg = 0;
            for (int k = 0; k < n; k++) {
                int k2 = k + 1 == n ? 0 : k + 1;
                float c = (cornerX(b + k2) - cornerX(b + k)) * (y - cornerY(b + k))
                        - (cornerY(b + k2) - cornerY(b + k)) * (x - cornerX(b + k));
                if (c > 0.5f) {
                    pos++;
                } else if (c < -0.5f) {
                    neg++;
                }
            }
            return pos == 0 || neg == 0;
        }

        float Router::polyDist(int i, float x, float y) const
        {
            if (inside(i, x, y)) {
                return 0;
            }
            int b = i * MaxSides, n = sides[ix(i)];
            float best = 100000000.0f;
            for (int k = 0; k < n; k++) {
                int k2 = k + 1 == n ? 0 : k + 1;
                float ex = cornerX(b + k2) - cornerX(b + k), ey = cornerY(b + k2) - cornerY(b + k);
                float f = ex * ex + ey * ey;
                if (f > 0) {
                    f = std::min(std::max(((x - cornerX(b + k)) * ex + (y - cornerY(b + k)) * ey) / f, 0.f), 1.f);
                }
                float dx = cornerX(b + k) + ex * f - x, dy = cornerY(b + k) + ey * f - y;
                float d = dx * dx + dy * dy;
                if (d < best) {
                    best = d;
                }
            }
            return std::sqrt(best);
        }

        int Router::findPoly(const Vec& p) const
        {
            int best = -1, nearest = -1;
            float bestD = 1000000.0f, nearD = 200.0f;
            for (int i = 0; i < count; i++) {
                if (sides[ix(i)] == 0 || p.Z < lo[ix(i)].Z - 40 || p.Z > hi[ix(i)].Z + 140) {
                    continue;
                }
                if (p.X < lo[ix(i)].X - nearD || p.X > hi[ix(i)].X + nearD || p.Y < lo[ix(i)].Y - nearD || p.Y > hi[ix(i)].Y + nearD) {
                    continue;
                }
                float d = polyDist(i, p.X, p.Y);
                if (d == 0) {
                    d = std::fabs(p.Z - (centre[ix(i)].Z + 45));
                    if (d < bestD) {
                        bestD = d;
                        best = i;
                    }
                } else if (best < 0 && d < nearD) {
                    nearD = d;
                    nearest = i;
                }
            }
            return best >= 0 ? best : nearest;
        }

        float Router::centreDist(int a, int b) const
        {
            float dx = centre[ix(a)].X - centre[ix(b)].X, dy = centre[ix(a)].Y - centre[ix(b)].Y, dz = centre[ix(a)].Z - centre[ix(b)].Z;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        void Router::heapUp(int i)
        {
            int p = heap[ix(i)];
            while (i > 0) {
                int parent = (i - 1) / 2;
                if (estimate[ix(heap[ix(parent)])] <= estimate[ix(p)]) {
                    break;
                }
                heap[ix(i)] = heap[ix(parent)];
                heapPos[ix(heap[ix(i)])] = i;
                i = parent;
            }
            heap[ix(i)] = p;
            heapPos[ix(p)] = i;
        }

        void Router::heapPush(int p)
        {
            heap[ix(heapN)] = p;
            heapPos[ix(p)] = heapN;
            heapN++;
            heapUp(heapN - 1);
        }

        int Router::heapPop()
        {
            int top = heap[0];
            heapN--;
            if (heapN > 0) {
                int p = heap[ix(heapN)], i = 0;
                while (true) {
                    int c = i * 2 + 1;
                    if (c >= heapN) {
                        break;
                    }
                    if (c + 1 < heapN && estimate[ix(heap[ix(c + 1)])] < estimate[ix(heap[ix(c)])]) {
                        c++;
                    }
                    if (estimate[ix(heap[ix(c)])] >= estimate[ix(p)]) {
                        break;
                    }
                    heap[ix(i)] = heap[ix(c)];
                    heapPos[ix(heap[ix(i)])] = i;
                    i = c;
                }
                heap[ix(i)] = p;
                heapPos[ix(p)] = i;
            }
            return top;
        }

        bool Router::findWay(int s, int t)
        {
            cost.assign(static_cast<std::size_t>(count), 0);
            estimate.assign(static_cast<std::size_t>(count), 0);
            cameFrom.assign(static_cast<std::size_t>(count), -1);
            heap.assign(static_cast<std::size_t>(count), 0);
            heapPos.assign(static_cast<std::size_t>(count), 0);
            seen.assign(static_cast<std::size_t>(count), '\0');
            closed.assign(static_cast<std::size_t>(count), '\0');
            heapN = 0;
            cost[ix(s)] = 0;
            estimate[ix(s)] = centreDist(s, t);
            cameFrom[ix(s)] = -1;
            seen[ix(s)] = 1;
            heapPush(s);
            for (int steps = 0; heapN > 0 && steps < MaxSteps; steps++) {
                int p = heapPop();
                if (p == t) {
                    break;
                }
                closed[ix(p)] = 1;
                int b = p * MaxSides, n = sides[ix(p)];
                for (int k = 0; k < n; k++) {
                    int q = m.Across[b + k];
                    if (q < 0 || q >= count || sides[ix(q)] == 0 || closed[ix(q)]) {
                        continue;
                    }
                    float c = cost[ix(p)] + centreDist(p, q);
                    if (!seen[ix(q)]) {
                        seen[ix(q)] = 1;
                        cost[ix(q)] = c;
                        estimate[ix(q)] = c + centreDist(q, t);
                        cameFrom[ix(q)] = p;
                        heapPush(q);
                    } else if (c < cost[ix(q)]) {
                        cost[ix(q)] = c;
                        estimate[ix(q)] = c + centreDist(q, t);
                        cameFrom[ix(q)] = p;
                        heapUp(heapPos[ix(q)]);
                    }
                }
            }
            if (!seen[ix(t)]) {
                return false;
            }
            way.clear();
            for (int p = t; p >= 0 && static_cast<int>(way.size()) < MaxPolys; p = cameFrom[ix(p)]) {
                way.push_back(p);
            }
            std::reverse(way.begin(), way.end());       // the start first
            return true;
        }

        float Router::area2(float px, float py, float qx, float qy, float sx, float sy)
        {
            return (sx - px) * (qy - py) - (qx - px) * (sy - py);    // twice the signed area of p, q, s seen from above
        }

        bool Router::portal(int i)
        {
            int a = way[ix(i - 1)], c = way[ix(i)], b = a * MaxSides, n = sides[ix(a)], k;
            for (k = 0; k < n; k++) {
                if (m.Across[b + k] == c) {
                    break;
                }
            }
            if (k == n) {
                return false;
            }
            int k2 = k + 1 == n ? 0 : k + 1;
            float dx = centre[ix(c)].X - centre[ix(a)].X, dy = centre[ix(c)].Y - centre[ix(a)].Y;
            float mx = (cornerX(b + k) + cornerX(b + k2)) / 2, my = (cornerY(b + k) + cornerY(b + k2)) / 2;
            if (dx * (cornerY(b + k) - my) - dy * (cornerX(b + k) - mx) > 0) {     // corner k on the left
                left[ix(i)] = corner(b + k);
                right[ix(i)] = corner(b + k2);
            } else {
                left[ix(i)] = corner(b + k2);
                right[ix(i)] = corner(b + k);
            }
            return true;
        }

        void Router::addCorner(int i, bool atLeft, Vec* out, int& n) const
        {
            if (n >= MaxCorners - 1) {
                return;
            }
            Vec c = atLeft ? left[ix(i)] : right[ix(i)], o = atLeft ? right[ix(i)] : left[ix(i)];
            float wx = o.X - c.X, wy = o.Y - c.Y;
            float w = std::sqrt(wx * wx + wy * wy + 0.f);
            if (w > 1) {
                float f = std::min(20.f, w / 2) / w;
                c = Vec{c.X + (o.X - c.X) * f, c.Y + (o.Y - c.Y) * f, c.Z + (o.Z - c.Z) * f};
            }
            out[n++] = c;
        }

        int Router::makeCorners(const Vec& a, const Vec& b, Vec* out)
        {
            int numWay = static_cast<int>(way.size()), last = numWay + 1, n = 0;
            left.assign(static_cast<std::size_t>(numWay + 1), Vec{0, 0, 0});
            right.assign(static_cast<std::size_t>(numWay + 1), Vec{0, 0, 0});
            left[0] = right[0] = a;              // portal 0: the start, 1..numWay-1: the edges, numWay: the end
            left[ix(numWay)] = right[ix(numWay)] = b;
            for (int i = 1; i < numWay; i++) {
                if (!portal(i)) {
                    left[ix(i)] = right[ix(i)] = centre[ix(way[ix(i)])];
                }
            }
            float apexX = a.X, apexY = a.Y, leftX = left[0].X, leftY = left[0].Y, rightX = right[0].X, rightY = right[0].Y;
            int apex = 0, l = 0, r = 0;
            for (int i = 1; i < last && n < MaxCorners - 1; i++) {
                // the right side of the funnel
                if (area2(apexX, apexY, rightX, rightY, right[ix(i)].X, right[ix(i)].Y) <= 0) {
                    if ((apexX == rightX && apexY == rightY) || area2(apexX, apexY, leftX, leftY, right[ix(i)].X, right[ix(i)].Y) > 0) {
                        rightX = right[ix(i)].X;
                        rightY = right[ix(i)].Y;
                        r = i;
                    } else {
                        addCorner(l, true, out, n);         // the right side crossed the left: turn at the left
                        apex = l;
                        apexX = leftX;
                        apexY = leftY;
                        rightX = apexX;
                        rightY = apexY;
                        r = apex;
                        i = apex;
                        continue;
                    }
                }
                // the left side
                if (area2(apexX, apexY, leftX, leftY, left[ix(i)].X, left[ix(i)].Y) >= 0) {
                    if ((apexX == leftX && apexY == leftY) || area2(apexX, apexY, rightX, rightY, left[ix(i)].X, left[ix(i)].Y) < 0) {
                        leftX = left[ix(i)].X;
                        leftY = left[ix(i)].Y;
                        l = i;
                    } else {
                        addCorner(r, false, out, n);
                        apex = r;
                        apexX = rightX;
                        apexY = rightY;
                        leftX = apexX;
                        leftY = apexY;
                        l = apex;
                        i = apex;
                        continue;
                    }
                }
            }
            if (n > 0) {
                float dx = out[n - 1].X - b.X, dy = out[n - 1].Y - b.Y, dz = out[n - 1].Z - b.Z;
                if (std::sqrt(dx * dx + dy * dy + dz * dz) < 2) {
                    n--;                            // the funnel's last turn was at the place itself
                }
            }
            out[n++] = b;
            return n;
        }
    }  // namespace nav
}  // namespace dx
