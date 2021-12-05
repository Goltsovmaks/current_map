#include "test_runner.h"
#include "profile.h"

#include <algorithm>
#include <numeric>

#include <future>
#include <map>
#include <vector>
#include <string>
#include <random>
using namespace std;

template <typename K, typename V>
class ConcurrentMap {
public:
    static_assert(is_integral_v<K>, "ConcurrentMap supports only integer keys");

    struct Access 
    {       
        lock_guard<mutex> g;
        V& ref_to_value;
    };

    explicit ConcurrentMap(size_t bucket_count)
        :bucket_count(bucket_count), maps(bucket_count)
    { 
    }

    Access operator[](const K& key)
    {
       /* for (pair<map<K, V>, mutex>& sinch_mp : maps) //поиск сущ-го ключа
        {
            map<K,V>& m = sinch_mp.first;
            if (m.count(key))
            {
                return { lock_guard(sinch_mp.second), m[key] };
            }
        }*/

        int64_t bucket_index = key;
        bucket_index =  (bucket_index >=0) ? bucket_index % bucket_count : -bucket_index % bucket_count;
        pair<map<K, V>, mutex>& s_m = maps[bucket_index];

        return { lock_guard(s_m.second), s_m.first[key] };

        //Критическая секция
        //создание ключа для какой нибудь мапы
      /*  pair<map<K, V>, mutex>& s_m = maps[index_to_push];
        index_to_push = (index_to_push + 1) % bucket_count;
        return { lock_guard(s_m.second), s_m.first[key] };*/
    }

    map<K, V> BuildOrdinaryMap()
    {       
        map<K, V> result;
        size_t lenght = 0;
        for ( auto& sinch_mp : maps)
        {
            lenght += sinch_mp.first.size();
            for ( auto& [k, v] : sinch_mp.first)
            {
                lock_guard g(sinch_mp.second);
                //Access access{ lock_guard(sinch_mp.second), sinch_mp.first[k] };
                result[k] = v;
            }
        }
        return result;
    }

private:
    vector<pair<map<K,V>,mutex>> maps;
    size_t bucket_count;
    size_t index_to_push = 0;
    mutex m;

};

void RunConcurrentUpdates(
    ConcurrentMap<int, int>& cm, size_t thread_count, int key_count
) {
    auto kernel = [&cm, key_count](int seed) {
        vector<int> updates(key_count);
        iota(begin(updates), end(updates), -key_count / 2);
        shuffle(begin(updates), end(updates), default_random_engine(seed));

        for (int i = 0; i < 2; ++i) {
            for (auto key : updates) 
            {
                cm[key].ref_to_value++;
            }
        }
    };

    vector<future<void>> futures;
    for (size_t i = 0; i < thread_count; ++i) {
        futures.push_back(async(kernel, i));
    }
}

void RunConcurrentUpdates2(
    map<int, int>& cm, size_t thread_count, int key_count
) {
    auto kernel = [&cm, key_count](int seed) {
        vector<int> updates(key_count);
        iota(begin(updates), end(updates), -key_count / 2);
        shuffle(begin(updates), end(updates), default_random_engine(seed));

        for (int i = 0; i < 2; ++i) {
            for (auto key : updates)
            {
                cm[key]++;
            }
        }
    };

    vector<future<void>> futures;
    for (size_t i = 0; i < thread_count; ++i) {
        futures.push_back(async(kernel, i));
    }
}
void TestConcurrentUpdate() {
    const size_t thread_count = 3;
    const size_t key_count = 50'000;

    ConcurrentMap<int, int> cm(thread_count);
    RunConcurrentUpdates(cm, thread_count, key_count);

    const auto result = cm.BuildOrdinaryMap();

    ASSERT_EQUAL(result.size(), key_count);
    for (auto& [k, v] : result) {
        AssertEqual(v, 6, "Key = " + to_string(k));
    }
}

void TestReadAndWrite() {
    ConcurrentMap<size_t, string> cm(5);

    auto updater = [&cm] {
        for (size_t i = 0; i < 50000; ++i) {
            cm[i].ref_to_value += 'a';
        }
    };
    auto reader = [&cm] {
        vector<string> result(50000);
        for (size_t i = 0; i < result.size(); ++i) {
            result[i] = cm[i].ref_to_value;
        }
        return result;
    };

    auto u1 = async(updater);
    auto r1 = async(reader);
    auto u2 = async(updater);
    auto r2 = async(reader);

    u1.get();
    u2.get();

    for (auto f : { &r1, &r2 }) {
        auto result = f->get();
        ASSERT(all_of(result.begin(), result.end(), [](const string& s) {
            return s.empty() || s == "a" || s == "aa";
            }));
    }
}

void TestSpeedup() {
    {
        ConcurrentMap<int, int> single_lock(1);
        LOG_DURATION("Single lock");
        RunConcurrentUpdates(single_lock, 4, 50000);
    }
    {
        ConcurrentMap<int, int> many_locks(100);
        LOG_DURATION("100 locks");
        RunConcurrentUpdates(many_locks, 4, 50000);
    }
}

int main() {
    TestRunner tr;
    RUN_TEST(tr, TestConcurrentUpdate);
    RUN_TEST(tr, TestReadAndWrite);
    RUN_TEST(tr, TestSpeedup);


  /*  vector<int> vi;
    mutex m;
    auto mutex_lambda = [&vi, &m]() {
        lock_guard g(m);
        cout << "Mutex1 is ready" << endl;
        for (size_t i = 0; i < 1'000'000'000; i++)
        {
            if (i % 1'000'000 == 0)
            {
                cout  << "HEHEHEHEH";
            }
            vi.push_back(i);
        }
        cout << "Result = " << vi.size() << "\n Destroy mutex " << endl;
    };

    auto lambda = [&vi]()
    {
        mutex m;
        lock_guard g(m);
        cout << "Second thread" << endl;
        for (size_t i = 0; i < 1'000'000'000; i++)
        {
            if (i % 100'000'000 == 0)
            {
                cout << i << ' ';
            }
                vi.push_back(i);
        }
        cout << "End of lambda "<< endl;
    };

    auto u1 = async(mutex_lambda);
    auto r1 = async(mutex_lambda);

    */
}