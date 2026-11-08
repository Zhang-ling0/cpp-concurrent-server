#include <functional>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <iostream>
#include <vector>
using TaskFunc = std::function<void()>;
using ReleaseFunc = std::function<void()>;

class TimerTask
{
    public:
    //构造函数
        TimerTask(uint64_t id,uint32_t timeout,const TaskFunc &cb)
        :_id(id),_timeout(timeout),_task_cb(cb)
        {}
    //设计延迟
     void SetRelease(const ReleaseFunc &cb)
     {
        _release = cb;
     }
     uint32_t DelayTime(){return _timeout;}
    //析构函数
        ~TimerTask(){_task_cb();_release();}
    private:
        uint64_t _id;//定时器任务对象ID
        uint32_t _timeout;//定时任务超时时间
        TaskFunc _task_cb;//定时器任务要执行的定时任务
        ReleaseFunc _release;//用于删除TimeTask中保存的定时器对象信息
};
class TimeWheel
{
    public:
        TimeWheel() :_capacity(60),_tick(0),_wheel(_capacity){}
        void TimerAdd(uint64_t id,uint32_t delay,const TaskFunc &cb)
        {//添加定时任务
            PtrTask pt(new TimerTask(id,delay,cb));
            pt->SetRealease(std::bind(RemoveTimer,time,id));
            _timers[id] = WeakTask(pt);
            int pos = (_tick + delay) % _capacity;
            _wheel[pos].push_back(pt);
            _timers[id]=WeakTask(pt);
        }
        void TimerRefresh(uint64_t id,uint32_t timeout)
        {//刷新、延迟定时任务
            auto it = _timers.find(id);
            if(it == _timers.end())
            {
                return;//没有找到定时器任务对象
            }
            PtrTask pt = it->second.lock();//lock获取weak_ptr指向的shared_ptr对象
            int delay = pt->DelayTime();
            int pos = (_tick + delay) % _capacity;
            _wheel[pos].push_back(pt);
        }
        //这个函数应该没秒钟被执行一次，相当于秒针向后走了一步    
    private:
    using PtrTask = std::weak_ptr<TimerTask>;
    using PtrList = std::shared_ptr<std::vector<PtrTask>>;
    std::vector<std::vector<PtrList>> _wheel;//时间轮槽位
    int _tick;//当前的秒针，走到哪里释放哪里，释放哪里就相当于执行哪里的任务
    int _capacity;//表盘最大数量--其实就是最大延迟时间 
    std::unordered_map<uint64_t,PtrTask> _timers;//定时器任务对象ID与定时器任务对象的映射
};
