# 智能指针

`StandardCon::Pointer` 提供两种独占所有权包装器和一组共享/弱引用包装器。它们管理对象生命周期，但不保证被管理对象本身可并发访问。

## 所有权实现

- `SmartPtr<T, Deleter>` 和 `UniquePtr<T, Deleter>` 保存一个原始指针与删除器；复制被禁止，移动会把指针置空并转移删除器。对象在所有者析构或被移动赋值替换时由删除器释放。`UniquePtr` 还提供 `GetPtr()`；两者接口历史不同，均表示独占所有权。
- `SharedPtr<T, Deleter>` 包装 `std::shared_ptr<T>`，复制会共享同一控制块，最后一个共享所有者析构时调用删除器。`GetCount()` 返回共享所有者数，`GetPtr()` 只取原始指针，不转移所有权。
- `WeakPtr<T>` 包装 `std::weak_ptr<T>`，不增加强所有者数。`Lock()` 原子地尝试取得一个 `SharedPtr<T>`；对象已销毁时返回空指针。

接口上，`SmartPtr`、`UniquePtr` 和 `SharedPtr` 都提供 `operator*` 与 `operator->`；`UniquePtr`、`SharedPtr` 另有 `GetPtr()`。只有 `SharedPtr` 提供布尔状态和强引用计数。`WeakPtr::Expired()` 对未绑定或已过期对象返回 `true`；`GetCount()` 在默认未绑定时返回 -1，绑定后返回强所有者数。

```cpp
#include <Wan.hpp>

int main()
{
    StandardCon::Pointer::SharedPtr<int> Owner{new int{42}};
    StandardCon::Pointer::WeakPtr<int> Observer{Owner};

    if (auto Locked = Observer.Lock())
    {
        *Locked = 43;
    }
    return 0;
}
```

不要用 `Expired()` 检查后再尝试直接访问对象：最后一个强所有者可能在检查后立刻销毁对象。应直接调用 `Lock()` 并检查返回值。`WeakPtr` 不提供解引用操作，避免这种检查与使用之间的竞态。

原始指针构造函数会接管传入指针，不能再由其他独占所有者释放。自定义删除器必须与该指针的分配方式匹配。`SharedPtr` 的控制块同步不等于对象成员访问同步；共享对象的并发读写由调用方负责。
