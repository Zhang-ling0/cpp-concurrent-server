#include <iostream>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <cstdint>

using TaskFunc = std::function<void()>;
using ReleaseFunc = std::function<void()>;

class TimerTask {
public:
    TimerTask(uint64_t id, uint32_t timeout, const TaskFunc &cb)
        : _id(id), _timeout(timeout), _task_cb(cb), _canceled(false) {}
    
    // 设置清理回调
    void SetRelease(const ReleaseFunc &cb) { _release = cb; }
    
    uint32_t DelayTime() { return _timeout; }
    
    // 取消任务
    void Cancel() { _canceled = true; }

    ~TimerTask() {
        // 如果没有被取消，才执行定时任务
        if (_canceled == false) _task_cb();
        // 无论取不取消，都要把映射表里的自己删掉
        _release();
    }

private:
    uint64_t _id;
    uint32_t _timeout;
    TaskFunc _task_cb;
    ReleaseFunc _release;
    bool _canceled;
};

class TimeWheel {
public:
    TimeWheel() : _capacity(60), _tick(0), _wheel(_capacity) {}

    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb) {
        PtrTask pt(new TimerTask(id, delay, cb));
        // 绑定释放回调，注意这里用 &TimeWheel::RemoveTimer
        pt->SetRelease(std::bind(&TimeWheel::RemoveTimer, this, id));
        
        // 先存弱指针映射
        _timers[id] = WeakTask(pt);
        // 放入时间轮槽位
        int pos = (_tick + delay) % _capacity;
        _wheel[pos].push_back(pt);
    }

    void TimerRefresh(uint64_t id) {
        auto it = _timers.find(id);
        if (it == _timers.end()) return;
        
        PtrTask pt = it->second.lock();
        if (!pt) return; 
        
        int delay = pt->DelayTime();
        int pos = (_tick + delay) % _capacity;
        _wheel[pos].push_back(pt); // 追加引用计数，实现延迟执行
    }

    void TimerCancel(uint64_t id) {
        auto it = _timers.find(id);
        if (it == _timers.end()) return;
        PtrTask pt = it->second.lock();
        if (pt) pt->Cancel();
    }

    void RunTimerTask() {
        _tick = (_tick + 1) % _capacity;
        _wheel[_tick].clear(); // 清空该槽位，释放shared_ptr，如果计数为0则触发析构
    }

private:
    void RemoveTimer(uint64_t id) {
        _timers.erase(id);
    }

    using PtrTask = std::shared_ptr<TimerTask>;
    using WeakTask = std::weak_ptr<TimerTask>;

    int _tick;
    int _capacity;
    std::vector<std::vector<PtrTask>> _wheel;
    std::unordered_map<uint64_t, WeakTask> _timers;
};