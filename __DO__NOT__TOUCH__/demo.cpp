#include <bits/stdc++.h>
#include <thread>
#include <mutex>
#include <random>
#include <atomic>
#define CLUSTER_DEMO_MAIN
// 二維點
struct Point {
    double x;
    double y;
};

// 計算平方距離
inline double sqDist(const Point& a, const Point& b) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return dx * dx + dy * dy;
}

// 並查集（Disjoint‑Set Union）支援多執行緒
class ParallelDSU {
public:
    ParallelDSU(size_t n) : parent(n), rank(n, 0), locks(n) {
        std::iota(parent.begin(), parent.end(), 0);
    }

    int find(int x) {
        while (true) {
            int p = parent[x].load(std::memory_order_acquire);
            if (p == x) return p;
            // path compression (無鎖)
            int gp = parent[p].load(std::memory_order_relaxed);
            if (gp == p) return gp;
            parent[x].compare_exchange_weak(p, gp, std::memory_order_release);
            x = gp;
        }
    }

    void unite(int a, int b) {
        while (true) {
            int ra = find(a);
            int rb = find(b);
            if (ra == rb) return;

            // 鎖定 root，避免同時 union 造成資料競爭
            std::scoped_lock lock(locks[std::min(ra, rb)], locks[std::max(ra, rb)]);
            // roots 可能在鎖前被改變，重新取得
            ra = find(ra);
            rb = find(rb);
            if (ra == rb) return;

            // union by rank
            if (rank[ra] < rank[rb]) {
                parent[ra].store(rb, std::memory_order_release);
            } else if (rank[ra] > rank[rb]) {
                parent[rb].store(ra, std::memory_order_release);
            } else {
                parent[rb].store(ra, std::memory_order_release);
                ++rank[ra];
            }
            return;
        }
    }

private:
    std::vector<std::atomic<int>> parent;
    std::vector<int> rank;
    std::vector<std::mutex> locks;
};

// 多執行緒叢集函式
std::vector<std::vector<int>> cluster(
    const std::vector<Point>& points,
    double radius,
    unsigned thread_cnt = std::thread::hardware_concurrency()
) {
    const size_t n = points.size();
    ParallelDSU dsu(n);
    const double radius_sq = radius * radius;

    // Monte Carlo：隨機順序處理索引
    std::vector<size_t> indices(n);
    std::iota(indices.begin(), indices.end(), 0);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(indices.begin(), indices.end(), gen);

    // 每個 thread 處理分段區間
    std::vector<std::thread> workers;
    auto work = [&](size_t start_idx, size_t end_idx) {
        for (size_t idx = start_idx; idx < end_idx; ++idx) {
            size_t i = indices[idx];
            for (size_t j = i + 1; j < n; ++j) {
                if (sqDist(points[i], points[j]) <= radius_sq) {
                    dsu.unite(static_cast<int>(i), static_cast<int>(j));
                }
            }
        }
    };

    size_t chunk = (n + thread_cnt - 1) / thread_cnt;
    size_t begin = 0;
    for (unsigned t = 0; t < thread_cnt; ++t) {
        size_t end = std::min(begin + chunk, n);
        if (begin >= end) break;
        workers.emplace_back(work, begin, end);
        begin = end;
    }
    for (auto& th : workers) th.join();

    // 收集叢集
    std::unordered_map<int, std::vector<int>> groups;
    for (size_t i = 0; i < n; ++i) {
        int root = dsu.find(static_cast<int>(i));
        groups[root].push_back(static_cast<int>(i));
    }

    std::vector<std::vector<int>> clusters;
    clusters.reserve(groups.size());
    for (auto& [root, vec] : groups) {
        clusters.emplace_back(std::move(vec));
    }
    return clusters;
}

// 範例 main
#ifdef CLUSTER_DEMO_MAIN
int main() {
    std::vector<Point> data = {
        {0,0}, {1,1}, {10,10}, {11,11}, {50,50}, {50.2,50.1}
    };
    double l2norm = 5.0;
    auto clusters = cluster(data, l2norm);

    for (size_t idx = 0; idx < clusters.size(); ++idx) {
        std::cout << "Cluster " << idx << " : ";
        for (int p : clusters[idx]) {
            std::cout << '(' << data[p].x << ',' << data[p].y << ") ";
        }
        std::cout << '\n';
    }
}
#endif